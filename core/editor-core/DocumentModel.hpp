#pragma once
#include "shared-types/Adjustment.hpp"
#include "shared-types/Layer.hpp"
#include "shared-types/Mask.hpp"
#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>
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
    // label, if empty, is auto-generated from type+value (e.g.
    // "Exposure: 1.50") -- see the .cpp. Callers with a better description
    // (rotate/flip, "Reset all adjustments") can pass one explicitly.
    void   setScalarAdjustmentForTarget(AdjustmentType type, double value, const QString& targetMaskId, const QString& label = QString());
    [[nodiscard]] double scalarAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId) const;

    // Legacy (targetMaskId = "")
    void   setScalarAdjustment(AdjustmentType type, double value, const QString& label = QString());
    [[nodiscard]] double scalarAdjustment(AdjustmentType type) const;

    // ── Layers ────────────────────────────────────────────────────────────────
    [[nodiscard]] QVector<Layer> layers() const;
    [[nodiscard]] QVector<Mask>  masks()  const;
    [[nodiscard]] QImage         layerImage(const QString& layerId) const;

    [[nodiscard]] bool    canUndo()      const;
    [[nodiscard]] bool    canRedo()      const;
    [[nodiscard]] bool    isDownsampled() const;
    // The undo stack's entry labels, oldest first -- i.e. the exact
    // chronological sequence of actions that produced the CURRENT
    // document state. This is the actual source of truth for a
    // Photoshop-style History panel: undo() shortens this list (by
    // popping the most recent entry), redo() lengthens it again (by
    // pushing the same entry back with its original label) -- nothing
    // else needs to special-case undo/redo, since the panel is just a
    // live read of this list.
    [[nodiscard]] QStringList historyLabels() const;

    // ── History transactions ─────────────────────────────────────────────────
    // Replaces the old "one pushHistorySnapshot() call per mutation" design,
    // which meant dragging a slider pushed one undo step per intermediate
    // tick. Callers that know they're bracketing a single logical user
    // interaction (a whole slider drag, a crop, an AI operation) call
    // beginHistoryTransaction()/commitHistoryTransaction() around it, so the
    // whole interaction becomes exactly one undo step. Callers that don't
    // (rotate, flip, add layer, ...) get today's "one call = one undo step"
    // behavior automatically -- see the private AutoHistoryStep guard.
    //
    // structural=true additionally captures/restores sourceImage and masks,
    // not just adjustments -- required for crop/inpaint/upscale, which
    // previously weren't part of undo/redo AT ALL (replaceSourceImage()
    // never pushed any history). Plain adjustment edits stay cheap (no
    // image data captured) since structural defaults to false.
    void beginHistoryTransaction(const QString& label = QString(), bool structural = false);
    // finalLabel, if non-empty, overwrites the transaction's label right
    // before it's pushed onto the undo stack -- for callers whose best
    // description of the action is only known at the END of it (e.g.
    // applyCrop() knows the final pixel dimensions only after cropping),
    // rather than at beginHistoryTransaction() time.
    void commitHistoryTransaction(const QString& finalLabel = QString());
    // Restores document state to what it was when the currently-open
    // transaction began, without creating an undo entry. Not currently
    // wired to any UI action; available for a future "cancel mid-drag" or
    // Escape-key feature.
    void cancelHistoryTransaction();

    // ── Multi-mask management ────────────────────────────────────────────────
    // Each Mask entry is independently paintable and can carry its own local
    // adjustments via adjustmentsForTarget(mask.id). Replaces the old
    // setActiveMask()/activeMask() pair, which always read/wrote m_masks[0]
    // regardless of which mask the user had selected.
    [[nodiscard]] QImage maskImage(const QString& maskId) const;
    void setMaskImage(const QString& maskId, const QImage& image);
    QString addMask(const QString& name = QString());
    // Removes the mask AND every Adjustment targeting it, so deleting a mask
    // never leaves orphaned per-mask adjustments behind.
    void removeMask(const QString& id);
    // Re-adds a single mask exactly as previously saved (id, name,
    // feather/inverted, and pixel data all preserved verbatim). Used only
    // by ProjectStore::loadProject(), after openSourceImage() has already
    // reset the document to an empty mask list. Deliberately bypasses
    // AutoHistoryStep/history entirely -- same reasoning as
    // DocumentModel::restoreLayer(): a freshly loaded project should start
    // with empty undo/redo, not synthetic entries per restored mask.
    void restoreMask(const Mask& mask);

    // Legacy read-only accessor kept only for RenderPipeline's unused legacy
    // renderPreview()/renderPreviewFromData() overloads. Nothing in
    // DocumentController calls this anymore as of the multi-mask rework.
    [[nodiscard]] QImage activeMask() const;

    void rotateClockwise();
    void rotateCounterClockwise();
    void flipHorizontal();
    void flipVertical();
    void undo();
    void redo();

    // Layer management
    void addImageLayer(const QString& path);
    // Re-adds a single overlay layer exactly as previously saved (id,
    // transform, and metadata preserved verbatim) plus its pixel data.
    // Used only by ProjectStore::loadProject() while rebuilding the
    // overlay-layer stack after openSourceImage() has already reset the
    // document to just its base layer -- see restoreLayer()'s definition
    // for why this deliberately bypasses history/AutoHistoryStep.
    void restoreLayer(const Layer& layer, const QImage& image);
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
    // Emitted (in addition to changed()) when undo()/redo() restores a
    // structural snapshot -- i.e. sourceImage and/or masks were swapped, not
    // just adjustments. DocumentController listens for this to recreate
    // m_brushEngine and regenerate mask temp-preview files, the same resync
    // work applyCrop() already does going forward, now also needed going
    // backward/forward through undo/redo.
    void structuralHistoryApplied();

private:
    // Returns the first adjustment matching type AND targetMaskId.
    Adjustment*       findAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId);
    const Adjustment* findAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId) const;
    // Legacy: only searches among targetMaskId=="" adjustments.
    Adjustment*       findAdjustment(AdjustmentType type);
    const Adjustment* findAdjustment(AdjustmentType type) const;
    Layer*            findLayer(const QString& id);

    // One undo/redo step. Adjustment-only steps (the common case -- dragging
    // a slider) leave sourceImage/masks default-constructed, which keeps
    // most undo steps as cheap as a small QVector<Adjustment> copy; QImage
    // and QVector<Mask> are implicitly shared besides, so even structural
    // snapshots don't deep-copy pixel data until something actually diverges.
    struct HistorySnapshot {
        QVector<Adjustment> adjustments;
        // Layer metadata (order, opacity, visibility, and -- what this
        // fixes -- posX/posY/scaleX/scaleY/rotation). Captured/restored
        // unconditionally, same as `adjustments`, NOT gated behind
        // `structural`: Layer entries are lightweight value structs (no
        // QImage inside Layer itself -- that lives separately in
        // m_layerImages, untouched by history), so there's no cost reason
        // to treat them as "expensive, structural-only" state the way
        // sourceImage/masks are. This also retroactively fixes
        // addImageLayer()/moveLayer()/deleteLayer(), which already wrap
        // themselves in AutoHistoryStep but previously had nothing in the
        // snapshot to actually restore.
        QVector<Layer> layers;
        QImage        sourceImage;   // valid only when structural == true
        QVector<Mask> masks;         // valid only when structural == true
        // Overlay layer pixel data, keyed by Layer::id -- valid only when
        // structural == true. Added to fix a real bug: deleteLayer()
        // removes the deleted layer's entry from m_layerImages, but that
        // removal was previously untracked by history entirely (see the
        // comment above -- "m_layerImages, untouched by history" was a
        // known, deliberate simplification). Undoing a delete restored the
        // layer's METADATA (posX/posY/scaleX/scaleY/etc., via `layers`
        // above) but not its pixel data, so layerImage() fell back to
        // returning the base source image for that id -- rendering the
        // base photo, transformed exactly like the deleted layer, in its
        // place. deleteLayer() now opens a structural=true transaction so
        // this field captures/restores m_layerImages the same way
        // sourceImage/masks already do for crop/inpaint/upscale.
        QHash<QString, QImage> layerImages;
        bool          structural = false;
        QString       label;
    };
    [[nodiscard]] HistorySnapshot captureSnapshot(const QString& label, bool structural) const;
    void applySnapshot(const HistorySnapshot& snapshot);
    // Returns false ("nothing to record") if the currently-open transaction
    // didn't actually change anything -- e.g. a slider was pressed and
    // released without moving -- so a no-op undo step isn't pushed.
    [[nodiscard]] bool transactionChangedAnything() const;

    // RAII guard giving any call site that doesn't know about explicit
    // transactions today's "one call = one undo step" behavior for free: it
    // opens a transaction only if one isn't already open (so it's a no-op
    // nested inside an explicit beginHistoryTransaction()/commit() pair, e.g.
    // a slider drag), and commits on scope exit.
    class AutoHistoryStep {
    public:
        AutoHistoryStep(DocumentModel& doc, const QString& label, bool structural)
            : m_doc(doc), m_owns(!doc.m_transactionOpen)
        {
            if (m_owns) {
                m_doc.beginHistoryTransaction(label, structural);
            } else if (!label.isEmpty()) {
                // Nested inside an outer, caller-owned transaction (e.g. a
                // slider drag bracketed by beginAdjustmentEdit()/
                // commitAdjustmentEdit()). Refresh the ALREADY-OPEN
                // transaction's label to this call's more specific/current
                // description, so the entry that eventually gets committed
                // reflects the LAST tick's value (e.g. "Exposure: 1.50")
                // rather than whatever generic placeholder the outer
                // transaction was opened with. Purely a label update --
                // doesn't touch ownership or the captured pre-mutation
                // snapshot data, and is a harmless no-op for callers (like
                // layer-transform drags) that pass the same literal label
                // on every tick anyway.
                m_doc.m_transactionSnapshot.label = label;
            }
        }
        ~AutoHistoryStep() { if (m_owns) m_doc.commitHistoryTransaction(); }
        AutoHistoryStep(const AutoHistoryStep&) = delete;
        AutoHistoryStep& operator=(const AutoHistoryStep&) = delete;
    private:
        DocumentModel& m_doc;
        bool m_owns;
    };

    QString   m_projectId;
    QString   m_sourcePath;
    QImage    m_sourceImage;
    bool      m_isDownsampled = false;
    QVector<Layer>      m_layers;
    QVector<Mask>       m_masks;
    QVector<Adjustment> m_adjustments;
    QHash<QString, QImage> m_layerImages;
    bool             m_transactionOpen = false;
    HistorySnapshot  m_transactionSnapshot;   // valid only while m_transactionOpen
    QVector<HistorySnapshot>      m_undoStack;
    QVector<HistorySnapshot>      m_redoStack;
    QVector<QImage>               m_sourceImageHistory;
};
} // namespace lumen
