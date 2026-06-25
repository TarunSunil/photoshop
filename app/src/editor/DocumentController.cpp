#include "editor/DocumentController.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QVariantMap>
#include <QtConcurrent>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#ifdef HAVE_OPENCV
#  include <opencv2/core.hpp>
#  include <opencv2/imgproc.hpp>
#endif

// ── Edge-refinement helper ────────────────────────────────────────────────────
// Runs only when HAVE_OPENCV is defined. Detects edges in the source image with
// Canny, then snaps the mask boundary to those edges — cheap approximation of
// Photoshop's "Refine Edge" that uses only core OpenCV without ximgproc.
#ifdef HAVE_OPENCV
static QImage refineMaskEdgesOcv(const QImage& sourceImage, const QImage& mask)
{
    const int maxDim = 1200;
    QImage src = (sourceImage.width() > maxDim || sourceImage.height() > maxDim)
        ? sourceImage.scaled(maxDim, maxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        : sourceImage;

    QImage gray8 = src.convertToFormat(QImage::Format_Grayscale8);
    cv::Mat grayMat(gray8.height(), gray8.width(), CV_8UC1,
                    const_cast<uchar*>(gray8.constBits()),
                    static_cast<size_t>(gray8.bytesPerLine()));

    cv::Mat edges;
    cv::Canny(grayMat, edges, 20, 80);
    cv::Mat dilatedEdges;
    cv::dilate(edges, dilatedEdges, cv::Mat(), cv::Point(-1,-1), 2);

    // Float alpha mask at processing resolution
    QImage scaledMask = mask.scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_ARGB32);
    cv::Mat maskAlpha(scaledMask.height(), scaledMask.width(), CV_32F);
    for (int y = 0; y < scaledMask.height(); ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(scaledMask.constScanLine(y));
        float* frow = maskAlpha.ptr<float>(y);
        for (int x = 0; x < scaledMask.width(); ++x)
            frow[x] = qAlpha(row[x]) / 255.0f;
    }

    // Smooth the mask so transitions are soft away from edges
    cv::Mat blurredMask;
    cv::GaussianBlur(maskAlpha, blurredMask, cv::Size(0,0), 3.0);

    // At detected edge pixels: snap to binary. Away from edges: keep smooth.
    cv::Mat refinedMask = blurredMask.clone();
    for (int y = 0; y < dilatedEdges.rows; ++y) {
        const uchar* edgeRow = dilatedEdges.ptr<uchar>(y);
        float*       refRow  = refinedMask.ptr<float>(y);
        const float* blurRow = blurredMask.ptr<float>(y);
        for (int x = 0; x < dilatedEdges.cols; ++x)
            if (edgeRow[x] > 0)
                refRow[x] = blurRow[x] > 0.5f ? 1.0f : 0.0f;
    }

    // Convert float mask → ARGB32 QImage
    QImage resultSmall(refinedMask.cols, refinedMask.rows, QImage::Format_ARGB32);
    for (int y = 0; y < refinedMask.rows; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(resultSmall.scanLine(y));
        const float* frow = refinedMask.ptr<float>(y);
        for (int x = 0; x < refinedMask.cols; ++x) {
            const uchar a = static_cast<uchar>(std::clamp(frow[x] * 255.0f, 0.0f, 255.0f));
            row[x] = qRgba(255, 255, 255, a);
        }
    }
    return resultSmall.scaled(mask.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
#endif

// ─────────────────────────────────────────────────────────────────────────────

DocumentController::DocumentController(QObject* parent)
    : QObject(parent)
    , m_cancelFlag(std::make_shared<std::atomic<bool>>(false))
    , m_hqCancelFlag(std::make_shared<std::atomic<bool>>(false))
{
    // LQ preview debounce — 100 ms after last change
    m_previewDebounce = new QTimer(this);
    m_previewDebounce->setSingleShot(true);
    m_previewDebounce->setInterval(100);
    connect(m_previewDebounce, &QTimer::timeout, this, &DocumentController::rebuildPreview);

    // HQ preview — fires 1.5 s after LQ is shown and no further changes arrive
    m_hqTimer = new QTimer(this);
    m_hqTimer->setSingleShot(true);
    m_hqTimer->setInterval(1500);
    connect(m_hqTimer, &QTimer::timeout, this, &DocumentController::buildHqPreview);

    // Mask-save timer — kept for the setActiveTool flush path; no longer started
    // from paintMaskStroke (the hot path is now fire-and-forget; commit happens on release)
    m_maskSaveTimer = new QTimer(this);
    m_maskSaveTimer->setSingleShot(true);
    m_maskSaveTimer->setInterval(50);
    connect(m_maskSaveTimer, &QTimer::timeout, this, &DocumentController::flushMaskSave);

    // Any document change: cancel HQ, restart LQ debounce
    connect(&m_document, &lumen::DocumentModel::changed, this, [this]() {
        emit documentChanged();
        emit adjustmentsChanged();
        emit layersChanged();
        m_hqTimer->stop();
        *m_hqCancelFlag = true;
        m_previewDebounce->start();
    });
    connect(&m_document, &lumen::DocumentModel::historyChanged,
            this, &DocumentController::historyChanged);
    connect(&m_aiRuntime, &lumen::AiRuntime::busyChanged,
            this, [this](bool busy){ setAiBusy(busy); });

    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setInterval(2 * 60 * 1000);
    connect(m_autosaveTimer, &QTimer::timeout, this, &DocumentController::autoSave);
    m_autosaveTimer->start();

    checkRecovery();
}

bool DocumentController::hasDocument() const { return m_document.hasDocument(); }
QString DocumentController::sourceName() const
{
    return m_document.hasDocument()
        ? QFileInfo(m_document.sourcePath()).fileName()
        : "No image loaded";
}
QString DocumentController::imageUrl() const
{
    if (!m_document.hasDocument()) return {};
    if (m_showOriginal || m_previewPath.isEmpty())
        return QUrl::fromLocalFile(m_document.sourcePath()).toString();
    return QUrl::fromLocalFile(m_previewPath).toString();
}
bool DocumentController::canUndo()      const { return m_document.canUndo(); }
bool DocumentController::canRedo()      const { return m_document.canRedo(); }
bool DocumentController::showOriginal() const { return m_showOriginal; }
bool DocumentController::cropActive()   const { return m_cropActive; }
void DocumentController::setShowOriginal(bool v)
{
    if (m_showOriginal == v) return;
    m_showOriginal = v;
    emit previewChanged();
}
double DocumentController::brightness()    const { return m_document.scalarAdjustment(lumen::AdjustmentType::Brightness); }
double DocumentController::exposure()      const { return m_document.scalarAdjustment(lumen::AdjustmentType::Exposure); }
double DocumentController::contrast()      const { return m_document.scalarAdjustment(lumen::AdjustmentType::Contrast); }
double DocumentController::saturation()    const { return m_document.scalarAdjustment(lumen::AdjustmentType::Saturation); }
double DocumentController::highlights()    const { return m_document.scalarAdjustment(lumen::AdjustmentType::Highlights); }
double DocumentController::shadows()       const { return m_document.scalarAdjustment(lumen::AdjustmentType::Shadows); }
double DocumentController::whites()        const { return m_document.scalarAdjustment(lumen::AdjustmentType::Whites); }
double DocumentController::blacks()        const { return m_document.scalarAdjustment(lumen::AdjustmentType::Blacks); }
double DocumentController::vibrance()      const { return m_document.scalarAdjustment(lumen::AdjustmentType::Vibrance); }
double DocumentController::temperature()   const { return m_document.scalarAdjustment(lumen::AdjustmentType::Temperature); }
double DocumentController::tint()          const { return m_document.scalarAdjustment(lumen::AdjustmentType::Tint); }
double DocumentController::noiseReduction()const { return m_document.scalarAdjustment(lumen::AdjustmentType::NoiseReduction); }
double DocumentController::sharpening()    const { return m_document.scalarAdjustment(lumen::AdjustmentType::Sharpening); }
void DocumentController::setBrightness(double v)   { setAdjustment(lumen::AdjustmentType::Brightness, v); }
void DocumentController::setExposure(double v)     { setAdjustment(lumen::AdjustmentType::Exposure, v); }
void DocumentController::setContrast(double v)     { setAdjustment(lumen::AdjustmentType::Contrast, v); }
void DocumentController::setSaturation(double v)   { setAdjustment(lumen::AdjustmentType::Saturation, v); }
void DocumentController::setHighlights(double v)   { setAdjustment(lumen::AdjustmentType::Highlights, v); }
void DocumentController::setShadows(double v)      { setAdjustment(lumen::AdjustmentType::Shadows, v); }
void DocumentController::setWhites(double v)       { setAdjustment(lumen::AdjustmentType::Whites, v); }
void DocumentController::setBlacks(double v)       { setAdjustment(lumen::AdjustmentType::Blacks, v); }
void DocumentController::setVibrance(double v)     { setAdjustment(lumen::AdjustmentType::Vibrance, v); }
void DocumentController::setTemperature(double v)  { setAdjustment(lumen::AdjustmentType::Temperature, v); }
void DocumentController::setTint(double v)         { setAdjustment(lumen::AdjustmentType::Tint, v); }
void DocumentController::setNoiseReduction(double v){ setAdjustment(lumen::AdjustmentType::NoiseReduction, v); }
void DocumentController::setSharpening(double v)   { setAdjustment(lumen::AdjustmentType::Sharpening, v); }
int  DocumentController::activeTool()  const { return m_activeTool; }
bool DocumentController::hasMask()     const { return !m_document.activeMask().isNull(); }
QString DocumentController::maskUrl()  const
{
    return m_maskTempPath.isEmpty() ? QString()
        : QUrl::fromLocalFile(m_maskTempPath).toString();
}
int  DocumentController::sourceWidth()  const { return m_document.sourceSize().width(); }
int  DocumentController::sourceHeight() const { return m_document.sourceSize().height(); }
void DocumentController::setActiveTool(int tool)
{
    if (m_activeTool == tool) return;
    if (m_maskSaveTimer && m_maskSaveTimer->isActive()) {
        m_maskSaveTimer->stop();
        flushMaskSave();
    }
    m_activeTool = tool;
    m_cropActive = (tool == 5);
    emit cropActiveChanged();
    emit activeToolChanged();
}
bool    DocumentController::aiBusy()   const { return m_aiBusy; }
QString DocumentController::aiStatus() const { return m_aiStatus; }
void DocumentController::setAiBusy(bool busy)
{
    if (m_aiBusy == busy) return;
    m_aiBusy = busy;
    emit aiBusyChanged();
}
void DocumentController::setAiStatus(const QString& s)
{
    m_aiStatus = s;
    emit aiStatusChanged();
}
QVariantList DocumentController::layerModel() const
{
    QVariantList list;
    for (const lumen::Layer& l : m_document.layers()) {
        QVariantMap m;
        m["id"]      = l.id;
        m["name"]    = l.name;
        m["opacity"] = l.opacity;
        m["visible"] = l.visible;
        m["order"]   = l.order;
        list.prepend(m);
    }
    return list;
}
bool DocumentController::hasPendingRecovery() const { return m_hasPendingRecovery; }
QString DocumentController::autosavePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + "/lumenforge-autosave.lfproj";
}
void DocumentController::checkRecovery()
{
    m_hasPendingRecovery = QFileInfo::exists(autosavePath());
    if (m_hasPendingRecovery) emit recoveryChanged();
}
void DocumentController::autoSave()
{
    if (!m_document.hasDocument()) return;
    m_projectStore.saveProject(m_document, autosavePath());
}
void DocumentController::recoverProject()
{
    if (m_projectStore.loadProject(m_document, autosavePath()))
        QFile::remove(autosavePath());
    m_hasPendingRecovery = false;
    emit recoveryChanged();
}
void DocumentController::discardRecovery()
{
    QFile::remove(autosavePath());
    m_hasPendingRecovery = false;
    emit recoveryChanged();
}

bool DocumentController::openImage(const QUrl& url)
{
    if (!m_document.openSourceImage(localPath(url))) {
        emit operationFailed("Could not open image.");
        return false;
    }
    m_brushEngine = std::make_unique<lumen::BrushEngine>(m_document.sourceSize());
    m_maskTempPath.clear();
    emit maskChanged();
    QFile::remove(autosavePath());
    m_hasPendingRecovery = false;
    emit recoveryChanged();
    return true;
}
bool DocumentController::saveProject(const QUrl& url)
{
    if (!m_projectStore.saveProject(m_document, localPath(url))) {
        emit operationFailed("Could not save project.");
        return false;
    }
    return true;
}
bool DocumentController::loadProject(const QUrl& url)
{
    if (!m_projectStore.loadProject(m_document, localPath(url))) {
        emit operationFailed("Could not load project.");
        return false;
    }
    m_brushEngine = std::make_unique<lumen::BrushEngine>(m_document.sourceSize());
    return true;
}
bool DocumentController::exportImage(const QUrl& url)
{
    if (!m_exportService.exportImage(m_document, localPath(url))) {
        emit operationFailed("Could not export image.");
        return false;
    }
    return true;
}
void DocumentController::resetAdjustments()
{
    for (auto type : {
        lumen::AdjustmentType::Brightness,
        lumen::AdjustmentType::Exposure,     lumen::AdjustmentType::Contrast,
        lumen::AdjustmentType::Saturation,   lumen::AdjustmentType::Highlights,
        lumen::AdjustmentType::Shadows,      lumen::AdjustmentType::Whites,
        lumen::AdjustmentType::Blacks,       lumen::AdjustmentType::Vibrance,
        lumen::AdjustmentType::Temperature,  lumen::AdjustmentType::Tint,
        lumen::AdjustmentType::NoiseReduction, lumen::AdjustmentType::Sharpening,
    }) setAdjustment(type, 0.0);
}
void DocumentController::rotateClockwise()        { m_document.rotateClockwise(); }
void DocumentController::rotateCounterClockwise() { m_document.rotateCounterClockwise(); }
void DocumentController::flipHorizontal()         { m_document.flipHorizontal(); }
void DocumentController::flipVertical()           { m_document.flipVertical(); }
void DocumentController::undo()                   { m_document.undo(); }
void DocumentController::redo()                   { m_document.redo(); }

// ── Brush mask ────────────────────────────────────────────────────────────────
// PERFORMANCE FIX: paintMaskStroke no longer starts any timer or touches the
// file system. The BrushEngine accumulates strokes in memory only.  QML draws
// them locally via drawLocalStroke() for instant visual feedback.  A single
// file-I/O + model update happens in commitMaskPaint() on mouseRelease.
void DocumentController::paintMaskStroke(double x, double y, double radius, bool erase)
{
    if (!m_document.hasDocument() || !m_brushEngine) return;
    m_brushEngine->paintStroke(QPointF(x, y), radius, 0.85, erase);
    // No timer, no I/O — accumulate silently. QML handles live visual feedback.
}

void DocumentController::commitMaskPaint()
{
    // Stop any pending timer (safety net for setActiveTool path)
    if (m_maskSaveTimer && m_maskSaveTimer->isActive()) m_maskSaveTimer->stop();
    if (!m_brushEngine) return;
    // Single flush: model update + PNG save + signal
    m_document.setActiveMask(m_brushEngine->mask());
    saveMaskToTemp();
    emit maskChanged();
}

void DocumentController::flushMaskSave()
{
    if (!m_brushEngine) return;
    m_document.setActiveMask(m_brushEngine->mask());
    saveMaskToTemp();
    emit maskChanged();
}

void DocumentController::clearMask()
{
    if (m_maskSaveTimer->isActive()) m_maskSaveTimer->stop();
    if (m_brushEngine) m_brushEngine->clear();
    m_document.setActiveMask(QImage());
    m_maskTempPath.clear();
    emit maskChanged();
}

void DocumentController::saveMaskToTemp()
{
    const QImage& mask = m_document.activeMask();
    if (mask.isNull()) return;
    QImage displayMask = (mask.width() > 1600)
        ? mask.scaled(1600, 1200, Qt::KeepAspectRatio, Qt::FastTransformation)
        : mask;
    const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QString("/lumenforge-mask-%1.png").arg(++m_maskVersion);
    if (displayMask.save(path, "PNG")) {
        if (!m_maskTempPath.isEmpty()) QFile::remove(m_maskTempPath);
        m_maskTempPath = path;
    }
}

void DocumentController::applyGradientMask(double x1, double y1, double x2, double y2)
{
    if (!m_document.hasDocument()) return;
    const QSize sz = m_document.sourceSize();
    QImage mask(sz, QImage::Format_ARGB32);
    QPainter p(&mask);
    QLinearGradient grad(x1, y1, x2, y2);
    grad.setColorAt(0.0, QColor(255,255,255,255));
    grad.setColorAt(1.0, QColor(255,255,255,0));
    p.fillRect(QRectF(0,0,sz.width(),sz.height()), grad);
    p.end();
    if (!m_brushEngine) m_brushEngine = std::make_unique<lumen::BrushEngine>(sz);
    m_brushEngine->mask() = mask;
    m_document.setActiveMask(mask);
    saveMaskToTemp();
    emit maskChanged();
}

void DocumentController::applyRadialMask(double cx, double cy, double radius)
{
    if (!m_document.hasDocument()) return;
    const QSize sz = m_document.sourceSize();
    QImage mask(sz, QImage::Format_ARGB32);
    mask.fill(Qt::transparent);
    QPainter p(&mask);
    QRadialGradient grad(cx, cy, radius);
    grad.setColorAt(0.0,  QColor(255,255,255,255));
    grad.setColorAt(0.65, QColor(255,255,255,180));
    grad.setColorAt(1.0,  QColor(255,255,255,0));
    p.fillRect(QRectF(0,0,sz.width(),sz.height()), grad);
    p.end();
    if (!m_brushEngine) m_brushEngine = std::make_unique<lumen::BrushEngine>(sz);
    m_brushEngine->mask() = mask;
    m_document.setActiveMask(mask);
    saveMaskToTemp();
    emit maskChanged();
}

void DocumentController::applyCrop(int x, int y, int w, int h)
{
    if (!m_document.hasDocument()) return;
    const QSize sz = m_document.sourceSize();
    const QRect rect(
        qBound(0, x, sz.width()),
        qBound(0, y, sz.height()),
        qBound(1, w, sz.width()  - qBound(0, x, sz.width())),
        qBound(1, h, sz.height() - qBound(0, y, sz.height()))
    );
    if (rect.isEmpty()) return;
    m_document.replaceSourceImage(m_document.sourceImage().copy(rect));
    m_brushEngine = std::make_unique<lumen::BrushEngine>(m_document.sourceSize());
    m_maskTempPath.clear();
    setActiveTool(0);   // sets m_cropActive=false, emits cropActiveChanged
    emit maskChanged();
}

void DocumentController::refineEdges()
{
#ifdef HAVE_OPENCV
    if (!m_document.hasDocument() || m_document.activeMask().isNull() || m_aiBusy) return;
    setAiBusy(true);
    setAiStatus("Refining edges…");
    const QImage src  = m_document.sourceImage();
    const QImage mask = m_document.activeMask();
    auto* w = new QFutureWatcher<QImage>(this);
    connect(w, &QFutureWatcher<QImage>::finished, this, [this, w]() {
        const QImage refined = w->result();
        if (!refined.isNull()) {
            if (m_brushEngine) m_brushEngine->mask() = refined;
            m_document.setActiveMask(refined);
            saveMaskToTemp();
            emit maskChanged();
            setAiStatus("Done");
        } else {
            setAiStatus("Edge refinement failed");
        }
        setAiBusy(false);
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([src, mask]() -> QImage {
        return refineMaskEdgesOcv(src, mask);
    }));
#else
    setAiStatus("Edge refinement requires OpenCV (not built)");
#endif
}

void DocumentController::requestAiMask(double x, double y)
{
    if (!m_document.hasDocument() || m_aiBusy) return;
    setAiStatus("Loading model…");
    const QImage src = m_document.sourceImage();
    const QPointF pt(x, y);
    m_aiRuntime.predictMask(src, pt, [this](QImage result, QString error) {
        if (!result.isNull()) {
            if (!m_brushEngine)
                m_brushEngine = std::make_unique<lumen::BrushEngine>(m_document.sourceSize());
            m_brushEngine->mask() = result;
            m_document.setActiveMask(result);
            saveMaskToTemp();
            emit maskChanged();
            setAiStatus("Done");
        } else {
            const QString message = error.isEmpty() ? "AI mask prediction failed." : error;
            setAiStatus(message);
            emit operationFailed(message);
        }
    });
}

void DocumentController::applyInpaint()
{
    if (!m_document.hasDocument() || m_document.activeMask().isNull() || m_aiBusy) return;
    if (!m_inpaintEngine) {
        try { m_inpaintEngine = std::make_unique<lumen::InpaintEngine>(); }
        catch (const std::exception& e) {
            const QString msg = QString("Failed to initialise inpaint engine: %1")
                                    .arg(QString::fromUtf8(e.what()));
            setAiStatus(msg); emit operationFailed(msg); return;
        }
    }
    setAiBusy(true); setAiStatus("Loading model…");
    const QImage src  = m_document.sourceImage();
    const QImage mask = m_document.activeMask();
    auto* w = new QFutureWatcher<QImage>(this);
    connect(w, &QFutureWatcher<QImage>::finished, this, [this, w]() {
        m_document.replaceSourceImage(w->result());
        if (m_brushEngine) m_brushEngine->resize(m_document.sourceSize());
        const QString error = m_inpaintEngine ? m_inpaintEngine->lastError() : QString();
        if (!error.isEmpty()) { setAiStatus(error); emit operationFailed(error); }
        else setAiStatus("Done");
        setAiBusy(false); rebuildPreview(); w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([this, src, mask]() mutable -> QImage {
        QMetaObject::invokeMethod(this, [this]{ setAiStatus("Running inference…"); });
        return m_inpaintEngine->inpaint(src, mask);
    }));
}

void DocumentController::applyUpscale()
{
    if (!m_document.hasDocument() || m_aiBusy) return;
    if (!m_upscaleEngine) {
        try { m_upscaleEngine = std::make_unique<lumen::UpscaleEngine>(); }
        catch (const std::exception& e) {
            const QString msg = QString("Failed to initialise upscale engine: %1")
                                    .arg(QString::fromUtf8(e.what()));
            setAiStatus(msg); emit operationFailed(msg); return;
        }
    }
    setAiBusy(true); setAiStatus("Loading model…");
    const QImage src = m_document.sourceImage();
    auto* w = new QFutureWatcher<QImage>(this);
    connect(w, &QFutureWatcher<QImage>::finished, this, [this, w]() {
        m_document.replaceSourceImage(w->result());
        if (m_brushEngine) m_brushEngine->resize(m_document.sourceSize());
        const QString error = m_upscaleEngine ? m_upscaleEngine->lastError() : QString();
        setAiStatus(error.isEmpty() ? "Done" : QString("Done (basic resize - %1)").arg(error));
        setAiBusy(false); rebuildPreview(); w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([this, src]() mutable -> QImage {
        QMetaObject::invokeMethod(this, [this]{ setAiStatus("Running inference…"); });
        return m_upscaleEngine->upscale(src);
    }));
}

void DocumentController::addImageLayer(const QUrl& url) { m_document.addImageLayer(localPath(url)); }
void DocumentController::deleteLayer(const QString& id)  { m_document.deleteLayer(id); }
void DocumentController::setLayerOpacity(const QString& id, double o) { m_document.setLayerOpacity(id, o); }
void DocumentController::setLayerVisible(const QString& id, bool v)   { m_document.setLayerVisible(id, v); }
void DocumentController::exportBatch(const QUrl& dir, const QStringList& fmts)
{ m_exportService.exportBatch(m_document, dir.toLocalFile(), fmts); }

// ── LQ preview (1400×1050, ~100-200 ms after last change) ────────────────────
void DocumentController::rebuildPreview()
{
    if (!m_document.hasDocument()) {
        ++m_previewRequestId;
        m_previewPath.clear();
        emit previewChanged();
        return;
    }
    if (m_previewWatcher && m_previewWatcher->isRunning()) {
        m_previewPending = true;
        *m_cancelFlag = true;
        return;
    }
    *m_cancelFlag = false;
    const QImage src  = m_document.sourceImage();
    const QImage mask = m_document.activeMask();
    const QVector<lumen::Adjustment> adjs = m_document.adjustments();
    const int requestId = ++m_previewRequestId;
    auto cancelled = m_cancelFlag;
    auto* watcher = new QFutureWatcher<QImage>(this);
    m_previewWatcher = watcher;
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, requestId]() {
        const QImage preview = watcher->result();
        if (!preview.isNull() && requestId == m_previewRequestId) {
            const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                + QString("/lumenforge-preview-%1.jpg").arg(++m_previewVersion);
            if (preview.save(path, "JPEG", 88)) {
                m_previewPath = path;
                emit previewChanged();
                // LQ is shown — arm the HQ idle timer
                *m_hqCancelFlag = false;
                m_hqTimer->start();
            }
        }
        watcher->deleteLater();
        m_previewWatcher = nullptr;
        if (m_previewPending) { m_previewPending = false; rebuildPreview(); }
    });
    watcher->setFuture(QtConcurrent::run(
        [pipeline = m_renderPipeline, src, adjs, mask, cancelled]() {
            return pipeline.renderPreviewFromData(src, adjs, QSize(1400, 1050), mask, cancelled);
        }));
}

// ── HQ preview (up to 3840×2160, triggered 1.5 s after user stops editing) ──
// Images ≤ 4K render at native resolution. Larger images are scaled to fit
// inside 4K. The render is cancelled the moment any adjustment changes.
void DocumentController::buildHqPreview()
{
    if (!m_document.hasDocument()) return;
    if (m_previewWatcher && m_previewWatcher->isRunning()) {
        m_hqTimer->start(500); // LQ still running — retry in 500 ms
        return;
    }
    if (m_hqWatcher && m_hqWatcher->isRunning()) return; // already in progress

    *m_hqCancelFlag = false;
    const QImage src  = m_document.sourceImage();
    const QImage mask = m_document.activeMask();
    const auto   adjs = m_document.adjustments();
    const int    reqId = m_previewRequestId;
    auto cancelled = m_hqCancelFlag;

    auto* watcher = new QFutureWatcher<QImage>(this);
    m_hqWatcher = watcher;
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, reqId]() {
        const QImage preview = watcher->result();
        if (!preview.isNull() && reqId == m_previewRequestId) {
            const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                + QString("/lumenforge-preview-%1.jpg").arg(++m_previewVersion);
            if (preview.save(path, "JPEG", 95)) { // higher quality for the idle HQ path
                if (!m_previewPath.isEmpty()) QFile::remove(m_previewPath);
                m_previewPath = path;
                emit previewChanged();
            }
        }
        watcher->deleteLater();
        m_hqWatcher = nullptr;
    });
    watcher->setFuture(QtConcurrent::run(
        [pipeline = m_renderPipeline, src, adjs, mask, cancelled]() {
            // renderPreviewFromData only downscales when the source exceeds the limit.
            // Passing QSize(3840,2160) means native resolution for any image ≤ 4K.
            return pipeline.renderPreviewFromData(src, adjs, QSize(3840, 2160), mask, cancelled);
        }));
}

void DocumentController::setAdjustment(lumen::AdjustmentType type, double value)
{
    if (qFuzzyCompare(m_document.scalarAdjustment(type), value)) return;
    m_document.setScalarAdjustment(type, value);
}
QString DocumentController::localPath(const QUrl& url) const
{ return url.isLocalFile() ? url.toLocalFile() : url.toString(); }