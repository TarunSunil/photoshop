#include "editor/DocumentController.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QVariantMap>
#include <QtConcurrent>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#ifdef HAVE_OPENCV
#  include <opencv2/core.hpp>
#  include <opencv2/imgproc.hpp>
#endif

// ---------------------------------------------------------------------------
// Brush-engine resolution cap (issue 4 fix)
// ---------------------------------------------------------------------------
// Running QPainter on a full-resolution (potentially 20MP+) QImage per
// mouse-move event causes multi-hundred millisecond stalls, especially after
// refineEdges() writes a full-res mask back into the brush engine. Capping
// the brush engine to 2000px on the longest side keeps stroke painting
// interactive. Masks are upsampled to source resolution before being stored
// in DocumentModel so exports remain full quality.
static QSize brushEngineSize(const QSize& sourceSize)
{
    constexpr int MAX_DIM = 2000;
    if (sourceSize.width() <= MAX_DIM && sourceSize.height() <= MAX_DIM)
        return sourceSize;
    return sourceSize.scaled(MAX_DIM, MAX_DIM, Qt::KeepAspectRatio);
}

static QImage upsampleMaskToSource(const QImage& mask, const QSize& sourceSize)
{
    if (mask.isNull() || mask.size() == sourceSize) return mask;
    return mask.scaled(sourceSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

static QImage downsampleMaskToBrush(const QImage& mask, const QSize& beSize)
{
    if (mask.isNull() || mask.size() == beSize) return mask;
    return mask.scaled(beSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

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
    cv::Mat edges, dilatedEdges;
    cv::Canny(grayMat, edges, 20, 80);
    cv::dilate(edges, dilatedEdges, cv::Mat(), cv::Point(-1,-1), 2);
    QImage scaledMask = mask.scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_ARGB32);
    cv::Mat maskAlpha(scaledMask.height(), scaledMask.width(), CV_32F);
    for (int y = 0; y < scaledMask.height(); ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(scaledMask.constScanLine(y));
        float* frow = maskAlpha.ptr<float>(y);
        for (int x = 0; x < scaledMask.width(); ++x)
            frow[x] = qAlpha(row[x]) / 255.0f;
    }
    cv::Mat blurredMask, refinedMask;
    cv::GaussianBlur(maskAlpha, blurredMask, cv::Size(0,0), 3.0);
    refinedMask = blurredMask.clone();
    for (int y = 0; y < dilatedEdges.rows; ++y) {
        const uchar* edgeRow = dilatedEdges.ptr<uchar>(y);
        float* refRow  = refinedMask.ptr<float>(y);
        const float* blurRow = blurredMask.ptr<float>(y);
        for (int x = 0; x < dilatedEdges.cols; ++x)
            if (edgeRow[x] > 0) refRow[x] = blurRow[x] > 0.5f ? 1.0f : 0.0f;
    }
    QImage resultSmall(refinedMask.cols, refinedMask.rows, QImage::Format_ARGB32);
    for (int y = 0; y < refinedMask.rows; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(resultSmall.scanLine(y));
        const float* frow = refinedMask.ptr<float>(y);
        for (int x = 0; x < refinedMask.cols; ++x) {
            const uchar a = static_cast<uchar>(std::clamp(frow[x]*255.0f,0.0f,255.0f));
            row[x] = qRgba(255,255,255,a);
        }
    }
    return resultSmall.scaled(mask.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
#endif

DocumentController::DocumentController(QObject* parent)
    : QObject(parent)
    , m_cancelFlag(std::make_shared<std::atomic<bool>>(false))
    , m_hqCancelFlag(std::make_shared<std::atomic<bool>>(false))
{
    m_previewDebounce = new QTimer(this);
    m_previewDebounce->setSingleShot(true);
    m_previewDebounce->setInterval(100);
    connect(m_previewDebounce, &QTimer::timeout, this, &DocumentController::rebuildPreview);

    m_hqTimer = new QTimer(this);
    m_hqTimer->setSingleShot(true);
    m_hqTimer->setInterval(1500);
    connect(m_hqTimer, &QTimer::timeout, this, &DocumentController::buildHqPreview);

    m_maskSaveTimer = new QTimer(this);
    m_maskSaveTimer->setSingleShot(true);
    m_maskSaveTimer->setInterval(50);
    connect(m_maskSaveTimer, &QTimer::timeout, this, &DocumentController::flushMaskSave);

    connect(&m_document, &lumen::DocumentModel::changed, this, [this]() {
        emit documentChanged();
        emit adjustmentsChanged();
        emit layersChanged();
        m_hqTimer->stop();
        *m_hqCancelFlag = true;
        m_previewDebounce->start();
    });
    connect(&m_document, &lumen::DocumentModel::historyChanged, this, [this]() {
        // historyLog is now a direct, live read of m_document's actual
        // undo stack (see historyLog() below) -- it needs to refresh
        // exactly whenever that stack does, which historyChanged already
        // signals for every commit/undo/redo. No separate bookkeeping.
        emit historyChanged();
        emit historyLogChanged();
    });
    connect(&m_document, &lumen::DocumentModel::structuralHistoryApplied,
            this, &DocumentController::resyncAfterStructuralHistory);
    connect(&m_aiRuntime, &lumen::AiRuntime::busyChanged,
            this, [this](bool busy){ setAiBusy(busy); });

    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setInterval(2 * 60 * 1000);
    connect(m_autosaveTimer, &QTimer::timeout, this, &DocumentController::autoSave);
    m_autosaveTimer->start();

    checkRecovery();
}

// ── Properties ────────────────────────────────────────────────────────────────
bool DocumentController::hasDocument() const { return m_document.hasDocument(); }
QString DocumentController::sourceName() const {
    return m_document.hasDocument()
        ? QFileInfo(m_document.sourcePath()).fileName() : "No image loaded";
}
QString DocumentController::imageUrl() const {
    if (!m_document.hasDocument()) return {};
    if (m_showOriginal || m_previewPath.isEmpty())
        return QUrl::fromLocalFile(m_document.sourcePath()).toString();
    return QUrl::fromLocalFile(m_previewPath).toString();
}
bool DocumentController::canUndo()      const { return m_document.canUndo(); }
bool DocumentController::canRedo()      const { return m_document.canRedo(); }
bool DocumentController::showOriginal() const { return m_showOriginal; }
bool DocumentController::cropActive()   const { return m_cropActive; }
void DocumentController::setShowOriginal(bool v) {
    if (m_showOriginal == v) return; m_showOriginal = v; emit previewChanged();
}

// ── Issue 5: target-aware sliders ────────────────────────────────────────────
// Every getter/setter below now routes through scalarAdjustmentForTarget /
// setScalarAdjustmentForTarget using m_activeAdjustmentTarget, instead of the
// old global-only scalarAdjustment(). When activeAdjustmentTarget == "" this
// behaves exactly like before (full-image editing).
double DocumentController::brightness()    const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Brightness, m_activeAdjustmentTarget); }
double DocumentController::exposure()      const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Exposure, m_activeAdjustmentTarget); }
double DocumentController::contrast()      const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Contrast, m_activeAdjustmentTarget); }
double DocumentController::saturation()    const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Saturation, m_activeAdjustmentTarget); }
double DocumentController::highlights()    const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Highlights, m_activeAdjustmentTarget); }
double DocumentController::shadows()       const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Shadows, m_activeAdjustmentTarget); }
double DocumentController::whites()        const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Whites, m_activeAdjustmentTarget); }
double DocumentController::blacks()        const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Blacks, m_activeAdjustmentTarget); }
double DocumentController::vibrance()      const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Vibrance, m_activeAdjustmentTarget); }
double DocumentController::temperature()   const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Temperature, m_activeAdjustmentTarget); }
double DocumentController::tint()          const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Tint, m_activeAdjustmentTarget); }
double DocumentController::noiseReduction()const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::NoiseReduction, m_activeAdjustmentTarget); }
double DocumentController::sharpening()    const { return m_document.scalarAdjustmentForTarget(lumen::AdjustmentType::Sharpening, m_activeAdjustmentTarget); }

void DocumentController::setBrightness(double v)    { setAdjustment(lumen::AdjustmentType::Brightness,   v); }
void DocumentController::setExposure(double v)      { setAdjustment(lumen::AdjustmentType::Exposure,     v); }
void DocumentController::setContrast(double v)      { setAdjustment(lumen::AdjustmentType::Contrast,     v); }
void DocumentController::setSaturation(double v)    { setAdjustment(lumen::AdjustmentType::Saturation,   v); }
void DocumentController::setHighlights(double v)    { setAdjustment(lumen::AdjustmentType::Highlights,   v); }
void DocumentController::setShadows(double v)       { setAdjustment(lumen::AdjustmentType::Shadows,      v); }
void DocumentController::setWhites(double v)        { setAdjustment(lumen::AdjustmentType::Whites,       v); }
void DocumentController::setBlacks(double v)        { setAdjustment(lumen::AdjustmentType::Blacks,       v); }
void DocumentController::setVibrance(double v)      { setAdjustment(lumen::AdjustmentType::Vibrance,     v); }
void DocumentController::setTemperature(double v)   { setAdjustment(lumen::AdjustmentType::Temperature,  v); }
void DocumentController::setTint(double v)          { setAdjustment(lumen::AdjustmentType::Tint,         v); }
void DocumentController::setNoiseReduction(double v){ setAdjustment(lumen::AdjustmentType::NoiseReduction,v); }
void DocumentController::setSharpening(double v)    { setAdjustment(lumen::AdjustmentType::Sharpening,   v); }

int  DocumentController::activeTool()  const { return m_activeTool; }
bool DocumentController::hasMask()     const { return !m_document.maskImage(m_activeAdjustmentTarget).isNull(); }
QString DocumentController::maskUrl()  const {
    const QString path = m_maskTempPaths.value(m_activeAdjustmentTarget);
    return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
}
int  DocumentController::sourceWidth()  const { return m_document.sourceSize().width(); }
int  DocumentController::sourceHeight() const { return m_document.sourceSize().height(); }
bool    DocumentController::aiBusy()   const { return m_aiBusy; }
QString DocumentController::aiStatus() const { return m_aiStatus; }

QVariantList DocumentController::layerModel() const {
    QVariantList list;
    for (const lumen::Layer& l : m_document.layers()) {
        QVariantMap m;
        m["id"]=l.id; m["name"]=l.name; m["opacity"]=l.opacity;
        m["visible"]=l.visible; m["order"]=l.order; m["realId"]=l.id;
        m["isBase"] = (l.order == 0);
        m["posX"]=l.posX; m["posY"]=l.posY;
        m["scaleX"]=l.scaleX; m["scaleY"]=l.scaleY; m["rotation"]=l.rotation;
        // Layer Transform Gizmo (stage 1): native pixel size of this layer's
        // image, needed by LayerTransformOverlay.qml to compute the on-canvas
        // bounding box for click-to-select hit-testing. Mirrors the exact
        // convention RenderPipeline::compositeOverlayLayers() already uses
        // (img.width()/height() * scaleX/scaleY * canvas-scale).
        const QImage img = m_document.layerImage(l.id);
        m["imgWidth"]  = img.width();
        m["imgHeight"] = img.height();
        list.prepend(m);
    }
    return list;
}
// historyLog is a direct, live view of m_document's actual undo stack --
// oldest action first, matching the order they were actually performed in.
// This used to be a separately-maintained QStringList (m_historyLog) that
// every action manually appended to via logHistory(), completely
// decoupled from the real undo/redo mechanism -- which is exactly why
// Undo/Redo used to show up as their OWN new entries instead of the panel
// reflecting the state actually being undone/redone to. There is no
// separate bookkeeping left to get out of sync: undo() shortens the real
// stack, redo() lengthens it again, and this getter (re-evaluated
// whenever historyLogChanged fires, see the constructor) just reads
// whatever is there.
QStringList DocumentController::historyLog() const { return m_document.historyLabels(); }

QVariantList DocumentController::maskList() const {
    QVariantList list;
    for (const lumen::Mask& m : m_document.masks()) {
        QVariantMap e;
        e["id"]   = m.id;
        e["name"] = m.name;
        const QString path = m_maskTempPaths.value(m.id);
        e["url"]  = path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
        list.append(e);
    }
    return list;
}

// Issue 5: combo model for the Masks tab — "Full Image" first, then each mask.
QVariantList DocumentController::adjustmentTargets() const {
    QVariantList list;
    QVariantMap full; full["id"] = QString(); full["name"] = "Full Image";
    list.append(full);
    for (const lumen::Mask& m : m_document.masks()) {
        QVariantMap e; e["id"] = m.id; e["name"] = m.name.isEmpty() ? "Mask" : m.name;
        list.append(e);
    }
    return list;
}

QString DocumentController::activeAdjustmentTarget() const { return m_activeAdjustmentTarget; }
void DocumentController::setActiveAdjustmentTarget(const QString& targetMaskId) {
    if (m_activeAdjustmentTarget == targetMaskId) return;
    if (m_maskSaveTimer && m_maskSaveTimer->isActive()) { m_maskSaveTimer->stop(); flushMaskSave(); }
    m_activeAdjustmentTarget = targetMaskId;
    syncBrushEngineToTarget(m_activeAdjustmentTarget);
    emit activeAdjustmentTargetChanged();
    emit adjustmentsChanged();   // sliders must re-read values for the new target
    emit maskChanged();          // hasMask/maskUrl/adjustmentTargets depend on the target too
}

QString DocumentController::selectedLayerId() const { return m_selectedLayerId; }
void DocumentController::setSelectedLayerId(const QString& id) {
    if (m_selectedLayerId == id) return;
    m_selectedLayerId = id;
    emit selectedLayerChanged();
}

QStringList DocumentController::recentFiles() const {
    QSettings s("LumenForge","LumenForge");
    return s.value("recentFiles").toStringList();
}
bool DocumentController::hasPendingRecovery() const { return m_hasPendingRecovery; }

// ── Helpers ───────────────────────────────────────────────────────────────────
void DocumentController::addRecentFile(const QString& path) {
    QSettings s("LumenForge","LumenForge");
    QStringList r = s.value("recentFiles").toStringList();
    r.removeAll(path); r.prepend(path);
    while (r.size() > 20) r.removeLast();
    s.setValue("recentFiles", r);
    emit recentFilesChanged();
}
void DocumentController::setAiBusy(bool busy) {
    if (m_aiBusy == busy) return; m_aiBusy = busy; emit aiBusyChanged();
}
void DocumentController::setAiStatus(const QString& s) {
    m_aiStatus = s; emit aiStatusChanged();
}
QString DocumentController::autosavePath() const {
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + "/lumenforge-autosave.lfproj";
}
void DocumentController::checkRecovery() {
    m_hasPendingRecovery = QFileInfo::exists(autosavePath());
    if (m_hasPendingRecovery) emit recoveryChanged();
}
void DocumentController::autoSave() {
    if (!m_document.hasDocument()) return;
    m_projectStore.saveProject(m_document, autosavePath());
}
void DocumentController::recoverProject() {
    if (m_projectStore.loadProject(m_document, autosavePath()))
        QFile::remove(autosavePath());
    m_hasPendingRecovery = false; emit recoveryChanged();
}
void DocumentController::discardRecovery() {
    QFile::remove(autosavePath());
    m_hasPendingRecovery = false; emit recoveryChanged();
}
void DocumentController::setActiveTool(int tool) {
    if (m_activeTool == tool) return;
    if (m_maskSaveTimer && m_maskSaveTimer->isActive()) { m_maskSaveTimer->stop(); flushMaskSave(); }
    m_activeTool = tool;
    m_cropActive = (tool == 5);
    emit cropActiveChanged(); emit activeToolChanged();
}
QString DocumentController::localPath(const QUrl& url) const {
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

// ── File operations ───────────────────────────────────────────────────────────
bool DocumentController::openImage(const QUrl& url) {
    const QString path = localPath(url);
    if (!m_document.openSourceImage(path)) {
        emit operationFailed("Could not open image."); return false;
    }
    m_brushEngine = std::make_unique<lumen::BrushEngine>(brushEngineSize(m_document.sourceSize()));
    for (const QString& oldPath : std::as_const(m_maskTempPaths)) QFile::remove(oldPath);
    m_maskTempPaths.clear(); m_pendingStrokes.clear();
    m_activeAdjustmentTarget.clear(); emit activeAdjustmentTargetChanged();
    m_selectedLayerId.clear(); emit selectedLayerChanged();
    emit maskChanged();
    QFile::remove(autosavePath());
    m_hasPendingRecovery = false; emit recoveryChanged();
    addRecentFile(m_document.sourcePath());
    return true;
}
bool DocumentController::saveProject(const QUrl& url) {
    if (!m_projectStore.saveProject(m_document, localPath(url))) {
        emit operationFailed("Could not save project."); return false;
    }
    return true;
}
bool DocumentController::loadProject(const QUrl& url) {
    if (!m_projectStore.loadProject(m_document, localPath(url))) {
        emit operationFailed("Could not load project."); return false;
    }
    m_brushEngine = std::make_unique<lumen::BrushEngine>(brushEngineSize(m_document.sourceSize()));
    for (const QString& oldPath : std::as_const(m_maskTempPaths)) QFile::remove(oldPath);
    m_maskTempPaths.clear();
    m_pendingStrokes.clear();
    m_activeAdjustmentTarget.clear(); emit activeAdjustmentTargetChanged();
    syncBrushEngineToTarget(m_activeAdjustmentTarget);
    emit maskChanged();
    return true;
}
bool DocumentController::exportImage(const QUrl& url) {
    if (!m_exportService.exportImage(m_document, localPath(url))) {
        emit operationFailed("Could not export image."); return false;
    }
    return true;
}
void DocumentController::resetAdjustments() {
    // Reset only the CURRENTLY ACTIVE target (issue 5) — resetting the full
    // image must not wipe out per-mask edits and vice versa.
    // Wrapped in one explicit transaction so resetting several adjustment
    // types at once (each of which would otherwise open/commit its own
    // transaction independently) becomes a single History entry and a
    // single undo step, not one per type that happened to be non-zero.
    const QString label = m_activeAdjustmentTarget.isEmpty()
        ? "Reset all adjustments (Full Image)" : "Reset all adjustments (mask)";
    m_document.beginHistoryTransaction(label);
    for (auto t : {
        lumen::AdjustmentType::Brightness, lumen::AdjustmentType::Exposure,
        lumen::AdjustmentType::Contrast,   lumen::AdjustmentType::Saturation,
        lumen::AdjustmentType::Highlights, lumen::AdjustmentType::Shadows,
        lumen::AdjustmentType::Whites,     lumen::AdjustmentType::Blacks,
        lumen::AdjustmentType::Vibrance,   lumen::AdjustmentType::Temperature,
        lumen::AdjustmentType::Tint, lumen::AdjustmentType::NoiseReduction,
        lumen::AdjustmentType::Sharpening,
    }) m_document.setScalarAdjustmentForTarget(t, 0.0, m_activeAdjustmentTarget, label);
    m_document.commitHistoryTransaction();
}
void DocumentController::rotateClockwise()        { m_document.rotateClockwise(); }
void DocumentController::rotateCounterClockwise() { m_document.rotateCounterClockwise(); }
void DocumentController::flipHorizontal()         { m_document.flipHorizontal(); }
void DocumentController::flipVertical()           { m_document.flipVertical(); }
void DocumentController::undo()                   { m_document.undo(); }
void DocumentController::redo()                   { m_document.redo(); }


void DocumentController::beginAdjustmentEdit() {
    if (m_adjustmentEditOpen) return;
    m_adjustmentEditOpen = true;
    m_document.beginHistoryTransaction("Adjustment");
}
void DocumentController::commitAdjustmentEdit() {
    if (!m_adjustmentEditOpen) return;
    m_adjustmentEditOpen = false;
    // Pushes (at most) ONE undo step for the whole drag -- DocumentModel
    // itself detects and skips a no-op interaction (press without moving).
    // The committed entry's label is already the LAST tick's descriptive
    // text (e.g. "Exposure: 1.50") -- setScalarAdjustmentForTarget()
    // refreshes it on every tick via AutoHistoryStep's nested-label
    // update, so there's nothing left to do here.
    m_document.commitHistoryTransaction();
}

// ── Brush mask ────────────────────────────────────────────────────────────────
void DocumentController::paintMaskStroke(double x, double y, double radius, bool erase) {
    if (!m_document.hasDocument() || !m_brushEngine) return;
    m_pendingStrokes.append({x, y, radius, erase});
}
void DocumentController::commitMaskPaint() {
    if (m_maskSaveTimer && m_maskSaveTimer->isActive()) m_maskSaveTimer->stop();
    if (!m_brushEngine) return;
    // CRITICAL FIX: m_pendingStrokes coordinates arrive from MaskCanvas.qml in
    // FULL SOURCE-RESOLUTION pixel space (it converts mouse position using
    // docCtrl.sourceWidth/sourceHeight). But m_brushEngine's QImage is capped
    // to brushEngineSize() (issue 4 fix — max 2000px) for paint performance.
    // Without rescaling, strokes were being painted at full-res coordinates
    // into a much smaller buffer, landing in the wrong place (or off-canvas
    // entirely) on any image above the 2000px cap — which is almost every
    // real photo. Scale x/y/radius into brush-engine space here, once, before
    // painting.
    const QString targetId = ensurePaintTarget();
    const QSize srcSz = m_document.sourceSize();
    const QSize beSz  = m_brushEngine->mask().size();
    const double scale = (srcSz.width() > 0)
        ? static_cast<double>(beSz.width()) / static_cast<double>(srcSz.width())
        : 1.0;
    for (const auto& s : m_pendingStrokes)
        m_brushEngine->paintStroke(QPointF(s.x * scale, s.y * scale), s.radius * scale, 0.85, s.erase);
    m_pendingStrokes.clear();
    // Issue 4: brush engine is capped resolution; upsample before storing.
    m_document.setMaskImage(targetId, upsampleMaskToSource(m_brushEngine->mask(), srcSz));
    saveMaskToTemp(targetId);
    emit maskChanged();
}
void DocumentController::flushMaskSave() {
    if (!m_brushEngine || m_activeAdjustmentTarget.isEmpty()) return;
    m_document.setMaskImage(m_activeAdjustmentTarget,
                             upsampleMaskToSource(m_brushEngine->mask(), m_document.sourceSize()));
    saveMaskToTemp(m_activeAdjustmentTarget); emit maskChanged();
}
void DocumentController::clearMask() {
    if (m_maskSaveTimer->isActive()) m_maskSaveTimer->stop();
    const QString target = m_activeAdjustmentTarget;
    if (target.isEmpty()) return; // "Full Image" has no mask to delete
    m_pendingStrokes.clear();
    m_document.removeMask(target);
    const QString oldPath = m_maskTempPaths.take(target);
    if (!oldPath.isEmpty()) QFile::remove(oldPath);
    // Switches back to Full Image, resyncs the (now-empty) brush surface,
    // and emits maskChanged/adjustmentsChanged for us.
    setActiveAdjustmentTarget(QString());
}
void DocumentController::saveMaskToTemp(const QString& maskId) {
    if (maskId.isEmpty()) return;
    const QImage mask = m_document.maskImage(maskId);
    if (mask.isNull()) return;
    QImage dm = (mask.width() > 1600)
        ? mask.scaled(1600,1200,Qt::KeepAspectRatio,Qt::FastTransformation) : mask;
    const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QString("/lumenforge-mask-%1.png").arg(++m_maskVersion);
    if (dm.save(path,"PNG")) {
        const QString oldPath = m_maskTempPaths.value(maskId);
        if (!oldPath.isEmpty()) QFile::remove(oldPath);
        m_maskTempPaths[maskId] = path;
    }
}
QString DocumentController::ensurePaintTarget() {
    if (!m_activeAdjustmentTarget.isEmpty()) return m_activeAdjustmentTarget;
    const QString id = m_document.addMask(QString("Mask %1").arg(m_document.masks().size() + 1));
    setActiveAdjustmentTarget(id);
    return id;
}
void DocumentController::syncBrushEngineToTarget(const QString& targetId) {
    if (!m_brushEngine) return;
    const QSize beSz = m_brushEngine->mask().size();
    QImage buffer(beSz, QImage::Format_ARGB32);
    buffer.fill(Qt::transparent);
    const QImage existing = m_document.maskImage(targetId);
    if (!existing.isNull()) {
        QPainter p(&buffer);
        p.drawImage(QRect(QPoint(0,0), beSz),
                    existing.scaled(beSz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
    m_brushEngine->mask() = buffer;
}
void DocumentController::resyncAfterStructuralHistory() {
    // A crop/inpaint/upscale undo or redo just swapped sourceImage and/or
    // masks out from under whatever was cached -- same resync applyCrop()
    // already does going forward, needed here going backward/forward too.
    m_brushEngine = std::make_unique<lumen::BrushEngine>(brushEngineSize(m_document.sourceSize()));

    // The currently-selected mask target may not exist at this point in
    // history (e.g. undoing past the crop/AI op that created it) -- fall
    // back to Full Image rather than pointing at a dangling id.
    bool targetStillExists = m_activeAdjustmentTarget.isEmpty();
    for (const lumen::Mask& mask : m_document.masks())
        if (mask.id == m_activeAdjustmentTarget) { targetStillExists = true; break; }
    if (!targetStillExists) {
        m_activeAdjustmentTarget.clear();
        emit activeAdjustmentTargetChanged();
        emit adjustmentsChanged();
    }
    syncBrushEngineToTarget(m_activeAdjustmentTarget);

    for (const QString& oldPath : std::as_const(m_maskTempPaths)) QFile::remove(oldPath);
    m_maskTempPaths.clear();
    for (const lumen::Mask& mask : m_document.masks()) saveMaskToTemp(mask.id);
    emit maskChanged();
}
void DocumentController::applyGradientMask(double x1,double y1,double x2,double y2) {
    if (!m_document.hasDocument()) return;
    const QString targetId = ensurePaintTarget();
    const QSize sz = m_document.sourceSize();
    const QSize beSz = brushEngineSize(sz);
    const double scaleX = static_cast<double>(beSz.width())  / sz.width();
    const double scaleY = static_cast<double>(beSz.height()) / sz.height();
    QImage mask(beSz, QImage::Format_ARGB32);
    QPainter p(&mask);
    QLinearGradient grad(x1*scaleX,y1*scaleY,x2*scaleX,y2*scaleY);
    grad.setColorAt(0.0,QColor(255,255,255,255));
    grad.setColorAt(1.0,QColor(255,255,255,0));
    p.fillRect(QRectF(0,0,beSz.width(),beSz.height()),grad); p.end();
    if (!m_brushEngine) m_brushEngine = std::make_unique<lumen::BrushEngine>(beSz);
    m_brushEngine->mask() = mask;
    m_document.setMaskImage(targetId, upsampleMaskToSource(mask, sz));
    saveMaskToTemp(targetId); emit maskChanged();
}
void DocumentController::applyRadialMask(double cx,double cy,double radius) {
    if (!m_document.hasDocument()) return;
    const QString targetId = ensurePaintTarget();
    const QSize sz = m_document.sourceSize();
    const QSize beSz = brushEngineSize(sz);
    const double scaleX = static_cast<double>(beSz.width())  / sz.width();
    const double scaleY = static_cast<double>(beSz.height()) / sz.height();
    QImage mask(beSz, QImage::Format_ARGB32); mask.fill(Qt::transparent);
    QPainter p(&mask);
    QRadialGradient grad(cx*scaleX,cy*scaleY,radius*qMin(scaleX,scaleY));
    grad.setColorAt(0.0,QColor(255,255,255,255));
    grad.setColorAt(0.65,QColor(255,255,255,180));
    grad.setColorAt(1.0,QColor(255,255,255,0));
    p.fillRect(QRectF(0,0,beSz.width(),beSz.height()),grad); p.end();
    if (!m_brushEngine) m_brushEngine = std::make_unique<lumen::BrushEngine>(beSz);
    m_brushEngine->mask() = mask;
    m_document.setMaskImage(targetId, upsampleMaskToSource(mask, sz));
    saveMaskToTemp(targetId); emit maskChanged();
}
void DocumentController::applyCrop(int x,int y,int w,int h) {
    if (!m_document.hasDocument()) return;
    const QSize sz = m_document.sourceSize();
    const QRect rect(qBound(0,x,sz.width()),qBound(0,y,sz.height()),
        qBound(1,w,sz.width()-qBound(0,x,sz.width())),
        qBound(1,h,sz.height()-qBound(0,y,sz.height())));
    if (rect.isEmpty()) return;
    // Structural: also captures/restores sourceImage + masks, so Undo can
    // fully reverse a crop (previously replaceSourceImage() never
    // participated in undo/redo at all).
    m_document.beginHistoryTransaction("Crop", /*structural=*/true);
    // Crop every mask's pixel data by the identical rect BEFORE replacing the
    // source image, so marked regions stay aligned to the same content
    // instead of drifting to absolute canvas coordinates post-crop. Masks are
    // always stored at the current source resolution (see setMaskImage()
    // call sites), so rect applies directly; intersected() is a defensive
    // guard in case that invariant is ever violated (e.g. after an upscale).
    for (const lumen::Mask& mask : m_document.masks()) {
        if (mask.mask.isNull()) continue;
        const QRect clamped = rect.intersected(mask.mask.rect());
        m_document.setMaskImage(mask.id, clamped.isEmpty() ? QImage() : mask.mask.copy(clamped));
    }
    m_document.replaceSourceImage(m_document.sourceImage().copy(rect));
    // Final label needs the post-crop size, which is only known now (crop
    // replaced the source image on the line above) -- commitHistoryTransaction()'s
    // optional override exists for exactly this "known only at the end"
    // case, so the pushed undo entry reads e.g. "Crop 4000×3000" directly,
    // no separate logging step needed.
    m_document.commitHistoryTransaction(
        QString("Crop %1\u00d7%2").arg(m_document.sourceSize().width()).arg(m_document.sourceSize().height()));
    m_brushEngine = std::make_unique<lumen::BrushEngine>(brushEngineSize(m_document.sourceSize()));
    syncBrushEngineToTarget(m_activeAdjustmentTarget);
    for (const QString& oldPath : std::as_const(m_maskTempPaths)) QFile::remove(oldPath);
    m_maskTempPaths.clear(); m_pendingStrokes.clear();
    // Regenerate temp preview files for whatever masks still exist, so the
    // Masks tab thumbnails don't keep showing now-cropped-away content.
    for (const lumen::Mask& mask : m_document.masks()) saveMaskToTemp(mask.id);
    {
        const QImage& cropped = m_document.sourceImage();
        const QSize maxLQ(1400,1050);
        QImage ph = (cropped.width()>maxLQ.width()||cropped.height()>maxLQ.height())
            ? cropped.scaled(maxLQ,Qt::KeepAspectRatio,Qt::SmoothTransformation) : cropped;
        const QString phPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            + QString("/lumenforge-preview-%1.jpg").arg(++m_previewVersion);
        if (ph.convertToFormat(QImage::Format_RGB888).save(phPath,"JPEG",80)) {
            if (!m_previewPath.isEmpty()) QFile::remove(m_previewPath);
            m_previewPath = phPath;
            emit previewChanged();
        }
    }
    setActiveTool(0); emit maskChanged();
}
void DocumentController::refineEdges() {
#ifdef HAVE_OPENCV
    const QString targetId = m_activeAdjustmentTarget;
    if (!m_document.hasDocument()||targetId.isEmpty()||m_document.maskImage(targetId).isNull()||m_aiBusy) return;
    setAiBusy(true); setAiStatus("Refining edges\u2026");
    const QImage src=m_document.sourceImage(), mask=m_document.maskImage(targetId);
    auto* w = new QFutureWatcher<QImage>(this);
    connect(w,&QFutureWatcher<QImage>::finished,this,[this,w,targetId](){
        const QImage refined=w->result();
        if (!refined.isNull()) {
            // Issue 4: store a CAPPED copy in the brush engine (fast painting),
            // and the FULL-RES copy (already at source size, since
            // refineMaskEdgesOcv upsamples its result to mask.size()) in the
            // document so exports/HQ preview stay correct.
            if (m_brushEngine) {
                m_brushEngine->mask() = downsampleMaskToBrush(refined, m_brushEngine->mask().size());
            }
            m_document.setMaskImage(targetId, refined);
            saveMaskToTemp(targetId); emit maskChanged();
            setAiStatus("Done");
        } else setAiStatus("Edge refinement failed");
        setAiBusy(false); w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([src,mask]()->QImage{return refineMaskEdgesOcv(src,mask);}));
#else
    setAiStatus("Edge refinement requires OpenCV");
#endif
}

// Creates a real, independently-paintable mask via DocumentModel::addMask()
// and switches editing focus to it. Sliders for a target id with no
// Adjustment entries yet naturally read 0 (see
// DocumentModel::scalarAdjustmentForTarget), satisfying "new mask resets
// sliders to zero" without any extra bookkeeping.
void DocumentController::addNewMaskTarget() {
    if (!m_document.hasDocument()) return;
    const QString name = QString("Mask %1").arg(m_document.masks().size() + 1);
    const QString id = m_document.addMask(name);
    setActiveAdjustmentTarget(id);
}

void DocumentController::requestAiMask(double x,double y) {
    if (!m_document.hasDocument()||m_aiBusy) return;
    setAiStatus("Loading mask model\u2026");
    const QImage src=m_document.sourceImage();
    m_aiRuntime.predictMask(src,QPointF(x,y),[this](QImage result,QString error){
        if (!result.isNull()) {
            const QString targetId = ensurePaintTarget();
            if (!m_brushEngine) m_brushEngine=std::make_unique<lumen::BrushEngine>(brushEngineSize(m_document.sourceSize()));
            m_brushEngine->mask()=downsampleMaskToBrush(result, m_brushEngine->mask().size());
            m_document.setMaskImage(targetId, result); saveMaskToTemp(targetId); emit maskChanged();
            setAiStatus("Done");
        } else {
            const QString msg=error.isEmpty()?"AI mask prediction failed.":error;
            setAiStatus(msg); emit operationFailed(msg);
        }
    });
}
void DocumentController::applyInpaint() {
    const QString targetId = m_activeAdjustmentTarget;
    if (!m_document.hasDocument()||targetId.isEmpty()||m_document.maskImage(targetId).isNull()||m_aiBusy) return;
    if (!m_inpaintEngine) {
        try { m_inpaintEngine=std::make_unique<lumen::InpaintEngine>(); }
        catch(const std::exception& e) {
            const QString msg=QString("Inpaint init failed: %1").arg(QString::fromUtf8(e.what()));
            setAiStatus(msg); emit operationFailed(msg); return;
        }
    }
    setAiBusy(true); setAiStatus("Inpainting\u2026");
    const QImage src=m_document.sourceImage(), mask=m_document.maskImage(targetId);
    auto* w=new QFutureWatcher<QImage>(this);
    connect(w,&QFutureWatcher<QImage>::finished,this,[this,w](){
        m_document.beginHistoryTransaction("Object removal", /*structural=*/true);
        m_document.replaceSourceImage(w->result());
        m_document.commitHistoryTransaction();
        if (m_brushEngine) m_brushEngine->resize(brushEngineSize(m_document.sourceSize()));
        const QString err = m_inpaintEngine ? m_inpaintEngine->lastError() : QString();
        if (!err.isEmpty()){setAiStatus(err);emit operationFailed(err);}
        else{setAiStatus("Done");}
        setAiBusy(false); rebuildPreview(); w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([this,src,mask]()mutable->QImage{
        QMetaObject::invokeMethod(this,[this]{setAiStatus("Running inpaint\u2026");});
        return m_inpaintEngine->inpaint(src,mask);
    }));
}
void DocumentController::applyUpscale() {
    if (!m_document.hasDocument()||m_aiBusy) return;
    if (!m_upscaleEngine) {
        try { m_upscaleEngine=std::make_unique<lumen::UpscaleEngine>(); }
        catch(const std::exception& e) {
            const QString msg=QString("Upscale init failed: %1").arg(QString::fromUtf8(e.what()));
            setAiStatus(msg); emit operationFailed(msg); return;
        }
    }
    setAiBusy(true); setAiStatus("Upscaling\u2026");
    const QImage src=m_document.sourceImage();
    auto* w=new QFutureWatcher<QImage>(this);
    connect(w,&QFutureWatcher<QImage>::finished,this,[this,w](){
        m_document.beginHistoryTransaction("AI Upscale \u00d74", /*structural=*/true);
        m_document.replaceSourceImage(w->result());
        m_document.commitHistoryTransaction();
        if (m_brushEngine) m_brushEngine->resize(brushEngineSize(m_document.sourceSize()));
        const QString err=m_upscaleEngine?m_upscaleEngine->lastError():QString();
        setAiStatus(err.isEmpty()?"Done":QString("Done (%1)").arg(err));
        setAiBusy(false); rebuildPreview(); w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([this,src]()mutable->QImage{
        QMetaObject::invokeMethod(this,[this]{setAiStatus("Running upscale\u2026");});
        return m_upscaleEngine->upscale(src);
    }));
}

// ── Layers (issue 6) ──────────────────────────────────────────────────────────
void DocumentController::addImageLayer(const QUrl& url){
    m_document.addImageLayer(localPath(url));
    // Place new overlay centred on canvas at its native size by default.
    const auto layers = m_document.layers();
    if (!layers.isEmpty())
        setSelectedLayerId(layers.last().id);
}
void DocumentController::deleteLayer(const QString& id){
    if (m_selectedLayerId == id) setSelectedLayerId(QString());
    m_document.deleteLayer(id);
}
void DocumentController::setLayerOpacity(const QString& id,double o){m_document.setLayerOpacity(id,o);}
void DocumentController::setLayerVisible(const QString& id,bool v){m_document.setLayerVisible(id,v);}
void DocumentController::moveLayerUp(const QString& id){
    const auto& layers=m_document.layers();
    for (int i=0;i<layers.size();++i)
        if (layers[i].id==id && i>0){m_document.moveLayer(i,i-1); break;}
}
void DocumentController::moveLayerDown(const QString& id){
    const auto& layers=m_document.layers();
    for (int i=0;i<layers.size();++i)
        if (layers[i].id==id && i<layers.size()-1){m_document.moveLayer(i,i+1); break;}
}
void DocumentController::setLayerTransform(const QString& id,
                                            double posX, double posY,
                                            double scaleX, double scaleY,
                                            double rotation) {
    m_document.setLayerTransform(id, posX, posY, scaleX, scaleY, rotation);
}
void DocumentController::beginLayerTransformEdit() {
    if (m_layerTransformEditOpen) return;
    m_layerTransformEditOpen = true;
    m_document.beginHistoryTransaction("Transform layer");
}
void DocumentController::commitLayerTransformEdit() {
    if (!m_layerTransformEditOpen) return;
    m_layerTransformEditOpen = false;
    // Pushes (at most) ONE undo step for the whole drag -- DocumentModel
    // itself detects and skips a no-op interaction (press without moving),
    // via transactionChangedAnything()'s layer check.
    m_document.commitHistoryTransaction();
}
void DocumentController::exportBatch(const QUrl& dir,const QStringList& fmts){
    m_exportService.exportBatch(m_document,dir.toLocalFile(),fmts);
}

// ── buildMaskAdjLayers (issue 5 helper) ───────────────────────────────────────
std::vector<lumen::MaskAdjLayer> DocumentController::buildMaskAdjLayers() const {
    std::vector<lumen::MaskAdjLayer> result;
    for (const lumen::Mask& mask : m_document.masks()) {
        const auto adjs = m_document.adjustmentsForTarget(mask.id);
        if (adjs.isEmpty() || mask.mask.isNull()) continue;
        result.push_back({mask.mask, adjs});
    }
    return result;
}

// ── Preview (LQ) ──────────────────────────────────────────────────────────────
void DocumentController::rebuildPreview() {
    if (!m_document.hasDocument()) {
        ++m_previewRequestId; m_previewPath.clear(); emit previewChanged(); return;
    }
    if (m_previewWatcher && m_previewWatcher->isRunning()) {
        m_previewPending=true; *m_cancelFlag=true; return;
    }
    *m_cancelFlag=false;
    const QImage src=m_document.sourceImage();
    // Issue 5: global adjustments are those with targetMaskId=="" — use
    // adjustmentsForTarget("") rather than the legacy adjustments() (which
    // returns EVERY adjustment regardless of target, including mask-scoped
    // ones that must NOT apply to the whole image).
    const QVector<lumen::Adjustment> globalAdjs = m_document.adjustmentsForTarget(QString());
    const auto maskAdjLayers = buildMaskAdjLayers();
    const QVector<lumen::Layer> layers = m_document.layers();
    QHash<QString, QImage> layerImages;
    for (const lumen::Layer& l : layers)
        layerImages.insert(l.id, m_document.layerImage(l.id));

    const int reqId=++m_previewRequestId;
    auto cancelled=m_cancelFlag;
    auto* watcher=new QFutureWatcher<QImage>(this);
    m_previewWatcher=watcher;
    connect(watcher,&QFutureWatcher<QImage>::finished,this,[this,watcher,reqId](){
        const QImage preview=watcher->result();
        if (!preview.isNull()&&reqId==m_previewRequestId) {
            const QString path=QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                +QString("/lumenforge-preview-%1.jpg").arg(++m_previewVersion);
            if (preview.save(path,"JPEG",88)){
                m_previewPath=path; emit previewChanged();
                *m_hqCancelFlag=false; m_hqTimer->start();
            }
        }
        watcher->deleteLater(); m_previewWatcher=nullptr;
        if (m_previewPending){m_previewPending=false;rebuildPreview();}
    });
    watcher->setFuture(QtConcurrent::run(
        [pipeline=m_renderPipeline,src,globalAdjs,maskAdjLayers,layers,layerImages,cancelled](){
            return pipeline.renderWithLayers(src, globalAdjs, maskAdjLayers, layers, layerImages,
                                             QSize(1400,1050), cancelled);
        }));
}

// ── Preview (HQ — after 1.5 s idle) ──────────────────────────────────────────
void DocumentController::buildHqPreview() {
    if (!m_document.hasDocument()) return;
    if ((m_previewWatcher&&m_previewWatcher->isRunning())||(m_hqWatcher&&m_hqWatcher->isRunning())) {
        m_hqTimer->start(500); return;
    }
    *m_hqCancelFlag=false;
    const QImage src=m_document.sourceImage();
    const QVector<lumen::Adjustment> globalAdjs = m_document.adjustmentsForTarget(QString());
    const auto maskAdjLayers = buildMaskAdjLayers();
    const QVector<lumen::Layer> layers = m_document.layers();
    QHash<QString, QImage> layerImages;
    for (const lumen::Layer& l : layers)
        layerImages.insert(l.id, m_document.layerImage(l.id));

    const int reqId=m_previewRequestId;
    auto cancelled=m_hqCancelFlag;
    auto* watcher=new QFutureWatcher<QImage>(this);
    m_hqWatcher=watcher;
    connect(watcher,&QFutureWatcher<QImage>::finished,this,[this,watcher,reqId](){
        const QImage preview=watcher->result();
        if (!preview.isNull()&&reqId==m_previewRequestId) {
            const QString path=QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                +QString("/lumenforge-preview-%1.jpg").arg(++m_previewVersion);
            if (preview.save(path,"JPEG",95)){
                if (!m_previewPath.isEmpty()) QFile::remove(m_previewPath);
                m_previewPath=path; emit previewChanged();
            }
        }
        watcher->deleteLater(); m_hqWatcher=nullptr;
    });
    watcher->setFuture(QtConcurrent::run(
        [pipeline=m_renderPipeline,src,globalAdjs,maskAdjLayers,layers,layerImages,cancelled](){
            return pipeline.renderWithLayers(src, globalAdjs, maskAdjLayers, layers, layerImages,
                                             QSize(3840,2160), cancelled);
        }));
}

// ── Target-aware setAdjustment (issue 5) ─────────────────────────────────────
void DocumentController::setAdjustment(lumen::AdjustmentType type,double value) {
    if (qFuzzyCompare(m_document.scalarAdjustmentForTarget(type, m_activeAdjustmentTarget) + 1.0, value + 1.0)) return;
    // DocumentModel::setScalarAdjustmentForTarget() now builds its own
    // descriptive label from type+value ("Exposure: 1.50") and writes it
    // straight into the real undo-stack entry -- whether that's a
    // standalone entry (no drag in progress) or a refresh of the
    // outer transaction opened by beginAdjustmentEdit() (mid-drag). No
    // separate label bookkeeping is needed here anymore.
    m_document.setScalarAdjustmentForTarget(type, value, m_activeAdjustmentTarget);
}
