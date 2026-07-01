#pragma once
#include "shared-types/Adjustment.hpp"
#include "shared-types/Layer.hpp"
#include "shared-types/Mask.hpp"
#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>
namespace lumen {
class DocumentModel final : public QObject {
    Q_OBJECT
public:
    explicit DocumentModel(QObject* parent = nullptr);

    bool openSourceImage(const QString& path);
    void replaceSourceImage(const QImage& newImage);
    void clear();

    [[nodiscard]] bool    hasDocument()  const;
    [[nodiscard]] QString sourcePath()   const;
    [[nodiscard]] QSize   sourceSize()   const;
    [[nodiscard]] const QImage& sourceImage() const;

    // ── Adjustments ───────────────────────────────────────────────────────────
    // Global (targetMaskId == "") and per-mask variants.
    // The old no-target overloads delegate to targetId="" for backward compat.
    [[nodiscard]] QVector<Adjustment> adjustments()             const;  // ALL
    [[nodiscard]] QVector<Adjustment> adjustmentsForLayer(const QString& layerId) const;

    // Issue 5: per-target (full-image vs per-mask) adjustment access
    [[nodiscard]] QVector<Adjustment> adjustmentsForTarget(const QString& targetMaskId) const;
    void   setScalarAdjustmentForTarget(AdjustmentType type, double value, const QString& targetMaskId);
    [[nodiscard]] double scalarAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId) const;

    // Legacy (targetMaskId = "")
    void   setScalarAdjustment(AdjustmentType type, double value);
    [[nodiscard]] double scalarAdjustment(AdjustmentType type) const;

    // ── Layers ────────────────────────────────────────────────────────────────
    [[nodiscard]] QVector<Layer> layers() const;
    [[nodiscard]] QVector<Mask>  masks()  const;
    [[nodiscard]] QImage         layerImage(const QString& layerId) const;

    [[nodiscard]] bool    canUndo()      const;
    [[nodiscard]] bool    canRedo()      const;
    [[nodiscard]] bool    isDownsampled() const;

    void setActiveMask(const QImage& mask);
    [[nodiscard]] const QImage& activeMask() const;

    void rotateClockwise();
    void rotateCounterClockwise();
    void flipHorizontal();
    void flipVertical();
    void undo();
    void redo();

    // Layer management
    void addImageLayer(const QString& path);
    void moveLayer(int fromIndex, int toIndex);
    void setLayerOpacity(const QString& id, double opacity);
    void setLayerVisible(const QString& id, bool visible);
    void setLayerBlendMode(const QString& id, BlendMode mode);
    void deleteLayer(const QString& id);

    // Issue 6: per-layer transform
    void setLayerTransform(const QString& id,
                           double posX, double posY,
                           double scaleX, double scaleY,
                           double rotation);

signals:
    void changed();
    void historyChanged();

private:
    // Returns the first adjustment matching type AND targetMaskId.
    Adjustment*       findAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId);
    const Adjustment* findAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId) const;
    // Legacy: only searches among targetMaskId=="" adjustments.
    Adjustment*       findAdjustment(AdjustmentType type);
    const Adjustment* findAdjustment(AdjustmentType type) const;
    Layer*            findLayer(const QString& id);
    void pushHistorySnapshot();
    void restoreAdjustments(const QVector<Adjustment>& adjustments);

    QString   m_projectId;
    QString   m_sourcePath;
    QImage    m_sourceImage;
    bool      m_isDownsampled = false;
    QVector<Layer>      m_layers;
    QVector<Mask>       m_masks;
    QVector<Adjustment> m_adjustments;
    QHash<QString, QImage> m_layerImages;
    QVector<QVector<Adjustment>>  m_undoStack;
    QVector<QVector<Adjustment>>  m_redoStack;
    QVector<QImage>               m_sourceImageHistory;
};
} // namespace lumen