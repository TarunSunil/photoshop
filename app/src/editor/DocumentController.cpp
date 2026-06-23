#include "editor/DocumentController.hpp"
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QVariantMap>
#include <QtConcurrent>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
DocumentController::DocumentController(QObject* parent)
    : QObject(parent)
    , m_cancelFlag(std::make_shared<std::atomic<bool>>(false))
{
    // Preview debounce: wait 100ms after last adjustment change
    m_previewDebounce = new QTimer(this);
    m_previewDebounce->setSingleShot(true);
    m_previewDebounce->setInterval(100);
    connect(m_previewDebounce, &QTimer::timeout, this, &DocumentController::rebuildPreview);
    // Mask save debounce: batch expensive PNG saves to every 50ms max.
    // Without this, every mouse-move during brush painting triggers a
    // full PNG encode + file write (potentially 100-500ms for large images)
    // which stacks up and causes the 10-second freeze.
    m_maskSaveTimer = new QTimer(this);
    m_maskSaveTimer->setSingleShot(true);
    m_maskSaveTimer->setInterval(50);
    connect(m_maskSaveTimer, &QTimer::timeout, this, &DocumentController::flushMaskSave);
    connect(&m_document, &lumen::DocumentModel::changed, this, [this]() {
        emit documentChanged();
        emit adjustmentsChanged();
        emit layersChanged();
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
    if (!m_document.hasDocument()) return QString();
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
double DocumentController::exposure()       const { return m_document.scalarAdjustment(lumen::AdjustmentType::Exposure); }
double DocumentController::contrast()       const { return m_document.scalarAdjustment(lumen::AdjustmentType::Contrast); }
double DocumentController::saturation()     const { return m_document.scalarAdjustment(lumen::AdjustmentType::Saturation); }
double DocumentController::highlights()     const { return m_document.scalarAdjustment(lumen::AdjustmentType::Highlights); }
double DocumentController::shadows()        const { return m_document.scalarAdjustment(lumen::AdjustmentType::Shadows); }
double DocumentController::whites()         const { return m_document.scalarAdjustment(lumen::AdjustmentType::Whites); }
double DocumentController::blacks()         const { return m_document.scalarAdjustment(lumen::AdjustmentType::Blacks); }
double DocumentController::vibrance()       const { return m_document.scalarAdjustment(lumen::AdjustmentType::Vibrance); }
double DocumentController::temperature()    const { return m_document.scalarAdjustment(lumen::AdjustmentType::Temperature); }
double DocumentController::tint()           const { return m_document.scalarAdjustment(lumen::AdjustmentType::Tint); }
double DocumentController::noiseReduction() const { return m_document.scalarAdjustment(lumen::AdjustmentType::NoiseReduction); }
double DocumentController::sharpening()     const { return m_document.scalarAdjustment(lumen::AdjustmentType::Sharpening); }
void DocumentController::setExposure(double v)      { setAdjustment(lumen::AdjustmentType::Exposure, v); }
void DocumentController::setContrast(double v)      { setAdjustment(lumen::AdjustmentType::Contrast, v); }
void DocumentController::setSaturation(double v)    { setAdjustment(lumen::AdjustmentType::Saturation, v); }
void DocumentController::setHighlights(double v)    { setAdjustment(lumen::AdjustmentType::Highlights, v); }
void DocumentController::setShadows(double v)       { setAdjustment(lumen::AdjustmentType::Shadows, v); }
void DocumentController::setWhites(double v)        { setAdjustment(lumen::AdjustmentType::Whites, v); }
void DocumentController::setBlacks(double v)        { setAdjustment(lumen::AdjustmentType::Blacks, v); }
void DocumentController::setVibrance(double v)      { setAdjustment(lumen::AdjustmentType::Vibrance, v); }
void DocumentController::setTemperature(double v)   { setAdjustment(lumen::AdjustmentType::Temperature, v); }
void DocumentController::setTint(double v)          { setAdjustment(lumen::AdjustmentType::Tint, v); }
void DocumentController::setNoiseReduction(double v){ setAdjustment(lumen::AdjustmentType::NoiseReduction, v); }
void DocumentController::setSharpening(double v)    { setAdjustment(lumen::AdjustmentType::Sharpening, v); }
int  DocumentController::activeTool()  const { return m_activeTool; }
bool DocumentController::hasMask()     const { return !m_document.activeMask().isNull(); }
QString DocumentController::maskUrl()  const
{
    return m_maskTempPath.isEmpty() ? QString()
        : QUrl::fromLocalFile(m_maskTempPath).toString();
}
int DocumentController::sourceWidth()  const { return m_document.sourceSize().width(); }
int DocumentController::sourceHeight() const { return m_document.sourceSize().height(); }
void DocumentController::setActiveTool(int tool)
{
    if (m_activeTool == tool) return;
    // Flush any pending mask before switching tools
    if (m_maskSaveTimer && m_maskSaveTimer->isActive()) {
        m_maskSaveTimer->stop();
        flushMaskSave();
    }
    m_activeTool = tool;
    // Entering crop mode: set flag so QML shows the crop overlay
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
// ── Brush mask ───────────────────────────────────────────────────────────────
void DocumentController::paintMaskStroke(double x, double y, double radius, bool erase)
{
    if (!m_document.hasDocument() || !m_brushEngine) return;
    // Paint in memory — this is fast (pure pixel ops, no I/O)
    m_brushEngine->paintStroke(QPointF(x, y), radius, 0.85, erase);
    // Do NOT call setActiveMask or saveMaskToTemp here — both are expensive
    // (model update emits 'changed' which triggers preview rebuild;
    // saveMaskToTemp encodes a full PNG file). Instead, batch them via timer.
    m_maskSaveTimer->start();
}
void DocumentController::commitMaskPaint()
{
    // Called on mouseRelease to flush any pending save immediately
    if (m_maskSaveTimer->isActive()) {
        m_maskSaveTimer->stop();
        flushMaskSave();
    }
}
void DocumentController::flushMaskSave()
{
    if (!m_brushEngine) return;
    // Update document model (triggers preview rebuild via debounce)
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
    // Save mask at display resolution (max 1600x1200) — the full-res mask
    // stays in memory; the temp file is only for QML display overlay.
    // This makes PNG encode 4-10x faster for large source images.
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
// ── Gradient mask ─────────────────────────────────────────────────────────────
void DocumentController::applyGradientMask(double x1, double y1, double x2, double y2)
{
    if (!m_document.hasDocument()) return;
    const QSize sz = m_document.sourceSize();
    QImage mask(sz, QImage::Format_ARGB32);
    QPainter p(&mask);
    QLinearGradient grad(x1, y1, x2, y2);
    grad.setColorAt(0.0, QColor(255, 255, 255, 255));
    grad.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(QRectF(0, 0, sz.width(), sz.height()), grad);
    p.end();
    if (!m_brushEngine) m_brushEngine = std::make_unique<lumen::BrushEngine>(sz);
    m_brushEngine->mask() = mask;
    m_document.setActiveMask(mask);
    saveMaskToTemp();
    emit maskChanged();
}
// ── Radial mask ───────────────────────────────────────────────────────────────
void DocumentController::applyRadialMask(double cx, double cy, double radius)
{
    if (!m_document.hasDocument()) return;
    const QSize sz = m_document.sourceSize();
    QImage mask(sz, QImage::Format_ARGB32);
    mask.fill(Qt::transparent);
    QPainter p(&mask);
    QRadialGradient grad(cx, cy, radius);
    grad.setColorAt(0.0, QColor(255, 255, 255, 255));
    grad.setColorAt(0.65, QColor(255, 255, 255, 180));
    grad.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(QRectF(0, 0, sz.width(), sz.height()), grad);
    p.end();
    if (!m_brushEngine) m_brushEngine = std::make_unique<lumen::BrushEngine>(sz);
    m_brushEngine->mask() = mask;
    m_document.setActiveMask(mask);
    saveMaskToTemp();
    emit maskChanged();
}
// ── Crop ─────────────────────────────────────────────────────────────────────
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
    m_cropActive = false;
    emit cropActiveChanged();
    emit maskChanged();
    setActiveTool(0);
}
// ── AI ────────────────────────────────────────────────────────────────────────
void DocumentController::requestAiMask(double x, double y)
{
    if (!m_document.hasDocument() || m_aiBusy) return;
    setAiStatus("Loading model...");
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
    // Create the engine on first use — this is when Ort::Env is first
    // initialised and onnxruntime.dll is first loaded (via delay-load).
    if (!m_inpaintEngine) {
        try {
            m_inpaintEngine = std::make_unique<lumen::InpaintEngine>();
        } catch (const std::exception& e) {
            const QString msg = QString("Failed to initialise inpaint engine: %1")
                                    .arg(QString::fromUtf8(e.what()));
            setAiStatus(msg);
            emit operationFailed(msg);
            return;
        }
    }
    setAiBusy(true);
    setAiStatus("Loading model...");
    const QImage src  = m_document.sourceImage();
    const QImage mask = m_document.activeMask();
    auto* w = new QFutureWatcher<QImage>(this);
    connect(w, &QFutureWatcher<QImage>::finished, this, [this, w]() {
        m_document.replaceSourceImage(w->result());
        if (m_brushEngine) m_brushEngine->resize(m_document.sourceSize());
        const QString error = m_inpaintEngine ? m_inpaintEngine->lastError() : QString();
        if (!error.isEmpty()) { setAiStatus(error); emit operationFailed(error); }
        else setAiStatus("Done");
        setAiBusy(false);
        rebuildPreview();
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([this, src, mask]() mutable -> QImage {
        QMetaObject::invokeMethod(this, [this]{ setAiStatus("Running inference..."); });
        return m_inpaintEngine->inpaint(src, mask);
    }));
}
void DocumentController::applyUpscale()
{
    if (!m_document.hasDocument() || m_aiBusy) return;
    // Create the engine on first use — same lazy-init rationale as applyInpaint.
    if (!m_upscaleEngine) {
        try {
            m_upscaleEngine = std::make_unique<lumen::UpscaleEngine>();
        } catch (const std::exception& e) {
            const QString msg = QString("Failed to initialise upscale engine: %1")
                                    .arg(QString::fromUtf8(e.what()));
            setAiStatus(msg);
            emit operationFailed(msg);
            return;
        }
    }
    setAiBusy(true);
    setAiStatus("Loading model...");
    const QImage src = m_document.sourceImage();
    auto* w = new QFutureWatcher<QImage>(this);
    connect(w, &QFutureWatcher<QImage>::finished, this, [this, w]() {
        m_document.replaceSourceImage(w->result());
        if (m_brushEngine) m_brushEngine->resize(m_document.sourceSize());
        const QString error = m_upscaleEngine ? m_upscaleEngine->lastError() : QString();
        setAiStatus(error.isEmpty() ? "Done" : QString("Done (basic resize - %1)").arg(error));
        setAiBusy(false);
        rebuildPreview();
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([this, src]() mutable -> QImage {
        QMetaObject::invokeMethod(this, [this]{ setAiStatus("Running inference..."); });
        return m_upscaleEngine->upscale(src);
    }));
}
// ── Layers ───────────────────────────────────────────────────────────────────
void DocumentController::addImageLayer(const QUrl& url)
{ m_document.addImageLayer(localPath(url)); }
void DocumentController::deleteLayer(const QString& id)
{ m_document.deleteLayer(id); }
void DocumentController::setLayerOpacity(const QString& id, double opacity)
{ m_document.setLayerOpacity(id, opacity); }
void DocumentController::setLayerVisible(const QString& id, bool visible)
{ m_document.setLayerVisible(id, visible); }
void DocumentController::exportBatch(const QUrl& directory, const QStringList& formats)
{ m_exportService.exportBatch(m_document, directory.toLocalFile(), formats); }
// ── Preview ──────────────────────────────────────────────────────────────────
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
            // Save preview as JPEG (much faster than PNG for large images)
            // quality=88 is indistinguishable from lossless at preview sizes
            if (preview.save(path, "JPEG", 88)) { m_previewPath = path; emit previewChanged(); }
        }
        watcher->deleteLater();
        m_previewWatcher = nullptr;
        if (m_previewPending) { m_previewPending = false; rebuildPreview(); }
    });
    // 1400x1050: good balance of quality and speed
    watcher->setFuture(QtConcurrent::run(
        [pipeline = m_renderPipeline, src, adjs, mask, cancelled]() {
            return pipeline.renderPreviewFromData(src, adjs, QSize(1400, 1050), mask, cancelled);
        }));
}
void DocumentController::setAdjustment(lumen::AdjustmentType type, double value)
{
    if (qFuzzyCompare(m_document.scalarAdjustment(type), value)) return;
    m_document.setScalarAdjustment(type, value);
}
QString DocumentController::localPath(const QUrl& url) const
{ return url.isLocalFile() ? url.toLocalFile() : url.toString(); }