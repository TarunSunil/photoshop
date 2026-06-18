# PART 4 of 4 — App layer — controller, QML UI, app/root CMake, license, model dir
# Tasks 28–36 (9 files)

## Checklist (mark [x] as each file is completed)
- [ ] Task 28: REPLACE `app/src/editor/DocumentController.hpp`
- [ ] Task 29: REPLACE `app/src/editor/DocumentController.cpp`
- [ ] Task 30: REPLACE `app/resources/qml/MaskCanvas.qml`
- [ ] Task 31: REPLACE `app/resources/qml/Main.qml`
- [ ] Task 32: REPLACE `app/CMakeLists.txt`
- [ ] Task 33: REPLACE `app/src/main/main.cpp`
- [ ] Task 34: REPLACE `CMakeLists.txt` (root)
- [ ] Task 35: CREATE `LICENSE.txt`
- [ ] Task 36: CREATE `models/.gitkeep`

---

## TASK 28 — REPLACE `app/src/editor/DocumentController.hpp`

```cpp
#pragma once
#include "editor-core/DocumentModel.hpp"
#include "export-core/ExportService.hpp"
#include "image-core/RenderPipeline.hpp"
#include "mask-core/BrushEngine.hpp"
#include "ai-core/AiRuntime.hpp"
#include "ai-core/InpaintEngine.hpp"
#include "ai-core/UpscaleEngine.hpp"
#include "storage/ProjectStore.hpp"
#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <atomic>
#include <memory>
class DocumentController final : public QObject {
    Q_OBJECT
    // Core
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY documentChanged)
    Q_PROPERTY(QString imageUrl READ imageUrl NOTIFY previewChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(bool showOriginal READ showOriginal WRITE setShowOriginal NOTIFY previewChanged)
    // Adjustments
    Q_PROPERTY(double exposure    READ exposure    WRITE setExposure    NOTIFY adjustmentsChanged)
    Q_PROPERTY(double contrast    READ contrast    WRITE setContrast    NOTIFY adjustmentsChanged)
    Q_PROPERTY(double saturation  READ saturation  WRITE setSaturation  NOTIFY adjustmentsChanged)
    Q_PROPERTY(double highlights  READ highlights  WRITE setHighlights  NOTIFY adjustmentsChanged)
    Q_PROPERTY(double shadows     READ shadows     WRITE setShadows     NOTIFY adjustmentsChanged)
    Q_PROPERTY(double whites      READ whites      WRITE setWhites      NOTIFY adjustmentsChanged)
    Q_PROPERTY(double blacks      READ blacks      WRITE setBlacks      NOTIFY adjustmentsChanged)
    Q_PROPERTY(double vibrance    READ vibrance    WRITE setVibrance    NOTIFY adjustmentsChanged)
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY adjustmentsChanged)
    Q_PROPERTY(double tint        READ tint        WRITE setTint        NOTIFY adjustmentsChanged)
    Q_PROPERTY(double noiseReduction READ noiseReduction WRITE setNoiseReduction NOTIFY adjustmentsChanged)
    Q_PROPERTY(double sharpening  READ sharpening  WRITE setSharpening  NOTIFY adjustmentsChanged)
    // M5 — masking
    Q_PROPERTY(int    activeTool  READ activeTool  WRITE setActiveTool  NOTIFY activeToolChanged)
    Q_PROPERTY(bool   hasMask     READ hasMask     NOTIFY maskChanged)
    Q_PROPERTY(QString maskUrl    READ maskUrl     NOTIFY maskChanged)
    Q_PROPERTY(int sourceWidth    READ sourceWidth  NOTIFY documentChanged)
    Q_PROPERTY(int sourceHeight   READ sourceHeight NOTIFY documentChanged)
    // M6/M7 — AI
    Q_PROPERTY(bool   aiBusy     READ aiBusy      NOTIFY aiBusyChanged)
    Q_PROPERTY(QString aiStatus  READ aiStatus    NOTIFY aiStatusChanged)
    // M8 — layers
    Q_PROPERTY(QVariantList layerModel READ layerModel NOTIFY layersChanged)
    // M10 — recovery
    Q_PROPERTY(bool hasPendingRecovery READ hasPendingRecovery NOTIFY recoveryChanged)
public:
    explicit DocumentController(QObject* parent = nullptr);
    [[nodiscard]] bool    hasDocument()  const;
    [[nodiscard]] QString sourceName()   const;
    [[nodiscard]] QString imageUrl()     const;
    [[nodiscard]] bool    canUndo()      const;
    [[nodiscard]] bool    canRedo()      const;
    [[nodiscard]] bool    showOriginal() const;
    void setShowOriginal(bool v);
    [[nodiscard]] double exposure()      const;  void setExposure(double v);
    [[nodiscard]] double contrast()      const;  void setContrast(double v);
    [[nodiscard]] double saturation()    const;  void setSaturation(double v);
    [[nodiscard]] double highlights()    const;  void setHighlights(double v);
    [[nodiscard]] double shadows()       const;  void setShadows(double v);
    [[nodiscard]] double whites()        const;  void setWhites(double v);
    [[nodiscard]] double blacks()        const;  void setBlacks(double v);
    [[nodiscard]] double vibrance()      const;  void setVibrance(double v);
    [[nodiscard]] double temperature()   const;  void setTemperature(double v);
    [[nodiscard]] double tint()          const;  void setTint(double v);
    [[nodiscard]] double noiseReduction()const;  void setNoiseReduction(double v);
    [[nodiscard]] double sharpening()    const;  void setSharpening(double v);
    [[nodiscard]] int    activeTool()    const;  void setActiveTool(int tool);
    [[nodiscard]] bool   hasMask()       const;
    [[nodiscard]] QString maskUrl()      const;
    [[nodiscard]] int sourceWidth()      const;
    [[nodiscard]] int sourceHeight()     const;
    [[nodiscard]] bool   aiBusy()        const;
    [[nodiscard]] QString aiStatus()     const;
    [[nodiscard]] QVariantList layerModel() const;
    [[nodiscard]] bool hasPendingRecovery() const;
    Q_INVOKABLE bool openImage(const QUrl& url);
    Q_INVOKABLE bool saveProject(const QUrl& url);
    Q_INVOKABLE bool loadProject(const QUrl& url);
    Q_INVOKABLE bool exportImage(const QUrl& url);
    Q_INVOKABLE void resetAdjustments();
    Q_INVOKABLE void rotateClockwise();
    Q_INVOKABLE void rotateCounterClockwise();
    Q_INVOKABLE void flipHorizontal();
    Q_INVOKABLE void flipVertical();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    // M5
    Q_INVOKABLE void paintMaskStroke(double x, double y, double radius, bool erase);
    Q_INVOKABLE void clearMask();
    // M6
    Q_INVOKABLE void requestAiMask(double x, double y);
    // M7
    Q_INVOKABLE void applyInpaint();
    Q_INVOKABLE void applyUpscale();
    // M8
    Q_INVOKABLE void addImageLayer(const QUrl& url);
    Q_INVOKABLE void deleteLayer(const QString& id);
    Q_INVOKABLE void setLayerOpacity(const QString& id, double opacity);
    Q_INVOKABLE void setLayerVisible(const QString& id, bool visible);
    Q_INVOKABLE void exportBatch(const QUrl& directory, const QStringList& formats);
    // M10
    Q_INVOKABLE void recoverProject();
    Q_INVOKABLE void discardRecovery();
signals:
    void documentChanged();
    void previewChanged();
    void adjustmentsChanged();
    void historyChanged();
    void operationFailed(QString message);
    void activeToolChanged();
    void maskChanged();
    void aiBusyChanged();
    void aiStatusChanged();
    void layersChanged();
    void recoveryChanged();
private:
    void rebuildPreview();
    void setAdjustment(lumen::AdjustmentType type, double value);
    [[nodiscard]] QString localPath(const QUrl& url) const;
    void saveMaskToTemp();
    void setAiBusy(bool busy);
    void setAiStatus(const QString& status);
    void autoSave();
    void checkRecovery();
    QString autosavePath() const;
    lumen::DocumentModel   m_document;
    lumen::RenderPipeline  m_renderPipeline;
    lumen::ExportService   m_exportService;
    lumen::ProjectStore    m_projectStore;
    lumen::AiRuntime       m_aiRuntime;
    lumen::InpaintEngine   m_inpaintEngine;
    lumen::UpscaleEngine   m_upscaleEngine;
    QString  m_previewPath;
    int      m_previewVersion    = 0;
    bool     m_showOriginal      = false;
    QFutureWatcher<QImage>* m_previewWatcher = nullptr;
    bool     m_previewPending    = false;
    int      m_previewRequestId  = 0;
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
    // M5
    int      m_activeTool        = 0;
    std::unique_ptr<lumen::BrushEngine> m_brushEngine;
    QString  m_maskTempPath;
    int      m_maskVersion       = 0;
    // M6/M7
    bool     m_aiBusy            = false;
    QString  m_aiStatus;
    // M10
    QTimer*  m_autosaveTimer     = nullptr;
    bool     m_hasPendingRecovery = false;
};
```


---

## TASK 29 — REPLACE `app/src/editor/DocumentController.cpp`

```cpp
#include "editor/DocumentController.hpp"
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QVariantMap>
#include <QtConcurrent>
DocumentController::DocumentController(QObject* parent)
    : QObject(parent)
    , m_cancelFlag(std::make_shared<std::atomic<bool>>(false))
{
    connect(&m_document, &lumen::DocumentModel::changed, this, [this]() {
        rebuildPreview();
        emit documentChanged();
        emit adjustmentsChanged();
        emit layersChanged();
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
// ─── Core properties ───────────────────────────────────────────────────────
bool DocumentController::hasDocument() const { return m_document.hasDocument(); }
QString DocumentController::sourceName() const
{
    return m_document.hasDocument()
        ? QFileInfo(m_document.sourcePath()).fileName()
        : "No image loaded";
}
QString DocumentController::imageUrl() const
{
    if (m_showOriginal && m_document.hasDocument())
        return QUrl::fromLocalFile(m_document.sourcePath()).toString();
    return m_previewPath.isEmpty() ? QString()
        : QUrl::fromLocalFile(m_previewPath).toString();
}
bool DocumentController::canUndo()      const { return m_document.canUndo(); }
bool DocumentController::canRedo()      const { return m_document.canRedo(); }
bool DocumentController::showOriginal() const { return m_showOriginal; }
void DocumentController::setShowOriginal(bool v)
{ if (m_showOriginal == v) return; m_showOriginal = v; emit previewChanged(); }
// ─── Adjustments ──────────────────────────────────────────────────────────
#define ADJ_GET(Name, Type) \\
    double DocumentController::Name() const \\
    { return m_document.scalarAdjustment(lumen::AdjustmentType::Type); }
#define ADJ_SET(Name, Type) \\
    void DocumentController::set##Name(double v) \\
    { setAdjustment(lumen::AdjustmentType::Type, v); }
#define ADJ(Name, Type) ADJ_GET(Name, Type) ADJ_SET(Name, Type)
ADJ(exposure, Exposure)   ADJ(contrast, Contrast)
ADJ(saturation, Saturation) ADJ(highlights, Highlights)
ADJ(shadows, Shadows)     ADJ(whites, Whites)
ADJ(blacks, Blacks)       ADJ(vibrance, Vibrance)
ADJ(temperature, Temperature) ADJ(tint, Tint)
ADJ(noiseReduction, NoiseReduction) ADJ(sharpening, Sharpening)
// ─── M5 mask properties ───────────────────────────────────────────────────
int  DocumentController::activeTool()  const { return m_activeTool; }
bool DocumentController::hasMask()     const { return !m_document.activeMask().isNull(); }
QString DocumentController::maskUrl()  const { return m_maskTempPath.isEmpty() ? QString()
    : QUrl::fromLocalFile(m_maskTempPath).toString(); }
int DocumentController::sourceWidth()  const { return m_document.sourceSize().width(); }
int DocumentController::sourceHeight() const { return m_document.sourceSize().height(); }
void DocumentController::setActiveTool(int tool)
{ if (m_activeTool == tool) return; m_activeTool = tool; emit activeToolChanged(); }
// ─── M6/M7 AI properties ──────────────────────────────────────────────────
bool    DocumentController::aiBusy()    const { return m_aiBusy; }
QString DocumentController::aiStatus()  const { return m_aiStatus; }
void DocumentController::setAiBusy(bool busy)
{ if (m_aiBusy == busy) return; m_aiBusy = busy; emit aiBusyChanged(); }
void DocumentController::setAiStatus(const QString& s)
{ m_aiStatus = s; emit aiStatusChanged(); }
// ─── M8 layer model ───────────────────────────────────────────────────────
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
        list.prepend(m); // topmost layer first
    }
    return list;
}
// ─── M10 recovery ─────────────────────────────────────────────────────────
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
// ─── Invokables ───────────────────────────────────────────────────────────
bool DocumentController::openImage(const QUrl& url)
{
    if (!m_document.openSourceImage(localPath(url))) {
        emit operationFailed("Could not open image."); return false;
    }
    // Reset brush engine to new image size
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
        emit operationFailed("Could not save project."); return false;
    }
    return true;
}
bool DocumentController::loadProject(const QUrl& url)
{
    if (!m_projectStore.loadProject(m_document, localPath(url))) {
        emit operationFailed("Could not load project."); return false;
    }
    m_brushEngine = std::make_unique<lumen::BrushEngine>(m_document.sourceSize());
    return true;
}
bool DocumentController::exportImage(const QUrl& url)
{
    if (!m_exportService.exportImage(m_document, localPath(url))) {
        emit operationFailed("Could not export image."); return false;
    }
    return true;
}
void DocumentController::resetAdjustments()
{
    for (auto type : {
        lumen::AdjustmentType::Exposure,    lumen::AdjustmentType::Contrast,
        lumen::AdjustmentType::Saturation,  lumen::AdjustmentType::Highlights,
        lumen::AdjustmentType::Shadows,     lumen::AdjustmentType::Whites,
        lumen::AdjustmentType::Blacks,      lumen::AdjustmentType::Vibrance,
        lumen::AdjustmentType::Temperature, lumen::AdjustmentType::Tint,
        lumen::AdjustmentType::NoiseReduction, lumen::AdjustmentType::Sharpening,
    }) setAdjustment(type, 0.0);
}
void DocumentController::rotateClockwise()       { m_document.rotateClockwise(); }
void DocumentController::rotateCounterClockwise(){ m_document.rotateCounterClockwise(); }
void DocumentController::flipHorizontal()        { m_document.flipHorizontal(); }
void DocumentController::flipVertical()          { m_document.flipVertical(); }
void DocumentController::undo()                  { m_document.undo(); }
void DocumentController::redo()                  { m_document.redo(); }
// M5
void DocumentController::paintMaskStroke(double x, double y, double radius, bool erase)
{
    if (!m_document.hasDocument() || !m_brushEngine) return;
    m_brushEngine->paintStroke(QPointF(x, y), radius, 0.85, erase);
    m_document.setActiveMask(m_brushEngine->mask());
    saveMaskToTemp();
    emit maskChanged();
}
void DocumentController::clearMask()
{
    if (m_brushEngine) m_brushEngine->clear();
    m_document.setActiveMask(QImage());
    m_maskTempPath.clear();
    emit maskChanged();
}
void DocumentController::saveMaskToTemp()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QString("/lumenforge-mask-%1.png").arg(++m_maskVersion);
    if (m_document.activeMask().save(path)) {
        if (!m_maskTempPath.isEmpty()) QFile::remove(m_maskTempPath);
        m_maskTempPath = path;
    }
}
// M6
void DocumentController::requestAiMask(double x, double y)
{
    if (!m_document.hasDocument() || m_aiBusy) return;
    setAiStatus("Loading model…");
    const QImage src = m_document.sourceImage();
    const QPointF pt(x, y);
    m_aiRuntime.predictMask(src, pt, [this](QImage result) {
        if (!result.isNull()) {
            if (!m_brushEngine)
                m_brushEngine = std::make_unique<lumen::BrushEngine>(m_document.sourceSize());
            m_brushEngine->mask() = result;
            m_document.setActiveMask(result);
            saveMaskToTemp();
            emit maskChanged();
        }
        setAiStatus("Done");
    });
}
// M7
void DocumentController::applyInpaint()
{
    if (!m_document.hasDocument() || m_document.activeMask().isNull() || m_aiBusy) return;
    setAiBusy(true);
    setAiStatus("Loading model…");
    const QImage src  = m_document.sourceImage();
    const QImage mask = m_document.activeMask();
    auto* w = new QFutureWatcher<QImage>(this);
    connect(w, &QFutureWatcher<QImage>::finished, this, [this, w]() {
        m_document.replaceSourceImage(w->result());
        if (m_brushEngine) m_brushEngine->resize(m_document.sourceSize());
        setAiStatus("Done");
        setAiBusy(false);
        rebuildPreview();
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([this, src, mask]() mutable -> QImage {
        QMetaObject::invokeMethod(this, [this]{ setAiStatus("Running inference…"); });
        return m_inpaintEngine.inpaint(src, mask);
    }));
}
void DocumentController::applyUpscale()
{
    if (!m_document.hasDocument() || m_aiBusy) return;
    setAiBusy(true);
    setAiStatus("Loading model…");
    const QImage src = m_document.sourceImage();
    auto* w = new QFutureWatcher<QImage>(this);
    connect(w, &QFutureWatcher<QImage>::finished, this, [this, w]() {
        m_document.replaceSourceImage(w->result());
        if (m_brushEngine) m_brushEngine->resize(m_document.sourceSize());
        setAiStatus("Done");
        setAiBusy(false);
        rebuildPreview();
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([this, src]() mutable -> QImage {
        QMetaObject::invokeMethod(this, [this]{ setAiStatus("Running inference…"); });
        return m_upscaleEngine.upscale(src);
    }));
}
// M8
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
// ─── Private ──────────────────────────────────────────────────────────────
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
                + QString("/lumenforge-preview-%1.png").arg(++m_previewVersion);
            if (preview.save(path)) { m_previewPath = path; emit previewChanged(); }
        }
        watcher->deleteLater();
        m_previewWatcher = nullptr;
        if (m_previewPending) { m_previewPending = false; rebuildPreview(); }
    });
    watcher->setFuture(QtConcurrent::run(
        [pipeline = m_renderPipeline, src, adjs, mask, cancelled]() {
            return pipeline.renderPreviewFromData(src, adjs, QSize(1800, 1400), mask, cancelled);
        }));
}
void DocumentController::setAdjustment(lumen::AdjustmentType type, double value)
{
    if (qFuzzyCompare(m_document.scalarAdjustment(type), value)) return;
    m_document.setScalarAdjustment(type, value);
}
QString DocumentController::localPath(const QUrl& url) const
{ return url.isLocalFile() ? url.toLocalFile() : url.toString(); }
```


---

## TASK 30 — REPLACE `app/resources/qml/MaskCanvas.qml`

```qml
import QtQuick
import QtQuick.Controls
Canvas {
    id: maskCanvas
    property var   docCtrl:        null
    property double brushRadius:   50
    property bool   eraseMode:     false
    property bool   paintEnabled:  false
    implicitWidth:  200
    implicitHeight: 200
    Connections {
        target: docCtrl
        function onMaskChanged() { maskCanvas.requestPaint() }
    }
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        if (!docCtrl || !docCtrl.hasDocument || !docCtrl.hasMask) return;
        const url = docCtrl.maskUrl;
        if (!url || url.length === 0) return;
        ctx.globalAlpha = 0.45;
        ctx.drawImage(url, 0, 0, width, height);
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        property bool dragging: false
        onPressed:  (mouse) => { if (paintEnabled) { dragging = true;  stroke(mouse.x, mouse.y) } }
        onReleased:            { dragging = false }
        onExited:              { dragging = false }
        onPositionChanged: (mouse) => { if (dragging && paintEnabled) stroke(mouse.x, mouse.y) }
        function stroke(x, y) {
            const sw = docCtrl.sourceWidth  || maskCanvas.width
            const sh = docCtrl.sourceHeight || maskCanvas.height
            docCtrl.paintMaskStroke(
                x / maskCanvas.width  * sw,
                y / maskCanvas.height * sh,
                brushRadius, eraseMode)
        }
    }
}
```


---

## TASK 31 — REPLACE `app/resources/qml/Main.qml`

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
ApplicationWindow {
    id: root
    width: 1440
    height: 920
    visible: true
    title: "LumenForge"
    color: "#15171b"
    property real zoom: 1.0
    property real brushRadius: 50
    function fitZoom() {
        if (imagePreview.sourceSize.width <= 0) return 1.0
        return Math.min(
            canvasFlick.width  / imagePreview.sourceSize.width,
            canvasFlick.height / imagePreview.sourceSize.height) * 0.95
    }
    // ── Dialogs ────────────────────────────────────────────────────────────
    FileDialog {
        id: openImageDialog
        title: "Open image"
        nameFilters: [
            "Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp " +
            "*.cr2 *.cr3 *.nef *.arw *.dng *.raf *.orf *.rw2)",
            "All files (*)"
        ]
        onAccepted: documentController.openImage(selectedFile)
    }
    FileDialog {
        id: openProjectDialog; title: "Open project"
        nameFilters: ["LumenForge project (*.lfproj)"]
        onAccepted: documentController.loadProject(selectedFile)
    }
    FileDialog {
        id: saveProjectDialog; title: "Save project"
        fileMode: FileDialog.SaveFile; defaultSuffix: "lfproj"
        nameFilters: ["LumenForge project (*.lfproj)"]
        onAccepted: documentController.saveProject(selectedFile)
    }
    FileDialog {
        id: exportDialog; title: "Export image"
        fileMode: FileDialog.SaveFile; defaultSuffix: "png"
        nameFilters: ["PNG (*.png)", "JPEG (*.jpg)", "WebP (*.webp)"]
        onAccepted: documentController.exportImage(selectedFile)
    }
    FileDialog {
        id: addLayerDialog; title: "Add image layer"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp)"]
        onAccepted: documentController.addImageLayer(selectedFile)
    }
    // ── Recovery dialog ────────────────────────────────────────────────────
    Dialog {
        id: recoveryDialog
        title: "Recover unsaved work?"
        modal: true
        visible: documentController.hasPendingRecovery
        anchors.centerIn: parent
        Label { text: "An autosaved project was found. Recover it?" }
        footer: DialogButtonBox {
            Button { text: "Recover"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: { documentController.recoverProject(); recoveryDialog.close() } }
            Button { text: "Discard"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: { documentController.discardRecovery(); recoveryDialog.close() } }
        }
    }
    // ── Shortcuts ──────────────────────────────────────────────────────────
    Shortcut { sequence: StandardKey.Open;  onActivated: openImageDialog.open() }
    Shortcut { sequence: StandardKey.Save;  onActivated: saveProjectDialog.open() }
    Shortcut { sequence: "Ctrl+E";          onActivated: exportDialog.open() }
    Shortcut { sequence: "Ctrl+0";          onActivated: root.zoom = 1.0 }
    Shortcut { sequence: "Ctrl++";          onActivated: root.zoom = Math.min(4.0, root.zoom+0.1) }
    Shortcut { sequence: "Ctrl+-";          onActivated: root.zoom = Math.max(0.1, root.zoom-0.1) }
    Shortcut { sequence: StandardKey.Undo;  onActivated: documentController.undo() }
    Shortcut { sequence: StandardKey.Redo;  onActivated: documentController.redo() }
    Shortcut { sequence: "\\\\";             onActivated: documentController.showOriginal = !documentController.showOriginal }
    Shortcut { sequence: "Escape";
        onActivated: if (documentController.activeTool > 0) documentController.activeTool = 0 }
    // ── Header ─────────────────────────────────────────────────────────────
    header: ToolBar {
        height: 52
        background: Rectangle { color: "#1e2228" }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14; spacing: 10
            Label { text: "LumenForge"; color: "#f4f7fb"; font.pixelSize: 18; font.bold: true }
            Label { text: documentController.sourceName; color: "#98a2b3"
                elide: Text.ElideMiddle; Layout.fillWidth: true }
            Button { text: "Open";    onClicked: openImageDialog.open() }
            Button { text: "Project"; onClicked: openProjectDialog.open() }
            Button { text: "Save";    enabled: documentController.hasDocument
                onClicked: saveProjectDialog.open() }
            Button { text: "Export";  enabled: documentController.hasDocument
                onClicked: exportDialog.open() }
        }
    }
    // ── Footer — Layer panel ────────────────────────────────────────────────
    footer: Rectangle {
        height: 120
        color: "#1b1f25"
        border.color: "#2b313a"
        RowLayout {
            anchors.fill: parent; anchors.margins: 8; spacing: 8
            // Layer stack panel
            Rectangle {
                Layout.preferredWidth: 320; Layout.fillHeight: true
                color: "#20252c"; radius: 6; border.color: "#353c46"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 6; spacing: 4
                    RowLayout {
                        spacing: 6
                        Label { text: "Layers"; color: "#cbd5e1"; font.pixelSize: 12; font.bold: true
                            Layout.fillWidth: true }
                        Button { text: "+"; implicitWidth: 28; implicitHeight: 24
                            enabled: documentController.hasDocument
                            onClicked: addLayerDialog.open() }
                    }
                    ListView {
                        id: layerList
                        Layout.fillWidth: true; Layout.fillHeight: true
                        model: documentController.layerModel
                        clip: true
                        delegate: Rectangle {
                            width: layerList.width; height: 28
                            color: "transparent"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 4; spacing: 4
                                Button { text: modelData.visible ? "👁" : "○"
                                    implicitWidth: 24; implicitHeight: 22; flat: true
                                    onClicked: documentController.setLayerVisible(modelData.id, !modelData.visible) }
                                Label { text: modelData.name; color: "#cbd5e1"
                                    font.pixelSize: 11; Layout.fillWidth: true
                                    elide: Text.ElideRight }
                                Slider { from: 0; to: 1; value: modelData.opacity
                                    implicitWidth: 60; implicitHeight: 22
                                    onMoved: documentController.setLayerOpacity(modelData.id, value) }
                                Button { text: "✕"; implicitWidth: 24; implicitHeight: 22; flat: true
                                    onClicked: documentController.deleteLayer(modelData.id) }
                            }
                        }
                    }
                }
            }
            // History / Masks / Filmstrip placeholders
            Repeater {
                model: ["History", "Masks", "Filmstrip"]
                delegate: Rectangle {
                    Layout.preferredWidth: 160; Layout.fillHeight: true
                    radius: 6; color: "#20252c"; border.color: "#353c46"
                    Label { anchors.centerIn: parent; text: modelData
                        color: "#cbd5e1"; font.pixelSize: 12 }
                }
            }
            // AI status
            Label {
                text: documentController.aiStatus
                color: "#f59e0b"; font.pixelSize: 11
                visible: documentController.aiStatus.length > 0
                Layout.alignment: Qt.AlignVCenter
            }
            Item { Layout.fillWidth: true }
        }
    }
    // ── Main layout ────────────────────────────────────────────────────────
    RowLayout {
        anchors.fill: parent; spacing: 0
        // Tool rail
        Rectangle {
            Layout.preferredWidth: 72; Layout.fillHeight: true
            color: "#181b20"; border.color: "#272d35"
            ColumnLayout {
                anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 14; spacing: 6
                Repeater {
                    model: [
                        { icon: "M", tip: "Move",         tool: 0 },
                        { icon: "C", tip: "Crop",         tool: 0 },
                        { icon: "B", tip: "Brush mask",   tool: 1 },
                        { icon: "E", tip: "Erase mask",   tool: 2 },
                        { icon: "G", tip: "Gradient mask",tool: 0 },
                        { icon: "R", tip: "Radial mask",  tool: 0 }
                    ]
                    delegate: Button {
                        Layout.preferredWidth: 44; Layout.preferredHeight: 38
                        text: modelData.icon
                        checkable: modelData.tool > 0
                        checked: modelData.tool > 0 && documentController.activeTool === modelData.tool
                        ToolTip.visible: hovered; ToolTip.text: modelData.tip
                        onClicked: {
                            if (modelData.tool > 0)
                                documentController.activeTool =
                                    (documentController.activeTool === modelData.tool) ? 0 : modelData.tool
                        }
                    }
                }
                Rectangle { width: 44; height: 1; color: "#2b313a" }
                Label { text: "Size"; color: "#6b7280"; font.pixelSize: 9
                    Layout.alignment: Qt.AlignHCenter }
                Slider {
                    from: 5; to: 200; value: root.brushRadius
                    orientation: Qt.Vertical; implicitHeight: 80
                    Layout.alignment: Qt.AlignHCenter
                    onMoved: root.brushRadius = value
                }
            }
        }
        // Canvas area
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: "#101215"
            Flickable {
                id: canvasFlick; anchors.fill: parent
                contentWidth:  Math.max(width,  imagePreview.width)
                contentHeight: Math.max(height, imagePreview.height)
                clip: true
                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: (event) => {
                        const d = event.angleDelta.y > 0 ? 0.1 : -0.1
                        root.zoom = Math.min(4.0, Math.max(0.1, root.zoom + d))
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width:  Math.max(360, imagePreview.width  + 72)
                    height: Math.max(260, imagePreview.height + 72)
                    color: "#0c0e11"; border.color: "#2d333c"
                    Image {
                        id: imagePreview; anchors.centerIn: parent
                        source: documentController.imageUrl
                        cache: false; fillMode: Image.PreserveAspectFit
                        width:  sourceSize.width  > 0 ? sourceSize.width  * root.zoom : 0
                        height: sourceSize.height > 0 ? sourceSize.height * root.zoom : 0
                        asynchronous: true
                        onSourceSizeChanged: root.zoom = root.fitZoom()
                    }
                    MaskCanvas {
                        id: maskOverlay
                        anchors.centerIn: parent
                        width:  imagePreview.width
                        height: imagePreview.height
                        visible: documentController.activeTool > 0
                        docCtrl:      documentController
                        brushRadius:  root.brushRadius
                        eraseMode:    documentController.activeTool === 2
                        paintEnabled: documentController.activeTool > 0
                    }
                    BusyIndicator {
                        anchors.centerIn: parent
                        visible: documentController.aiBusy
                        running: documentController.aiBusy
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: !documentController.hasDocument
                        text: "Open an image to begin"
                        color: "#d0d5dd"; font.pixelSize: 22
                    }
                }
            }
            Row {
                anchors.left: parent.left; anchors.bottom: parent.bottom
                anchors.margins: 18; spacing: 8
                Button { text: "Fit";   enabled: documentController.hasDocument
                    onClicked: root.zoom = root.fitZoom() }
                Button { text: "100%";  enabled: documentController.hasDocument
                    onClicked: root.zoom = 1.0 }
                Button { text: "-";     enabled: documentController.hasDocument
                    onClicked: root.zoom = Math.max(0.1, root.zoom-0.1) }
                Button { text: "+";     enabled: documentController.hasDocument
                    onClicked: root.zoom = Math.min(4.0, root.zoom+0.1) }
                Button {
                    text: documentController.showOriginal ? "After" : "Before / After"
                    enabled: documentController.hasDocument
                    onClicked: documentController.showOriginal = !documentController.showOriginal
                }
                Button {
                    text: "Clear mask"
                    enabled: documentController.hasDocument && documentController.hasMask
                    onClicked: documentController.clearMask()
                }
            }
        }
        // Adjustments panel
        Rectangle {
            Layout.preferredWidth: 340; Layout.fillHeight: true
            color: "#1a1e24"; border.color: "#2c333d"
            ScrollView {
                anchors.fill: parent
                ColumnLayout {
                    width: parent.width; spacing: 14; anchors.margins: 18
                    Label { text: "Adjustments"; color: "#f2f4f7"
                        font.pixelSize: 18; font.bold: true
                        Layout.leftMargin: 18; Layout.topMargin: 18 }
                    Label { text: "Transform"; color: "#f2f4f7"
                        font.pixelSize: 14; font.bold: true; Layout.leftMargin: 18 }
                    GridLayout {
                        Layout.leftMargin: 18; Layout.rightMargin: 18
                        Layout.fillWidth: true; columns: 2; rowSpacing: 6; columnSpacing: 6
                        Button { text: "Rotate left";  enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.rotateCounterClockwise() }
                        Button { text: "Rotate right"; enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.rotateClockwise() }
                        Button { text: "Flip H"; enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.flipHorizontal() }
                        Button { text: "Flip V"; enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.flipVertical() }
                    }
                    RowLayout {
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true; spacing: 6
                        Button { text: "Undo"; enabled: documentController.canUndo
                            Layout.fillWidth: true; onClicked: documentController.undo() }
                        Button { text: "Redo"; enabled: documentController.canRedo
                            Layout.fillWidth: true; onClicked: documentController.redo() }
                    }
                    AdjustmentSlider { label: "Exposure";    from: -3;   to: 3
                        value: documentController.exposure;    onMoved: (v) => documentController.exposure = v }
                    AdjustmentSlider { label: "Contrast";    from: -100; to: 100
                        value: documentController.contrast;    onMoved: (v) => documentController.contrast = v }
                    AdjustmentSlider { label: "Saturation";  from: -100; to: 100
                        value: documentController.saturation;  onMoved: (v) => documentController.saturation = v }
                    AdjustmentSlider { label: "Highlights";  from: -100; to: 100
                        value: documentController.highlights;  onMoved: (v) => documentController.highlights = v }
                    AdjustmentSlider { label: "Shadows";     from: -100; to: 100
                        value: documentController.shadows;     onMoved: (v) => documentController.shadows = v }
                    AdjustmentSlider { label: "Whites";      from: -100; to: 100
                        value: documentController.whites;      onMoved: (v) => documentController.whites = v }
                    AdjustmentSlider { label: "Blacks";      from: -100; to: 100
                        value: documentController.blacks;      onMoved: (v) => documentController.blacks = v }
                    AdjustmentSlider { label: "Vibrance";    from: -100; to: 100
                        value: documentController.vibrance;    onMoved: (v) => documentController.vibrance = v }
                    AdjustmentSlider { label: "Temperature"; from: -100; to: 100
                        value: documentController.temperature; onMoved: (v) => documentController.temperature = v }
                    AdjustmentSlider { label: "Tint";        from: -100; to: 100
                        value: documentController.tint;        onMoved: (v) => documentController.tint = v }
                    Label { text: "Detail"; color: "#f2f4f7"
                        font.pixelSize: 14; font.bold: true; Layout.leftMargin: 18 }
                    AdjustmentSlider { label: "Noise Reduction"; from: 0; to: 100
                        value: documentController.noiseReduction
                        onMoved: (v) => documentController.noiseReduction = v }
                    AdjustmentSlider { label: "Sharpening"; from: 0; to: 100
                        value: documentController.sharpening
                        onMoved: (v) => documentController.sharpening = v }
                    Button {
                        text: "Reset all"
                        enabled: documentController.hasDocument
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.resetAdjustments()
                    }
                    Label { text: "AI tools"; color: "#f2f4f7"
                        font.pixelSize: 14; font.bold: true
                        Layout.leftMargin: 18; Layout.topMargin: 8 }
                    Button {
                        text: "Subject mask"
                        enabled: documentController.hasDocument && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.requestAiMask(
                            imagePreview.width / 2, imagePreview.height / 2)
                    }
                    Button {
                        text: "Background mask"
                        enabled: documentController.hasDocument && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: {
                            documentController.requestAiMask(
                                imagePreview.width / 2, imagePreview.height / 2)
                            // invert after — TODO: expose invertMask() invokable
                        }
                    }
                    Button {
                        text: "Object removal"
                        enabled: documentController.hasDocument && documentController.hasMask
                               && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.applyInpaint()
                    }
                    Button {
                        text: "Upscale ×4"
                        enabled: documentController.hasDocument && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.applyUpscale()
                    }
                    Item { Layout.preferredHeight: 24 }
                }
            }
        }
    }
    // ── Inline component ──────────────────────────────────────────────────
    component AdjustmentSlider: ColumnLayout {
        id: sliderRoot
        property string label: ""
        property real   from:  0
        property real   to:    1
        property real   value: 0
        signal moved(real nextValue)
        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true; spacing: 4
        RowLayout {
            Layout.fillWidth: true
            Label { text: sliderRoot.label; color: "#d0d5dd"; Layout.fillWidth: true }
            Label { text: Number(slider.value).toFixed(sliderRoot.to <= 3 ? 2 : 0)
                color: "#98a2b3" }
        }
        Slider {
            id: slider; Layout.fillWidth: true
            from: sliderRoot.from; to: sliderRoot.to; value: sliderRoot.value
            enabled: documentController.hasDocument
            onMoved: sliderRoot.moved(value)
        }
    }
}
```


---

## TASK 32 — REPLACE `app/CMakeLists.txt`

```cmake
qt_add_executable(LumenForge
    src/main/main.cpp
    src/editor/DocumentController.cpp
    src/editor/DocumentController.hpp
)
qt_add_qml_module(LumenForge
    URI LumenForge
    VERSION 1.0
    QML_FILES
        resources/qml/Main.qml
        resources/qml/MaskCanvas.qml
)
target_link_libraries(LumenForge
    PRIVATE
        lumen_core
        Qt6::Core
        Qt6::Gui
        Qt6::Concurrent
        Qt6::Quick
        Qt6::QuickControls2
)
target_include_directories(LumenForge
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(LumenForge PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
install(TARGETS LumenForge
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)
# windeployqt — copies Qt DLLs next to the exe on Windows
find_program(WINDEPLOYQT windeployqt
    HINTS "${Qt6_DIR}/../../../bin"
          "$ENV{QTDIR}/bin")
if(WINDEPLOYQT AND WIN32)
    add_custom_command(TARGET LumenForge POST_BUILD
        COMMAND ${WINDEPLOYQT}
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/resources/qml"
            $<TARGET_FILE:LumenForge>
        COMMENT "Running windeployqt…"
        VERBATIM)
endif()
```


---

## TASK 33 — REPLACE `app/src/main/main.cpp`

```cpp
#include "editor/DocumentController.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("LumenForge");
    QGuiApplication::setOrganizationName("LumenForge");
    QGuiApplication::setApplicationVersion("0.1.0");
    QQuickStyle::setStyle("Fusion");
    DocumentController documentController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("documentController", &documentController);
    engine.loadFromModule("LumenForge", "Main");
    if (engine.rootObjects().isEmpty()) return -1;
    return QGuiApplication::exec();
}
```


---

## TASK 34 — REPLACE `CMakeLists.txt` (root)

```cmake
cmake_minimum_required(VERSION 3.24)
project(LumenForge
    VERSION 0.1.0
    DESCRIPTION "Offline-first native desktop photo editor"
    LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Qt6 6.5 REQUIRED
    COMPONENTS Core Gui Quick QuickControls2 Sql Concurrent)
qt_standard_project_setup(REQUIRES 6.5)
add_subdirectory(core)
add_subdirectory(app)
# ── CPack / WiX installer (Windows) ────────────────────────────────────────
set(CPACK_PACKAGE_NAME        "LumenForge")
set(CPACK_PACKAGE_VENDOR      "LumenForge")
set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION     "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
if(WIN32)
    set(CPACK_GENERATOR "WIX")
    set(CPACK_WIX_UPGRADE_GUID "A1B2C3D4-E5F6-7890-ABCD-EF1234567890")
    if(EXISTS "${CMAKE_SOURCE_DIR}/app/resources/icon.ico")
        set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/app/resources/icon.ico")
    endif()
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
endif()
include(CPack)
```


---

## TASK 35 — CREATE `LICENSE.txt`

```
MIT License
Copyright (c) 2024 LumenForge
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```


---

## TASK 36 — CREATE `models/.gitkeep`

```
```

(Empty file — ensures the `models/` directory is tracked by git even though `*.onnx` files are ignored.)

---

That is the complete task list — **35 files** to create or replace covering all six milestones. Execute them top to bottom. A few notes before you hand this to your agent:

**Build order for external dependencies** — before the first cmake configure after Task 27, run `vcpkg install onnxruntime libraw lcms2 opencv4` and pass `-DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake`. Each library is optional at compile time (guarded by `HAVE_*` defines), so the app builds and runs without them — AI features just won't function until the libs and model weights are present.

**Model weights** — after Task 36, manually download `mobile_sam.onnx`, `big-lama.onnx`, and `realesrgan-x4plus.onnx` into `models/`. Nothing in the build system fetches them.

**`app/resources/icon.ico`** — Task 34 references it for the installer. Create a placeholder `.ico` file or skip the CPack step until you have one; the app itself builds without it.

