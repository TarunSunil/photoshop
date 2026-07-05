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
#include <QHash>
#include <QImage>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
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
    Q_PROPERTY(double brightness  READ brightness  WRITE setBrightness  NOTIFY adjustmentsChanged)
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
    Q_PROPERTY(QVariantList layerModel  READ layerModel  NOTIFY layersChanged)
    Q_PROPERTY(QStringList  historyLog  READ historyLog  NOTIFY historyLogChanged)
    Q_PROPERTY(QVariantList maskList    READ maskList    NOTIFY maskChanged)
    Q_PROPERTY(QStringList  recentFiles READ recentFiles NOTIFY recentFilesChanged)
    Q_PROPERTY(bool hasPendingRecovery READ hasPendingRecovery NOTIFY recoveryChanged)
    Q_PROPERTY(bool cropActive READ cropActive NOTIFY cropActiveChanged)
    // Issue 5: which target ("" = full image, else a mask id) the sliders edit.
    Q_PROPERTY(QString activeAdjustmentTarget READ activeAdjustmentTarget
               WRITE setActiveAdjustmentTarget NOTIFY activeAdjustmentTargetChanged)
    // Issue 5: list of selectable targets for the Masks tab combo (Full Image + each mask)
    Q_PROPERTY(QVariantList adjustmentTargets READ adjustmentTargets NOTIFY maskChanged)
    // Issue 6: currently selected overlay layer id for transform editing ("" = none)
    Q_PROPERTY(QString selectedLayerId READ selectedLayerId WRITE setSelectedLayerId NOTIFY selectedLayerChanged)
public:
    explicit DocumentController(QObject* parent = nullptr);

    [[nodiscard]] bool    hasDocument()  const;
    [[nodiscard]] QString sourceName()   const;
    [[nodiscard]] QString imageUrl()     const;
    [[nodiscard]] bool    canUndo()      const;
    [[nodiscard]] bool    canRedo()      const;
    [[nodiscard]] bool    showOriginal() const;
    void setShowOriginal(bool v);

    // These now read/write through the CURRENT activeAdjustmentTarget (issue 5).
    [[nodiscard]] double brightness()    const;  void setBrightness(double v);
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

    [[nodiscard]] int     activeTool()   const;  void setActiveTool(int tool);
    [[nodiscard]] bool    hasMask()      const;
    [[nodiscard]] QString maskUrl()      const;
    [[nodiscard]] int     sourceWidth()  const;
    [[nodiscard]] int     sourceHeight() const;
    [[nodiscard]] bool    aiBusy()       const;
    [[nodiscard]] QString aiStatus()     const;
    [[nodiscard]] QVariantList layerModel()  const;
    [[nodiscard]] QStringList  historyLog()  const;
    [[nodiscard]] QVariantList maskList()    const;
    [[nodiscard]] QStringList  recentFiles() const;
    [[nodiscard]] bool    hasPendingRecovery() const;
    [[nodiscard]] bool    cropActive()   const;

    // Issue 5
    [[nodiscard]] QString activeAdjustmentTarget() const;
    void setActiveAdjustmentTarget(const QString& targetMaskId);
    [[nodiscard]] QVariantList adjustmentTargets() const;

    // Issue 6
    [[nodiscard]] QString selectedLayerId() const;
    void setSelectedLayerId(const QString& id);

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
    Q_INVOKABLE void applyGradientMask(double x1, double y1, double x2, double y2);
    Q_INVOKABLE void applyRadialMask(double cx, double cy, double radius);
    Q_INVOKABLE void applyCrop(int x, int y, int w, int h);
    Q_INVOKABLE void refineEdges();
    // Issue 5: create a brand-new empty mask slot and switch the editing
    // target to it. Sliders read 0 for a target with no adjustments yet,
    // so this alone satisfies "sliders reset for new mask".
    Q_INVOKABLE void addNewMaskTarget();
    // AI
    Q_INVOKABLE void requestAiMask(double x, double y);
    Q_INVOKABLE void applyInpaint();
    Q_INVOKABLE void applyUpscale();
    // Layers
    Q_INVOKABLE void addImageLayer(const QUrl& url);
    Q_INVOKABLE void deleteLayer(const QString& id);
    Q_INVOKABLE void setLayerOpacity(const QString& id, double opacity);
    Q_INVOKABLE void setLayerVisible(const QString& id, bool visible);
    Q_INVOKABLE void moveLayerUp(const QString& id);
    Q_INVOKABLE void moveLayerDown(const QString& id);
    // Issue 6: per-layer transform (position/scale/rotation)
    Q_INVOKABLE void setLayerTransform(const QString& id,
                                        double posX, double posY,
                                        double scaleX, double scaleY,
                                        double rotation);
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
    void historyLogChanged();
    void recentFilesChanged();
    void recoveryChanged();
    void cropActiveChanged();
    void activeAdjustmentTargetChanged();
    void selectedLayerChanged();

private:
    struct StrokePoint { double x, y, radius; bool erase; };

    void rebuildPreview();
    void buildHqPreview();
    // Issue 5: target-aware adjustment plumbing (replaces setAdjustment(type,value))
    void setAdjustment(lumen::AdjustmentType type, double value);
    void logHistory(const QString& label);
    void addRecentFile(const QString& path);
    [[nodiscard]] QString localPath(const QUrl& url) const;
    // Returns the mask id that paint tools (brush/gradient/radial/AI mask)
    // should write into. If no mask is currently selected (activeAdjustmentTarget
    // == "" / Full Image), creates a new mask and switches the active target to
    // it, so pressing the Brush tool always has somewhere real to paint instead
    // of silently writing into a fake/unregistered target.
    [[nodiscard]] QString ensurePaintTarget();
    // Reloads m_brushEngine's paint surface from the given mask's stored pixels
    // (or clears it if targetId is empty/has no mask yet) so switching which
    // mask is being edited doesn't paint new strokes on top of stale content
    // left over from whichever mask was active before.
    void syncBrushEngineToTarget(const QString& targetId);
    void saveMaskToTemp(const QString& maskId);
    void flushMaskSave();
    void setAiBusy(bool busy);
    void setAiStatus(const QString& status);
    void autoSave();
    void checkRecovery();
    QString autosavePath() const;
    // Builds the MaskAdjLayer vector for all masks that have local adjustments,
    // for use by renderWithLayers().
    [[nodiscard]] std::vector<lumen::MaskAdjLayer> buildMaskAdjLayers() const;

    lumen::DocumentModel   m_document;
    lumen::RenderPipeline  m_renderPipeline;
    lumen::ExportService   m_exportService;
    lumen::ProjectStore    m_projectStore;
    lumen::AiRuntime       m_aiRuntime;
    std::unique_ptr<lumen::InpaintEngine>  m_inpaintEngine;
    std::unique_ptr<lumen::UpscaleEngine>  m_upscaleEngine;

    QString  m_previewPath;
    int      m_previewVersion    = 0;
    bool     m_showOriginal      = false;
    QFutureWatcher<QImage>* m_previewWatcher  = nullptr;
    QFutureWatcher<QImage>* m_hqWatcher       = nullptr;
    bool     m_previewPending    = false;
    int      m_previewRequestId  = 0;
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
    std::shared_ptr<std::atomic<bool>> m_hqCancelFlag;

    QTimer*  m_hqTimer           = nullptr;
    int      m_activeTool        = 0;
    std::unique_ptr<lumen::BrushEngine> m_brushEngine;
    QVector<StrokePoint> m_pendingStrokes;
    QHash<QString, QString> m_maskTempPaths;   // maskId -> temp PNG preview path
    int      m_maskVersion       = 0;
    bool     m_aiBusy            = false;
    QString  m_aiStatus;
    QTimer*  m_autosaveTimer     = nullptr;
    QTimer*  m_previewDebounce   = nullptr;
    QTimer*  m_maskSaveTimer     = nullptr;
    bool     m_hasPendingRecovery = false;
    bool     m_cropActive        = false;
    QStringList m_historyLog;

    // Issue 5
    QString  m_activeAdjustmentTarget;   // "" = Full Image, else mask id
    // Issue 6
    QString  m_selectedLayerId;
};