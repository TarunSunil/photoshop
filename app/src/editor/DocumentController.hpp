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
#include <QStringList>
#include <atomic>
#include <memory>
class DocumentController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY documentChanged)
    Q_PROPERTY(QString imageUrl READ imageUrl NOTIFY previewChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(bool showOriginal READ showOriginal WRITE setShowOriginal NOTIFY previewChanged)
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
    Q_PROPERTY(int    activeTool  READ activeTool  WRITE setActiveTool  NOTIFY activeToolChanged)
    Q_PROPERTY(bool   hasMask     READ hasMask     NOTIFY maskChanged)
    Q_PROPERTY(QString maskUrl    READ maskUrl     NOTIFY maskChanged)
    Q_PROPERTY(int sourceWidth    READ sourceWidth  NOTIFY documentChanged)
    Q_PROPERTY(int sourceHeight   READ sourceHeight NOTIFY documentChanged)
    Q_PROPERTY(bool   aiBusy     READ aiBusy      NOTIFY aiBusyChanged)
    Q_PROPERTY(QString aiStatus  READ aiStatus    NOTIFY aiStatusChanged)
    Q_PROPERTY(QVariantList layerModel READ layerModel NOTIFY layersChanged)
    Q_PROPERTY(bool hasPendingRecovery READ hasPendingRecovery NOTIFY recoveryChanged)
    // Crop overlay state exposed to QML
    Q_PROPERTY(bool cropActive READ cropActive NOTIFY cropActiveChanged)
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
    [[nodiscard]] bool cropActive()      const;
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
    // Mask tools
    Q_INVOKABLE void paintMaskStroke(double x, double y, double radius, bool erase);
    Q_INVOKABLE void commitMaskPaint();
    Q_INVOKABLE void clearMask();
    // Gradient mask: linear gradient from (x1,y1) white to (x2,y2) transparent
    Q_INVOKABLE void applyGradientMask(double x1, double y1, double x2, double y2);
    // Radial mask: circle centered at (cx,cy) with given radius, white inside
    Q_INVOKABLE void applyRadialMask(double cx, double cy, double radius);
    // Crop: coordinates in source image space
    Q_INVOKABLE void applyCrop(int x, int y, int w, int h);
    // AI
    Q_INVOKABLE void requestAiMask(double x, double y);
    Q_INVOKABLE void applyInpaint();
    Q_INVOKABLE void applyUpscale();
    // Layers
    Q_INVOKABLE void addImageLayer(const QUrl& url);
    Q_INVOKABLE void deleteLayer(const QString& id);
    Q_INVOKABLE void setLayerOpacity(const QString& id, double opacity);
    Q_INVOKABLE void setLayerVisible(const QString& id, bool visible);
    Q_INVOKABLE void exportBatch(const QUrl& directory, const QStringList& formats);
    // Recovery
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
    void cropActiveChanged();
private:
    void rebuildPreview();
    void setAdjustment(lumen::AdjustmentType type, double value);
    [[nodiscard]] QString localPath(const QUrl& url) const;
    void saveMaskToTemp();
    void flushMaskSave();    // called by m_maskSaveTimer to batch mask PNG saves
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
    // Held as unique_ptr so the InpaintEngine / UpscaleEngine constructors
    // (which create an Ort::Env and load the ONNX Runtime) are NOT called at
    // DocumentController construction time.  Previously these were value
    // members, meaning two Ort::Env objects were created before main() even
    // reached QGuiApplication::exec() — if onnxruntime.dll or any transitive
    // dependency failed to load the process terminated silently.  With
    // unique_ptr the engines are created on first AI use; by then we are inside
    // the Qt event loop and can show an error message rather than crashing.
    std::unique_ptr<lumen::InpaintEngine>  m_inpaintEngine;
    std::unique_ptr<lumen::UpscaleEngine>  m_upscaleEngine;
    QString  m_previewPath;
    int      m_previewVersion    = 0;
    bool     m_showOriginal      = false;
    QFutureWatcher<QImage>* m_previewWatcher = nullptr;
    bool     m_previewPending    = false;
    int      m_previewRequestId  = 0;
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
    int      m_activeTool        = 0;
    std::unique_ptr<lumen::BrushEngine> m_brushEngine;
    QString  m_maskTempPath;
    int      m_maskVersion       = 0;
    bool     m_aiBusy            = false;
    QString  m_aiStatus;
    QTimer*  m_autosaveTimer     = nullptr;
    QTimer*  m_previewDebounce   = nullptr;
    QTimer*  m_maskSaveTimer     = nullptr;  // 50ms debounce for mask PNG saves
    bool     m_hasPendingRecovery = false;
    bool     m_cropActive        = false;
};