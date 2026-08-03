# LumenForge Source Bundle


---

# File: `core\editor-core\DocumentModel.hpp`
```cpp
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
    // asHistoryBoundary marks the pushed entry as a "clean state"
    // checkpoint (see HistorySnapshot::isHistoryBoundary) -- currently
    // only resetAdjustments() passes true.
    void commitHistoryTransaction(const QString& finalLabel = QString(), bool asHistoryBoundary = false);
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
    // targetLayerId: which layer this mask's adjustments apply to (empty =
    // base image). See Mask::targetLayerId.
    QString addMask(const QString& name = QString(), const QString& targetLayerId = QString());
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
    void setLayerName(const QString& id, const QString& name);
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
        // Marks this entry as a "clean state" checkpoint (currently only
        // Reset All uses this). historyLabels() stops and excludes
        // everything at or before the MOST RECENT such entry, so the
        // History panel shows an empty timeline right after Reset All --
        // matching "return to a clean, unedited state" -- while undo()/
        // redo() treat it as a perfectly ordinary stack entry: undoing it
        // still restores the exact pre-reset adjustments AND makes the
        // panel show the full prior timeline again (since historyLabels()
        // no longer sees a boundary in its way), and redoing it re-hides
        // that timeline. No separate history mechanism -- this is the
        // same m_undoStack/m_redoStack entries every other action uses,
        // with one extra bit that only the DISPLAY function reads.
        bool          isHistoryBoundary = false;
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
```

---

# File: `core\editor-core\DocumentModel.cpp`
```cpp
#include "editor-core/DocumentModel.hpp"
#include "image-core/RawImporter.hpp"
#include <QFileInfo>
#include <QUuid>
namespace lumen {
namespace {
QString makeId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
} // namespace

QString adjustmentTypeToString(AdjustmentType type)
{
    switch (type) {
    case AdjustmentType::Brightness:      return "brightness";
    case AdjustmentType::Exposure:        return "exposure";
    case AdjustmentType::Contrast:        return "contrast";
    case AdjustmentType::Highlights:      return "highlights";
    case AdjustmentType::Shadows:         return "shadows";
    case AdjustmentType::Whites:          return "whites";
    case AdjustmentType::Blacks:          return "blacks";
    case AdjustmentType::Saturation:      return "saturation";
    case AdjustmentType::Vibrance:        return "vibrance";
    case AdjustmentType::Temperature:     return "temperature";
    case AdjustmentType::Tint:            return "tint";
    case AdjustmentType::RotationDegrees: return "rotationDegrees";
    case AdjustmentType::FlipHorizontal:  return "flipHorizontal";
    case AdjustmentType::FlipVertical:    return "flipVertical";
    case AdjustmentType::ToneCurveLuma:   return "toneCurveLuma";
    case AdjustmentType::ToneCurveR:      return "toneCurveR";
    case AdjustmentType::ToneCurveG:      return "toneCurveG";
    case AdjustmentType::ToneCurveB:      return "toneCurveB";
    case AdjustmentType::NoiseReduction:  return "noiseReduction";
    case AdjustmentType::Sharpening:      return "sharpening";
    default:                              return "exposure";
    }
}

AdjustmentType adjustmentTypeFromString(const QString& v)
{
    if (v == "brightness")      return AdjustmentType::Brightness;
    if (v == "contrast")        return AdjustmentType::Contrast;
    if (v == "highlights")      return AdjustmentType::Highlights;
    if (v == "shadows")         return AdjustmentType::Shadows;
    if (v == "whites")          return AdjustmentType::Whites;
    if (v == "blacks")          return AdjustmentType::Blacks;
    if (v == "saturation")      return AdjustmentType::Saturation;
    if (v == "vibrance")        return AdjustmentType::Vibrance;
    if (v == "temperature")     return AdjustmentType::Temperature;
    if (v == "tint")            return AdjustmentType::Tint;
    if (v == "rotationDegrees") return AdjustmentType::RotationDegrees;
    if (v == "flipHorizontal")  return AdjustmentType::FlipHorizontal;
    if (v == "flipVertical")    return AdjustmentType::FlipVertical;
    if (v == "toneCurveLuma")   return AdjustmentType::ToneCurveLuma;
    if (v == "toneCurveR")      return AdjustmentType::ToneCurveR;
    if (v == "toneCurveG")      return AdjustmentType::ToneCurveG;
    if (v == "toneCurveB")      return AdjustmentType::ToneCurveB;
    if (v == "noiseReduction")  return AdjustmentType::NoiseReduction;
    if (v == "sharpening")      return AdjustmentType::Sharpening;
    return AdjustmentType::Exposure;
}

DocumentModel::DocumentModel(QObject* parent) : QObject(parent) {}

bool DocumentModel::openSourceImage(const QString& path)
{
    QImage image;
    const QString ext = QFileInfo(path).suffix().toLower();
    if (RawImporter::supportedExtensions().contains(ext)) {
        RawImporter importer;
        image = importer.load(path);
    }
    if (image.isNull()) image.load(path);
    if (image.isNull()) return false;

    clear();
    m_projectId  = makeId();
    m_sourcePath = QFileInfo(path).absoluteFilePath();

    constexpr int MAX_MP = 100'000'000;
    if (image.width() * image.height() > MAX_MP) {
        m_isDownsampled = true;
        const double scale = std::sqrt(static_cast<double>(MAX_MP) /
                                       (image.width() * image.height()));
        m_sourceImage = image
            .scaled(image.size() * scale, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_RGBA64);
    } else {
        m_isDownsampled = false;
        m_sourceImage   = image.convertToFormat(QImage::Format_RGBA64);
    }

    Layer base;
    base.id            = makeId();
    base.name          = QFileInfo(path).completeBaseName();
    base.kind          = LayerKind::Image;
    // Fixed, well-known id -- matches the "source" row ProjectStore
    // always writes to source_assets for the base image (see
    // ProjectStore::saveProject()/loadProject()). Previously this was
    // m_projectId (a fresh random UUID, and one never read by anything
    // else in the codebase -- see m_projectId's other two uses, both just
    // generating/clearing it), which meant nothing could reliably tell
    // "this saved layer row is the base layer" apart from an overlay
    // layer when reloading a project. loadProject()'s overlay-layer
    // restore loop depends on this exact value to skip re-creating the
    // base layer a second time, rather than relying on order_index (the
    // base layer can be reordered like any other via Main.qml's Move
    // Up/Down buttons, which have no isBase guard).
    base.sourceAssetId = kBaseLayerSourceAssetId;
    m_layers.push_back(base);
    m_layerImages[base.id] = m_sourceImage;
    emit changed();
    return true;
}

void DocumentModel::replaceSourceImage(const QImage& newImage)
{
    if (m_sourceImageHistory.size() >= 5)
        m_sourceImageHistory.removeFirst();
    m_sourceImageHistory.push_back(m_sourceImage);
    m_sourceImage = newImage.convertToFormat(QImage::Format_RGBA64);
    if (!m_layers.isEmpty())
        m_layerImages[m_layers.first().id] = m_sourceImage;
    emit changed();
}

void DocumentModel::clear()
{
    m_projectId.clear(); m_sourcePath.clear();
    m_sourceImage = {}; m_isDownsampled = false;
    m_layers.clear(); m_masks.clear(); m_adjustments.clear();
    m_layerImages.clear(); m_sourceImageHistory.clear();
    m_undoStack.clear(); m_redoStack.clear();
    emit changed(); emit historyChanged();
}

bool    DocumentModel::hasDocument()   const { return !m_sourceImage.isNull(); }
QString DocumentModel::sourcePath()    const { return m_sourcePath; }
QSize   DocumentModel::sourceSize()    const { return m_sourceImage.size(); }
bool    DocumentModel::isDownsampled() const { return m_isDownsampled; }
const QImage& DocumentModel::sourceImage() const { return m_sourceImage; }

// ── Adjustment queries ────────────────────────────────────────────────────────

QVector<Adjustment> DocumentModel::adjustments() const { return m_adjustments; }

QVector<Adjustment> DocumentModel::adjustmentsForLayer(const QString& layerId) const
{
    QVector<Adjustment> result;
    for (const Adjustment& a : m_adjustments) {
        if (a.targetLayerId == layerId || (layerId.isEmpty() && a.targetLayerId.isEmpty()))
            result.push_back(a);
    }
    return result;
}

// Issue 5: filter by targetMaskId
QVector<Adjustment> DocumentModel::adjustmentsForTarget(const QString& targetMaskId) const
{
    QVector<Adjustment> result;
    for (const Adjustment& a : m_adjustments)
        if (a.targetMaskId == targetMaskId && a.enabled)
            result.push_back(a);
    return result;
}

// ── Private finders ───────────────────────────────────────────────────────────

Adjustment* DocumentModel::findAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId)
{
    for (Adjustment& a : m_adjustments)
        if (a.type == type && a.targetMaskId == targetMaskId) return &a;
    return nullptr;
}

const Adjustment* DocumentModel::findAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId) const
{
    for (const Adjustment& a : m_adjustments)
        if (a.type == type && a.targetMaskId == targetMaskId) return &a;
    return nullptr;
}

// Legacy finders: only consider global (targetMaskId=="") adjustments.
// This prevents mask-targeted adjustments from leaking into global reads.
Adjustment* DocumentModel::findAdjustment(AdjustmentType type)
{
    for (Adjustment& a : m_adjustments)
        if (a.type == type && a.targetMaskId.isEmpty()) return &a;
    return nullptr;
}

const Adjustment* DocumentModel::findAdjustment(AdjustmentType type) const
{
    for (const Adjustment& a : m_adjustments)
        if (a.type == type && a.targetMaskId.isEmpty()) return &a;
    return nullptr;
}

// ── Target-aware scalar adjustment (issue 5) ──────────────────────────────────

void DocumentModel::setScalarAdjustmentForTarget(AdjustmentType type, double value, const QString& targetMaskId, const QString& label)
{
    if (qFuzzyCompare(scalarAdjustmentForTarget(type, targetMaskId) + 1.0, value + 1.0)) return;
    {
        // No-ops (doesn't open/commit its own transaction) when a caller
        // already has one open via beginHistoryTransaction() -- e.g. a
        // whole slider drag -- so that drag still ends up as one undo step
        // no matter how many times this is called during it. Whether or
        // not that's the case, the label still gets passed through: if
        // this call owns the transaction, it becomes that entry's label;
        // if it's nested inside an outer one, it REFRESHES that outer
        // transaction's label to this call's value (see AutoHistoryStep) --
        // which is exactly how a slider drag ends up committing with the
        // FINAL tick's descriptive text ("Exposure: 1.50") rather than
        // the generic placeholder the drag was opened with.
        QString effectiveLabel = label;
        if (effectiveLabel.isEmpty()) {
            QString name = adjustmentTypeToString(type);
            if (!name.isEmpty()) name[0] = name[0].toUpper();
            effectiveLabel = targetMaskId.isEmpty()
                ? QString("%1: %2").arg(name).arg(value, 0, 'f', 2)
                : QString("%1 (mask): %2").arg(name).arg(value, 0, 'f', 2);
        }
        AutoHistoryStep step(*this, effectiveLabel, false);
        Adjustment* adj = findAdjustmentForTarget(type, targetMaskId);
        if (!adj) {
            Adjustment next;
            next.id           = makeId();
            next.type         = type;
            next.targetMaskId = targetMaskId;
            next.order        = m_adjustments.size();
            m_adjustments.push_back(next);
            adj = &m_adjustments.last();
        }
        adj->parameters["value"] = value;
    }
    emit changed(); emit historyChanged();
}

double DocumentModel::scalarAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId) const
{
    const Adjustment* a = findAdjustmentForTarget(type, targetMaskId);
    return a ? a->parameters.value("value").toDouble(0.0) : 0.0;
}

// ── Legacy scalar adjustment (delegates to targetMaskId="") ──────────────────

void DocumentModel::setScalarAdjustment(AdjustmentType type, double value, const QString& label)
{
    setScalarAdjustmentForTarget(type, value, QString(), label);
}

double DocumentModel::scalarAdjustment(AdjustmentType type) const
{
    return scalarAdjustmentForTarget(type, QString());
}

// ── Layer/mask queries ────────────────────────────────────────────────────────

QVector<Layer> DocumentModel::layers()  const { return m_layers; }
QVector<Mask>  DocumentModel::masks()   const { return m_masks; }

QImage DocumentModel::layerImage(const QString& layerId) const
{
    if (m_layerImages.contains(layerId))
        return m_layerImages[layerId];
    return m_sourceImage;
}

QImage DocumentModel::maskImage(const QString& maskId) const
{
    if (maskId.isEmpty()) return {};
    for (const Mask& m : m_masks)
        if (m.id == maskId) return m.mask;
    return {};
}

void DocumentModel::setMaskImage(const QString& maskId, const QImage& image)
{
    if (maskId.isEmpty()) return;
    for (Mask& m : m_masks) {
        if (m.id == maskId) {
            m.mask = image;
            emit changed();
            return;
        }
    }
    // Defensive fallback: the target should always have been created via
    // addMask() before anything paints into it, but if that invariant is
    // ever violated, create the entry now rather than silently dropping
    // the stroke.
    Mask m;
    m.id   = maskId;
    m.name = QString("Mask %1").arg(m_masks.size() + 1);
    m.mask = image;
    m_masks.push_back(m);
    emit changed();
}

QString DocumentModel::addMask(const QString& name, const QString& targetLayerId)
{
    Mask m;
    m.id   = makeId();
    m.name = name.isEmpty() ? QString("Mask %1").arg(m_masks.size() + 1) : name;
    m.targetLayerId = targetLayerId;
    m_masks.push_back(m);
    emit changed();
    return m.id;
}

void DocumentModel::removeMask(const QString& id)
{
    if (id.isEmpty()) return;
    const qsizetype before = m_masks.size();
    m_masks.removeIf([&](const Mask& m) { return m.id == id; });
    if (m_masks.size() == before) return; // nothing removed
    m_adjustments.removeIf([&](const Adjustment& a) { return a.targetMaskId == id; });
    emit changed();
}

void DocumentModel::restoreMask(const Mask& mask)
{
    m_masks.push_back(mask);
    emit changed();
}

QImage DocumentModel::activeMask() const
{
    return m_masks.isEmpty() ? QImage() : m_masks.first().mask;
}

bool DocumentModel::canUndo() const { return !m_undoStack.isEmpty(); }
bool DocumentModel::canRedo() const { return !m_redoStack.isEmpty(); }

QStringList DocumentModel::historyLabels() const
{
    // Walk from the most recent entry backward, stopping (and excluding)
    // at the first "clean state" boundary found -- see
    // HistorySnapshot::isHistoryBoundary. With no boundary anywhere in
    // the stack, this just returns every label, oldest first, same as
    // before.
    QStringList labels;
    for (int i = m_undoStack.size() - 1; i >= 0; --i) {
        if (m_undoStack[i].isHistoryBoundary) break;
        labels.prepend(m_undoStack[i].label);
    }
    return labels;
}

// ── Transform adjustments ─────────────────────────────────────────────────────

void DocumentModel::rotateClockwise()
{ setScalarAdjustment(AdjustmentType::RotationDegrees,
    (int(scalarAdjustment(AdjustmentType::RotationDegrees)) + 90) % 360, "Rotate CW"); }
void DocumentModel::rotateCounterClockwise()
{ setScalarAdjustment(AdjustmentType::RotationDegrees,
    (int(scalarAdjustment(AdjustmentType::RotationDegrees)) + 270) % 360, "Rotate CCW"); }
void DocumentModel::flipHorizontal()
{ setScalarAdjustment(AdjustmentType::FlipHorizontal,
    scalarAdjustment(AdjustmentType::FlipHorizontal) > 0.5 ? 0.0 : 1.0, "Flip Horizontal"); }
void DocumentModel::flipVertical()
{ setScalarAdjustment(AdjustmentType::FlipVertical,
    scalarAdjustment(AdjustmentType::FlipVertical) > 0.5 ? 0.0 : 1.0, "Flip Vertical"); }

// ── Undo / redo ───────────────────────────────────────────────────────────────

void DocumentModel::undo()
{
    if (!canUndo()) return;
    const HistorySnapshot target = m_undoStack.takeLast();
    HistorySnapshot redoEntry = captureSnapshot(target.label, target.structural);
    // Propagate the boundary flag along with the entry -- redoing this
    // later needs to re-hide the timeline exactly like the original
    // commit did (see HistorySnapshot::isHistoryBoundary).
    redoEntry.isHistoryBoundary = target.isHistoryBoundary;
    m_redoStack.push_back(redoEntry);
    applySnapshot(target);
}
void DocumentModel::redo()
{
    if (!canRedo()) return;
    const HistorySnapshot target = m_redoStack.takeLast();
    HistorySnapshot undoEntry = captureSnapshot(target.label, target.structural);
    undoEntry.isHistoryBoundary = target.isHistoryBoundary;
    m_undoStack.push_back(undoEntry);
    applySnapshot(target);
}

// ── Layer management ──────────────────────────────────────────────────────────

void DocumentModel::addImageLayer(const QString& path)
{
    QImage img; img.load(path);
    if (img.isNull()) return;
    const QString name = QFileInfo(path).completeBaseName();
    AutoHistoryStep step(*this, QString("Add layer: %1").arg(name), false);
    Layer layer;
    layer.id    = makeId();
    layer.name  = name;
    layer.kind  = LayerKind::Image;
    layer.order = m_layers.size();
    // Persistence fix: remember where this layer's pixel data came from
    // so ProjectStore::saveProject() can create a source_assets row for
    // it (the same mechanism the base image already uses) -- previously
    // nothing recorded this anywhere, so overlay layers had no asset
    // reference to save and were silently dropped on reload.
    layer.sourcePath = QFileInfo(path).absoluteFilePath();
    // Issue 6: keep layer at its own native resolution; transforms handle placement.
    // Do NOT scale to source size — that would stretch every added image to fill
    // the entire canvas, which defeats the "sticker on base" use case.
    m_layerImages[layer.id] = img.convertToFormat(QImage::Format_RGBA64);

    // Adaptive initial spawn scale: lands the new layer at roughly
    // TARGET_CANVAS_FRACTION of the base canvas WIDTH, computed from actual
    // pixel dimensions rather than a fixed percentage -- but never upscales
    // a layer beyond its own native resolution (qMin(..., 1.0)). The
    // imported bitmap itself is completely untouched -- still stored at
    // full native resolution above, in m_layerImages -- only the transform
    // (scaleX/scaleY) is adjusted, so resizing later via the transform
    // handles always operates on that same full-resolution source, never a
    // downscaled copy.
    //
    // "Canvas" here means the base document image's pixel dimensions
    // (m_sourceImage), not the live on-screen viewport/zoom level --
    // consistent with how posX/posY/scaleX/scaleY are already documented in
    // Layer.hpp as being in base-image pixel space, independent of zoom or
    // window size. Using viewport pixels instead would make a newly added
    // layer's size depend on the window size or zoom level at the moment it
    // happened to be added, which would be inconsistent with every other
    // transform value in the document and wouldn't survive a window resize.
    constexpr double TARGET_CANVAS_FRACTION = 0.25; // ~25%, middle of the requested 20-30% band
    double initialScale = 1.0;
    if (img.width() > 0 && m_sourceImage.width() > 0) {
        const double scaleToTargetFraction =
            (m_sourceImage.width() * TARGET_CANVAS_FRACTION) / img.width();
        initialScale = qMin(scaleToTargetFraction, 1.0);
    }
    layer.scaleX = initialScale;
    layer.scaleY = initialScale;

    m_layers.push_back(layer);
    emit changed();
}

// Restores a single overlay layer exactly as previously saved (id,
// transform, and metadata preserved verbatim) plus its pixel data.
// Deliberately bypasses AutoHistoryStep/history entirely: a freshly
// loaded project should start with empty undo/redo, not one synthetic
// "add layer" step per restored layer -- unlike addImageLayer(), which is
// a real user action and should be undoable. Used only by
// ProjectStore::loadProject(), after openSourceImage() has already reset
// the document down to just its base layer.
void DocumentModel::restoreLayer(const Layer& layer, const QImage& image)
{
    m_layers.push_back(layer);
    m_layerImages[layer.id] = image.convertToFormat(QImage::Format_RGBA64);
    emit changed();
}

void DocumentModel::moveLayer(int from, int to)
{
    if (from < 0 || from >= m_layers.size() || to < 0 || to >= m_layers.size()) return;
    // Base layer is a fixed background, not reorderable.
    if (m_layers[from].isBaseLayer() || to == 0) return;
    AutoHistoryStep step(*this, QString("Move layer"), false);
    m_layers.move(from, to);
    for (int i = 0; i < m_layers.size(); ++i) m_layers[i].order = i;
    emit changed();
}

Layer* DocumentModel::findLayer(const QString& id)
{
    for (Layer& l : m_layers) if (l.id == id) return &l;
    return nullptr;
}

void DocumentModel::setLayerOpacity(const QString& id, double opacity)
{ if (Layer* l = findLayer(id)) { l->opacity = opacity; emit changed(); } }

void DocumentModel::setLayerVisible(const QString& id, bool visible)
{ if (Layer* l = findLayer(id)) { l->visible = visible; emit changed(); } }

void DocumentModel::setLayerBlendMode(const QString& id, BlendMode mode)
{ if (Layer* l = findLayer(id)) { l->blendMode = mode; emit changed(); } }

void DocumentModel::setLayerName(const QString& id, const QString& name)
{
    Layer* layer = findLayer(id);
    if (!layer) return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed == layer->name) return;

    AutoHistoryStep step(*this, QString("Rename layer: %1").arg(trimmed), false);
    layer->name = trimmed;
    emit changed();
}

void DocumentModel::deleteLayer(const QString& id)
{
    if (m_layers.size() <= 1) return;
    const Layer* target = findLayer(id);
    if (!target || target->isBaseLayer()) return;   // base layer isn't deletable
    AutoHistoryStep step(*this, QString("Delete layer"), true);
    m_layers.removeIf([&](const Layer& l){ return l.id == id; });
    m_layerImages.remove(id);
    // Renumber so order stays a contiguous 0..N-1 sequence, same as moveLayer().
    for (int i = 0; i < m_layers.size(); ++i) m_layers[i].order = i;
    QStringList removedMaskIds;
    for (const Mask& m : m_masks)
        if (m.targetLayerId == id) removedMaskIds.push_back(m.id);
    if (!removedMaskIds.isEmpty()) {
        m_masks.removeIf([&](const Mask& m) { return m.targetLayerId == id; });
        m_adjustments.removeIf([&](const Adjustment& a) { return removedMaskIds.contains(a.targetMaskId); });
    }
    emit changed();
}

// Issue 6: per-layer transform
void DocumentModel::setLayerTransform(const QString& id,
                                       double posX, double posY,
                                       double scaleX, double scaleY,
                                       double rotation)
{
    if (Layer* l = findLayer(id)) {
        // AutoHistoryStep gives a single, standalone call to
        // setLayerTransform() its own undo step (structural=false --
        // posX/posY/scaleX/scaleY/rotation are a few doubles, not image
        // data, same cost class as an adjustment edit). When a caller
        // brackets a whole gesture with an explicit
        // beginHistoryTransaction()/commitHistoryTransaction() pair (e.g. a
        // future DocumentController::beginLayerTransformEdit()/
        // commitLayerTransformEdit(), mirroring how beginAdjustmentEdit()/
        // commitAdjustmentEdit() already bracket slider drags), this
        // no-ops and the whole gesture collapses into that outer
        // transaction's single step instead -- same mechanism, no separate
        // history path.
        AutoHistoryStep step(*this, QString("Transform layer"), false);
        l->posX     = posX;
        l->posY     = posY;
        l->scaleX   = scaleX;
        l->scaleY   = scaleY;
        l->rotation = rotation;
        emit changed();
    }
}

// ── History transactions ────────────────────────────────────────────────────

void DocumentModel::beginHistoryTransaction(const QString& label, bool structural)
{
    if (m_transactionOpen) return; // nested begin -- first begin wins, matches AutoHistoryStep's expectations
    m_transactionOpen = true;
    m_transactionSnapshot = captureSnapshot(label, structural);
}

bool DocumentModel::transactionChangedAnything() const
{
    if (m_transactionSnapshot.adjustments != m_adjustments) return true;
    if (m_transactionSnapshot.layers != m_layers) return true;
    if (m_transactionSnapshot.structural
        && m_transactionSnapshot.sourceImage.cacheKey() != m_sourceImage.cacheKey())
        return true;
    return false;
}

void DocumentModel::commitHistoryTransaction(const QString& finalLabel, bool asHistoryBoundary)
{
    if (!m_transactionOpen) return;
    m_transactionOpen = false;
    if (!finalLabel.isEmpty()) m_transactionSnapshot.label = finalLabel;
    m_transactionSnapshot.isHistoryBoundary = asHistoryBoundary;
    if (!transactionChangedAnything()) return; // e.g. slider pressed then released without moving
    m_undoStack.push_back(m_transactionSnapshot);
    if (m_undoStack.size() > 100) m_undoStack.removeFirst();
    m_redoStack.clear();
    emit historyChanged();
}

void DocumentModel::cancelHistoryTransaction()
{
    if (!m_transactionOpen) return;
    m_transactionOpen = false;
    m_adjustments = m_transactionSnapshot.adjustments;
    if (m_transactionSnapshot.structural) {
        m_sourceImage = m_transactionSnapshot.sourceImage;
        m_masks       = m_transactionSnapshot.masks;
        m_layerImages = m_transactionSnapshot.layerImages;
    }
    emit changed(); emit historyChanged();
}

DocumentModel::HistorySnapshot DocumentModel::captureSnapshot(const QString& label, bool structural) const
{
    HistorySnapshot s;
    s.adjustments = m_adjustments;
    s.layers      = m_layers;
    s.label       = label;
    s.structural  = structural;
    if (structural) {
        s.sourceImage  = m_sourceImage;
        s.masks        = m_masks;
        s.layerImages  = m_layerImages;
    }
    return s;
}

void DocumentModel::applySnapshot(const HistorySnapshot& s)
{
    m_adjustments = s.adjustments;
    m_layers      = s.layers;
    if (s.structural) {
        m_sourceImage = s.sourceImage;
        m_masks       = s.masks;
        m_layerImages = s.layerImages;
    }
    emit changed();
    if (s.structural) emit structuralHistoryApplied();
    emit historyChanged();
}

} // namespace lumen
```

---

# File: `app\src\editor\DocumentController.hpp`
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
#include <QHash>
#include <QImage>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QThreadPool>
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
    // Which layer (if any) the CURRENTLY ACTIVE mask's pixel data is baked
    // into (empty = base image) -- see Mask::targetLayerId. MaskCanvas.qml
    // needs this to know whether to display maskUrl's PNG stretched across
    // the whole canvas (base-scoped) or positioned/scaled/rotated to match
    // an owning overlay's on-screen footprint (layer-scoped), since a
    // layer-scoped mask's stored pixels are in THAT layer's own native
    // space, not canvas space.
    Q_PROPERTY(QString activeMaskOwnerLayerId READ activeMaskOwnerLayerId NOTIFY maskChanged)
    Q_PROPERTY(int sourceWidth    READ sourceWidth  NOTIFY documentChanged)
    Q_PROPERTY(int sourceHeight   READ sourceHeight NOTIFY documentChanged)
    Q_PROPERTY(bool   aiBusy     READ aiBusy      NOTIFY aiBusyChanged)
    Q_PROPERTY(QString aiStatus  READ aiStatus    NOTIFY aiStatusChanged)
    Q_PROPERTY(QString aiTool    READ aiTool      NOTIFY aiStatusChanged)
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
    [[nodiscard]] QString activeMaskOwnerLayerId() const;
    [[nodiscard]] int     sourceWidth()  const;
    [[nodiscard]] int     sourceHeight() const;
    [[nodiscard]] bool    aiBusy()       const;
    [[nodiscard]] QString aiStatus()     const;
    [[nodiscard]] QString aiTool()       const;
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
    // Bracket a single adjustment-slider drag gesture so the whole drag is
    // one undo step and one History-log line, instead of one per tick.
    // Wired to QML Slider's pressed/released transition (see Main.qml).
    // Safe to call even for a discrete (non-drag) value change -- if these
    // aren't called, setAdjustment() falls back to logging/committing
    // immediately, matching pre-existing single-click behavior.
    Q_INVOKABLE void beginAdjustmentEdit();
    Q_INVOKABLE void commitAdjustmentEdit();
    // Mask tools
    Q_INVOKABLE void paintMaskStroke(double x, double y, double radius, bool erase);
    Q_INVOKABLE void commitMaskPaint();
    Q_INVOKABLE void clearMask();
    Q_INVOKABLE void applyGradientMask(double x1, double y1, double x2, double y2);
    Q_INVOKABLE void applyRadialMask(double cx, double cy, double radius);
    Q_INVOKABLE void applyCrop(int x, int y, int w, int h, double rotation = 0.0);
    Q_INVOKABLE void refineEdges();
    // Issue 5: create a brand-new empty mask slot and switch the editing
    // target to it. Sliders read 0 for a target with no adjustments yet,
    // so this alone satisfies "sliders reset for new mask".
    // Part 5 redesign: targetLayerId is chosen EXPLICITLY by the caller
    // (the "+ Add Mask" popover in Main.qml) at the moment of creation --
    // empty means base image. No longer inferred from selectedLayerId.
    Q_INVOKABLE void addNewMaskTarget(const QString& targetLayerId = QString());
    // Deletes an arbitrary mask by id (used by the per-row delete button
    // in the Masks panel list) -- unlike clearMask(), which always acts on
    // whichever mask is currently being edited, this can remove any mask
    // regardless of which one is active. Delegates to clearMask() when the
    // id happens to match the active target, since that already does the
    // right additional cleanup (resetting the active edit target) for
    // exactly that case.
    Q_INVOKABLE void deleteMask(const QString& maskId);
    // AI
    Q_INVOKABLE void requestAiMask(double x, double y);
    Q_INVOKABLE void applyInpaint();
    Q_INVOKABLE void applyUpscale();
    // Layers
    Q_INVOKABLE void addImageLayer(const QUrl& url);
    Q_INVOKABLE void deleteLayer(const QString& id);
    Q_INVOKABLE void setLayerOpacity(const QString& id, double opacity);
    Q_INVOKABLE void setLayerVisible(const QString& id, bool visible);
    Q_INVOKABLE void renameLayer(const QString& id, const QString& name);
    Q_INVOKABLE void moveLayerUp(const QString& id);
    Q_INVOKABLE void moveLayerDown(const QString& id);
    // Issue 6: per-layer transform (position/scale/rotation)
    Q_INVOKABLE void setLayerTransform(const QString& id,
                                        double posX, double posY,
                                        double scaleX, double scaleY,
                                        double rotation);
    // Bracket a single layer-drag gesture (move now; resize/rotate in later
    // stages, since they all go through the same setLayerTransform()) so the
    // whole gesture is one undo step and one History-log line, instead of
    // one per tick -- exactly mirrors beginAdjustmentEdit()/
    // commitAdjustmentEdit() for sliders, using the same
    // DocumentModel::beginHistoryTransaction()/commitHistoryTransaction()
    // pair. Wired to LayerTransformOverlay.qml's press/release. Safe to
    // call even when nothing actually moved -- DocumentModel's own
    // transactionChangedAnything() (now layer-aware) skips a no-op
    // interaction, same as it already does for adjustment drags.
    Q_INVOKABLE void beginLayerTransformEdit();
    Q_INVOKABLE void commitLayerTransformEdit();
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
    void addRecentFile(const QString& path);
    [[nodiscard]] QString localPath(const QUrl& url) const;
    // Resolves a mask's Mask::targetLayerId to a display name -- "Base
    // Image" for the (default) empty case, the owning layer's current
    // name if it still exists, or a defensive fallback label if not
    // (shouldn't normally be reachable since deleteLayer() already
    // removes masks scoped to a layer being deleted). Shared by
    // maskList() and adjustmentTargets(), the two places this needs
    // showing in the UI.
    [[nodiscard]] QString maskOwnerLayerName(const lumen::Mask& mask) const;
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
    // Root-cause fix for "overlay masks don't follow the overlay": a
    // layer-scoped mask's pixel data is baked into its owning layer's own
    // native pixel space (rather than staying in fixed base-image canvas
    // space) at paint-commit time, using that layer's CURRENT transform.
    // From then on the render pipeline applies it directly (see
    // RenderPipeline::canvasToLayerLocalTransform()), so the mask is
    // naturally carried along by whatever transform the layer has at
    // render time, instead of being re-sampled against it every render.
    //
    // bakeMaskForTarget:   canvas/base-image space -> layer-local space (storage)
    // unbakeMaskFromTarget: layer-local space -> canvas space (for continued
    //                       painting -- MaskCanvas.qml always works in
    //                       canvas space regardless of which layer a mask
    //                       belongs to)
    // Both are no-ops (return the input unchanged) for base-image-scoped
    // masks (targetLayerId empty), which is the only kind that existed
    // before layer-aware masking and needs no conversion either way.
    [[nodiscard]] QImage bakeMaskForTarget(const QString& maskId, const QImage& canvasSpaceMask) const;
    [[nodiscard]] QImage unbakeMaskFromTarget(const QString& maskId, const QImage& storedMask) const;
    // Shared warp used by both directions above: draws `source` through
    // `transform` into a fresh transparent image of `outputSize`.
    [[nodiscard]] QImage warpMask(const QImage& source, const QTransform& transform, QSize outputSize) const;
    // Resolves a mask id to its owning Layer (false if base-scoped, or if
    // the owning layer no longer exists).
    [[nodiscard]] bool findMaskOwnerLayer(const QString& maskId, lumen::Layer& outLayer) const;
    // Clears m_activeAdjustmentTarget (falling back to Full Image) if it
    // currently points to a mask id that no longer exists in the
    // document -- e.g. because the layer that owned it was just deleted,
    // or a structural undo/redo stepped past whatever created it. Shared
    // by deleteLayer() and resyncAfterStructuralHistory(), which both
    // need exactly this check.
    void clearAdjustmentTargetIfDangling();
    void saveMaskToTemp(const QString& maskId);
    void flushMaskSave();
    void setAiBusy(bool busy);
    void setAiTool(const QString& tool);
    void setAiStatus(const QString& status);
    void autoSave();
    void checkRecovery();
    QString autosavePath() const;
    // Builds the MaskAdjLayer vector for all masks that have local adjustments,
    // for use by renderWithLayers().
    [[nodiscard]] std::vector<lumen::MaskAdjLayer> buildMaskAdjLayers() const;
    // Resyncs m_brushEngine and mask temp-preview files after a structural
    // undo/redo (crop/inpaint/upscale) swaps sourceImage/masks out from under
    // whatever was cached -- the same work applyCrop() already does forward,
    // now also needed going backward/forward through history.
    void resyncAfterStructuralHistory();

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
    QString  m_aiTool;
    QString  m_aiStatus;
    QTimer*  m_autosaveTimer     = nullptr;
    QTimer*  m_previewDebounce   = nullptr;
    QTimer*  m_maskSaveTimer     = nullptr;
    QTimer*  m_edgeRefineTimer   = nullptr;
    QTimer*  m_transformUiTimer  = nullptr;
    QThreadPool m_refinePool;
    bool     m_edgeRefinePending = false;
    bool     m_hasPendingRecovery = false;
    bool     m_cropActive        = false;

    // Issue 5
    QString  m_activeAdjustmentTarget;   // "" = Full Image, else mask id
    // Issue 6
    QString  m_selectedLayerId;

    // Grouped undo/redo for adjustment sliders (see beginAdjustmentEdit()).
    bool     m_adjustmentEditOpen = false;

    // Grouped undo/redo for layer transform drags (see
    // beginLayerTransformEdit()) -- same pattern as the adjustment-edit
    // members just above.
    bool     m_layerTransformEditOpen = false;
};
```

---

# File: `app\src\editor\DocumentController.cpp`
```cpp
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
#include <QThread>
#include <QElapsedTimer>
#include <QDebug>
#include <QPainter>
#include <QTransform>
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

    m_edgeRefineTimer = new QTimer(this);
    m_edgeRefineTimer->setSingleShot(true);
    m_edgeRefineTimer->setInterval(350);
    connect(m_edgeRefineTimer, &QTimer::timeout, this, [this]() {
        if (m_aiBusy) {
            m_edgeRefinePending = true;
            return;
        }
        refineEdges();
    });

    m_transformUiTimer = new QTimer(this);
    m_transformUiTimer->setSingleShot(true);
    m_transformUiTimer->setInterval(16);
    connect(m_transformUiTimer, &QTimer::timeout, this, [this]() {
        emit layersChanged();
    });
    m_refinePool.setMaxThreadCount(1);
    m_refinePool.setThreadPriority(QThread::LowPriority);

    connect(&m_document, &lumen::DocumentModel::changed, this, [this]() {
        if (m_layerTransformEditOpen) {
            // A transform tick changes only lightweight layer metadata. Do
            // not rebuild every document binding and QML model for every
            // native mouse event; publish the latest layer state at ~60 Hz.
            if (!m_transformUiTimer->isActive()) m_transformUiTimer->start();
            return;
        }
        emit documentChanged();
        emit adjustmentsChanged();
        emit layersChanged();
        // While the brush/eraser is active the QML mask surface is the live
        // view. Rebuilding the full composite after every stroke competes
        // with pointer delivery and makes painting feel sticky; the first
        // non-paint tool change below schedules the committed preview.
        if (m_activeTool == 1 || m_activeTool == 2) return;
        // Layer-transform drags update the QML overlay directly from the
        // live layer model. Avoid starting a full asynchronous composite for
        // every mouse-move tick; commitLayerTransformEdit() schedules one
        // preview after the gesture ends.
        if (m_layerTransformEditOpen) return;
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
QString DocumentController::activeMaskOwnerLayerId() const {
    for (const lumen::Mask& m : m_document.masks())
        if (m.id == m_activeAdjustmentTarget) return m.targetLayerId;
    return QString();
}
int  DocumentController::sourceWidth()  const { return m_document.sourceSize().width(); }
int  DocumentController::sourceHeight() const { return m_document.sourceSize().height(); }
bool    DocumentController::aiBusy()   const { return m_aiBusy; }
QString DocumentController::aiStatus() const { return m_aiStatus; }
QString DocumentController::aiTool()   const { return m_aiTool; }

QVariantList DocumentController::layerModel() const {
    const bool profile = qEnvironmentVariableIsSet("LUMEN_PROFILE");
    QElapsedTimer timer;
    if (profile) timer.start();
    QVariantList list;
    for (const lumen::Layer& l : m_document.layers()) {
        QVariantMap m;
        m["id"]=l.id; m["name"]=l.name; m["opacity"]=l.opacity;
        m["visible"]=l.visible; m["order"]=l.order; m["realId"]=l.id;
        m["isBase"] = l.isBaseLayer();   // was: (l.order == 0)
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
    if (profile)
        qInfo().noquote() << "PROFILE layerModel ms=" << timer.nsecsElapsed()/1000000.0
                          << "layers=" << list.size();
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

QString DocumentController::maskOwnerLayerName(const lumen::Mask& mask) const {
    if (mask.targetLayerId.isEmpty()) return "Base Image";
    for (const lumen::Layer& l : m_document.layers())
        if (l.id == mask.targetLayerId) return l.name;
    return "Deleted layer";
}

QVariantList DocumentController::maskList() const {
    QVariantList list;
    for (const lumen::Mask& m : m_document.masks()) {
        QVariantMap e;
        e["id"]   = m.id;
        e["name"] = m.name;
        const QString path = m_maskTempPaths.value(m.id);
        e["url"]  = path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
        // Which layer this mask's adjustments apply to (see
        // Mask::targetLayerId), for display in the Masks panel -- without
        // this there'd be no way to tell layer-scoped masks apart from
        // base-image ones just by looking at the list.
        e["ownerLayerName"] = maskOwnerLayerName(m);
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
        // Include which layer this mask belongs to directly in the
        // dropdown text (this list's "name" field is what the ComboBox's
        // textRole displays) -- otherwise there'd be no way to tell a
        // base-image mask apart from an overlay-layer one just by looking
        // at the selector. See Mask::targetLayerId.
        const QString baseName = m.name.isEmpty() ? "Mask" : m.name;
        QVariantMap e; e["id"] = m.id; e["name"] = QString("%1 (%2)").arg(baseName, maskOwnerLayerName(m));
        list.append(e);
    }
    return list;
}

QString DocumentController::activeAdjustmentTarget() const { return m_activeAdjustmentTarget; }
void DocumentController::setActiveAdjustmentTarget(const QString& targetMaskId) {
    if (m_activeAdjustmentTarget == targetMaskId) return;
    if (m_maskSaveTimer && m_maskSaveTimer->isActive()) { m_maskSaveTimer->stop(); flushMaskSave(); }
    if (m_edgeRefineTimer) m_edgeRefineTimer->stop();
    m_edgeRefinePending = false;
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
    if (m_aiBusy == busy) return;
    m_aiBusy = busy;
    emit aiBusyChanged();
#ifdef HAVE_OPENCV
    if (!busy && m_edgeRefinePending) {
        m_edgeRefinePending = false;
        if (m_edgeRefineTimer) m_edgeRefineTimer->start(50);
    }
#endif
}
void DocumentController::setAiStatus(const QString& s) {
    m_aiStatus = s; emit aiStatusChanged();
}
void DocumentController::setAiTool(const QString& tool) {
    if (m_aiTool == tool) return;
    m_aiTool = tool;
    emit aiStatusChanged();
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
    if (tool < 1 || tool > 2) {
        if (m_edgeRefineTimer) m_edgeRefineTimer->stop();
        m_edgeRefinePending = false;
    }
    m_activeTool = tool;
    m_cropActive = (tool == 5);
    emit cropActiveChanged(); emit activeToolChanged();
    if (tool != 1 && tool != 2) m_previewDebounce->start();
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
    // asHistoryBoundary=true: Reset All should return the History panel to
    // a clean/empty state ("return to unedited"), not just append one more
    // entry to the visible timeline. Undo still works completely normally
    // -- it's an ordinary undo-stack entry as far as the actual
    // undo/redo mechanism is concerned (every adjustment value AND the
    // prior timeline both come back correctly) -- only historyLabels()'s
    // DISPLAY hides everything at or before a boundary until it's undone
    // past. See HistorySnapshot::isHistoryBoundary.
    m_document.commitHistoryTransaction(label, /*asHistoryBoundary=*/true);
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
bool DocumentController::findMaskOwnerLayer(const QString& maskId, lumen::Layer& outLayer) const {
    QString targetLayerId;
    for (const lumen::Mask& m : m_document.masks())
        if (m.id == maskId) { targetLayerId = m.targetLayerId; break; }
    if (targetLayerId.isEmpty()) return false;
    for (const lumen::Layer& l : m_document.layers())
        if (l.id == targetLayerId) { outLayer = l; return true; }
    return false; // owning layer no longer exists (shouldn't normally happen -- deleteLayer() cleans up its masks)
}

QImage DocumentController::warpMask(const QImage& source, const QTransform& transform, QSize outputSize) const {
    QImage result(outputSize, QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setTransform(transform);
    p.drawImage(QRectF(0, 0, source.width(), source.height()), source);
    return result;
}

QImage DocumentController::bakeMaskForTarget(const QString& maskId, const QImage& canvasSpaceMask) const {
    lumen::Layer owner;
    if (!findMaskOwnerLayer(maskId, owner)) return canvasSpaceMask; // base image -- already correct space
    const QImage layerImg = m_document.layerImage(owner.id);
    if (layerImg.isNull()) return canvasSpaceMask;
    bool ok = false;
    const QTransform canvasToLocal = lumen::RenderPipeline::canvasToLayerLocalTransform(
        owner, m_document.sourceSize(), layerImg.size(), &ok);
    if (!ok) return canvasSpaceMask;
    return warpMask(canvasSpaceMask, canvasToLocal, layerImg.size());
}

QImage DocumentController::unbakeMaskFromTarget(const QString& maskId, const QImage& storedMask) const {
    lumen::Layer owner;
    if (!findMaskOwnerLayer(maskId, owner)) return storedMask; // base image -- already canvas space
    if (storedMask.isNull()) return storedMask;
    bool ok = false;
    const QTransform canvasToLocal = lumen::RenderPipeline::canvasToLayerLocalTransform(
        owner, m_document.sourceSize(), storedMask.size(), &ok);
    if (!ok) return storedMask;
    return warpMask(storedMask, canvasToLocal.inverted(), m_document.sourceSize());
}

void DocumentController::clearAdjustmentTargetIfDangling() {
    if (m_activeAdjustmentTarget.isEmpty()) return;
    for (const lumen::Mask& mask : m_document.masks())
        if (mask.id == m_activeAdjustmentTarget) return; // still exists, nothing to do
    m_activeAdjustmentTarget.clear();
    emit activeAdjustmentTargetChanged();
    emit adjustmentsChanged();
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
    const QImage canvasSpaceMask = upsampleMaskToSource(m_brushEngine->mask(), srcSz);
    // Bake into the target's own layer-local space if it's layer-scoped
    // (no-op for base-image-scoped masks) -- see bakeMaskForTarget()'s
    // comment for why.
    m_document.setMaskImage(targetId, bakeMaskForTarget(targetId, canvasSpaceMask));
    saveMaskToTemp(targetId);
    emit maskChanged();
#ifdef HAVE_OPENCV
    // Coalesce rapid strokes and refine only after the painter pauses. The
    // refinement itself remains asynchronous, so the brush never waits on
    // OpenCV work.
    if (m_edgeRefineTimer) m_edgeRefineTimer->start();
#endif
}
void DocumentController::flushMaskSave() {
    if (!m_brushEngine || m_activeAdjustmentTarget.isEmpty()) return;
    const QImage canvasSpaceMask = upsampleMaskToSource(m_brushEngine->mask(), m_document.sourceSize());
    m_document.setMaskImage(m_activeAdjustmentTarget, bakeMaskForTarget(m_activeAdjustmentTarget, canvasSpaceMask));
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
void DocumentController::deleteMask(const QString& maskId) {
    if (maskId.isEmpty()) return;
    if (maskId == m_activeAdjustmentTarget) {
        clearMask(); // same target -- clearMask() already resets editing focus correctly
        return;
    }
    // A different, non-active mask: remove its data only. Deliberately does
    // NOT touch m_activeAdjustmentTarget or the brush engine -- deleting a
    // mask you're not currently editing shouldn't reset what you ARE
    // currently working on.
    m_document.removeMask(maskId);
    const QString oldPath = m_maskTempPaths.take(maskId);
    if (!oldPath.isEmpty()) QFile::remove(oldPath);
    emit maskChanged();
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
    // Part 5 redesign: painting with no target already selected always
    // creates a BASE-IMAGE mask -- no hidden state involved. A layer-scoped
    // mask can only be created through the explicit "+ Add Mask" target
    // chooser (see addNewMaskTarget()), which sets m_activeAdjustmentTarget
    // directly, so this fallback is never reached for that case. This
    // replaces the previous design, which silently inferred ownership from
    // whichever overlay layer happened to still be selected (a hidden,
    // easily-stale piece of state -- exactly the failure mode reported:
    // "the selected layer silently resets, mask lands on the wrong target").
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
        // If this mask is layer-scoped, its STORED content lives in that
        // layer's own native pixel space (see bakeMaskForTarget()) --
        // convert it back to canvas space before loading it into the
        // brush engine's canvas-space paint buffer, so it lines up
        // visually with the layer's current on-screen position/rotation
        // for continued painting. No-op for base-image-scoped masks.
        const QImage canvasSpaceExisting = unbakeMaskFromTarget(targetId, existing);
        QPainter p(&buffer);
        p.drawImage(QRect(QPoint(0,0), beSz),
                    canvasSpaceExisting.scaled(beSz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
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
    clearAdjustmentTargetIfDangling();
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
    const QImage canvasSpaceMask = upsampleMaskToSource(mask, sz);
    m_document.setMaskImage(targetId, bakeMaskForTarget(targetId, canvasSpaceMask));
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
    const QImage canvasSpaceMask = upsampleMaskToSource(mask, sz);
    m_document.setMaskImage(targetId, bakeMaskForTarget(targetId, canvasSpaceMask));
    saveMaskToTemp(targetId); emit maskChanged();
}
void DocumentController::applyCrop(int x,int y,int w,int h,double rotation) {
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
    QTransform cropRotation;
    cropRotation.rotate(rotation);

    // Crop every mask's pixel data by the identical rect BEFORE replacing the
    // source image, so marked regions stay aligned to the same content
    // instead of drifting to absolute canvas coordinates post-crop. Masks are
    // always stored at the current source resolution (see setMaskImage()
    // call sites), so rect applies directly; intersected() is a defensive
    // guard in case that invariant is ever violated (e.g. after an upscale).
    for (const lumen::Mask& mask : m_document.masks()) {
        if (mask.mask.isNull()) continue;
        const QRect clamped = rect.intersected(mask.mask.rect());
        const QImage croppedMask = clamped.isEmpty() ? QImage() : mask.mask.copy(clamped);
        m_document.setMaskImage(mask.id, croppedMask.isNull()
                                 ? QImage()
                                 : croppedMask.transformed(cropRotation, Qt::SmoothTransformation));
    }
    const QImage croppedSource = m_document.sourceImage().copy(rect);
    m_document.replaceSourceImage(croppedSource.transformed(cropRotation, Qt::SmoothTransformation));
    // Final label needs the post-crop size, which is only known now (crop
    // replaced the source image on the line above) -- commitHistoryTransaction()'s
    // optional override exists for exactly this "known only at the end"
    // case, so the pushed undo entry reads e.g. "Crop 4000×3000" directly,
    // no separate logging step needed.
    m_document.commitHistoryTransaction(
        QString("Crop %1\u00d7%2 (%3\u00b0)")
            .arg(m_document.sourceSize().width())
            .arg(m_document.sourceSize().height())
            .arg(qRound(rotation)));
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
    setAiTool("Mask edge refinement");
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
    w->setFuture(QtConcurrent::run(&m_refinePool,
        [src,mask]()->QImage{return refineMaskEdgesOcv(src,mask);}));
#else
    setAiStatus("Edge refinement requires OpenCV");
#endif
}

// Creates a real, independently-paintable mask via DocumentModel::addMask()
// and switches editing focus to it. Sliders for a target id with no
// Adjustment entries yet naturally read 0 (see
// DocumentModel::scalarAdjustmentForTarget), satisfying "new mask resets
// sliders to zero" without any extra bookkeeping.
void DocumentController::addNewMaskTarget(const QString& targetLayerId) {
    if (!m_document.hasDocument()) return;
    const QString name = QString("Mask %1").arg(m_document.masks().size() + 1);
    const QString id = m_document.addMask(name, targetLayerId);
    setActiveAdjustmentTarget(id);
}

void DocumentController::requestAiMask(double x,double y) {
    if (!m_document.hasDocument()||m_aiBusy) return;
    setAiTool("Subject mask");
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
    setAiTool("Object removal");
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
    setAiTool("AI Upscale ×4");
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
    // DocumentModel::deleteLayer() removes any masks scoped to this
    // layer (see its own comment) via the generic changed() signal,
    // which this controller's changed()-forwarding only maps to
    // documentChanged/adjustmentsChanged/layersChanged -- NOT
    // maskChanged, which is what maskList/adjustmentTargets/hasMask/
    // maskUrl all depend on for their NOTIFY. Without this, those stay
    // stale: a deleted layer's mask keeps showing up in the Masks
    // dropdown even though the underlying data is already gone. Also
    // clear the active target if it pointed at one of the
    // now-removed masks, same as resyncAfterStructuralHistory() already
    // does for undo/redo of a structural change.
    clearAdjustmentTargetIfDangling();
    syncBrushEngineToTarget(m_activeAdjustmentTarget);
    emit maskChanged();
}
void DocumentController::setLayerOpacity(const QString& id,double o){m_document.setLayerOpacity(id,o);}
void DocumentController::setLayerVisible(const QString& id,bool v){m_document.setLayerVisible(id,v);}
void DocumentController::renameLayer(const QString& id,const QString& name){m_document.setLayerName(id,name);}
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
    if (qEnvironmentVariableIsSet("LUMEN_PROFILE"))
        qInfo().noquote() << "PROFILE transform event id=" << id;
    m_document.setLayerTransform(id, posX, posY, scaleX, scaleY, rotation);
}
void DocumentController::beginLayerTransformEdit() {
    if (m_layerTransformEditOpen) return;
    m_layerTransformEditOpen = true;
    m_previewDebounce->stop();
    m_hqTimer->stop();
    ++m_previewRequestId;
    *m_cancelFlag = true;
    *m_hqCancelFlag = true;
    m_document.beginHistoryTransaction("Transform layer");
}
void DocumentController::commitLayerTransformEdit() {
    if (!m_layerTransformEditOpen) return;
    m_layerTransformEditOpen = false;
    // Pushes (at most) ONE undo step for the whole drag -- DocumentModel
    // itself detects and skips a no-op interaction (press without moving),
    // via transactionChangedAnything()'s layer check.
    m_document.commitHistoryTransaction();
    if (m_transformUiTimer->isActive()) m_transformUiTimer->stop();
    emit layersChanged();
    m_previewDebounce->start();
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
        result.push_back({mask.mask, adjs, mask.targetLayerId});
    }
    return result;
}

// ── Preview (LQ) ──────────────────────────────────────────────────────────────
void DocumentController::rebuildPreview() {
    QElapsedTimer profileTimer;
    const bool profile = qEnvironmentVariableIsSet("LUMEN_PROFILE");
    if (profile) profileTimer.start();
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
        [pipeline=m_renderPipeline,src,globalAdjs,maskAdjLayers,layers,layerImages,cancelled,profile](){
            QElapsedTimer t; if (profile) t.start();
            if (profile) qInfo().noquote() << "PROFILE preview worker begin";
            QImage out = pipeline.renderWithLayers(src, globalAdjs, maskAdjLayers, layers, layerImages,
                                                   QSize(1400,1050), cancelled);
            if (profile) qInfo().noquote() << "PROFILE preview worker ms=" << t.nsecsElapsed()/1000000.0;
            return out;
        }));
    if (profile)
        qInfo().noquote() << "PROFILE rebuildPreview dispatch ms="
                          << profileTimer.nsecsElapsed()/1000000.0;
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
```

---

# File: `app\resources\qml\Main.qml`
```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1440; height: 920
    visible: true
    // Part 4: frameless/borderless window with a custom title bar (merged
    // into the existing toolbar below, rather than adding a second bar --
    // see the toolbar's own comment). Move/minimize/maximize/close are all
    // implemented via Qt's own recommended, cross-platform-safe mechanism
    // (Window.startSystemMove()/startSystemResize(), and the visibility
    // property) rather than any platform-specific code.
    flags: Qt.Window | Qt.FramelessWindowHint
    title: documentController.hasDocument
        ? "LumenForge \u2014 " + documentController.sourceName
        : "LumenForge"
    color: "#0f1117"

    property real   zoom:            1.0
    property real   brushRadius:     50
    property string lastSourceName:  ""
    property int    bottomTab:       0    // 0=Layers 1=History 2=Masks 3=Filmstrip

    // ---------------------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------------------

    // Extract filename from any path (handles both / and \ separators).
    // Replaces the removed Qt.fileInfo() which does not exist in Qt 6 QML.
    function baseFileName(path) {
        return path.replace(/\\/g, '/').split('/').pop() || path;
    }

    // Single source of truth for the breathing room around the image --
    // previously this was a hardcoded "80" duplicated across five
    // different places (Flickable content size, wheel-zoom math twice
    // over, and the frame Rectangle's size) AND compounded with a
    // separate 0.95 shrink factor in fitZoom(), which together produced
    // the large unused border around the image: at "Fit" zoom the image
    // was already shrunk to 95% of the viewport, then an ADDITIONAL 80px
    // of padding was added on top of that. Professional editors (Photoshop,
    // Lightroom, Affinity, Pixelmator) use a thin, fixed gutter and let
    // Fit actually fill the rest of the viewport -- this single constant
    // is that gutter, and fitZoom() below now computes zoom directly from
    // "the space left after the gutter" instead of an arbitrary percentage.
    readonly property real canvasPadding: 24

    // Fit-to-canvas zoom uses the source image dimensions exposed by the
    // controller so the calculation is independent of the preview JPEG size.
    function fitZoom() {
        if (!documentController.hasDocument) return 1.0;
        const w = documentController.sourceWidth;
        const h = documentController.sourceHeight;
        if (w <= 0 || h <= 0) return 1.0;
        const availW = Math.max(1, canvasFlick.width  - root.canvasPadding);
        const availH = Math.max(1, canvasFlick.height - root.canvasPadding);
        return Math.min(availW / w, availH / h);
    }

    // Reset zoom only when a genuinely new document is opened, not on every
    // preview refresh (the JPEG URL change also triggers sourceSize updates).
    Connections {
        target: documentController
        function onDocumentChanged() {
            const n = documentController.sourceName;
            if (n !== root.lastSourceName && n !== "No image loaded") {
                root.lastSourceName = n;
                Qt.callLater(function() { root.zoom = root.fitZoom(); });
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Dialogs
    // ---------------------------------------------------------------------------
    FileDialog { id: openImageDialog; title: "Open image"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp *.cr2 *.cr3 *.nef *.arw *.dng *.raf *.orf *.rw2)","All files (*)"]
        onAccepted: documentController.openImage(selectedFile) }
    FileDialog { id: openProjectDialog; title: "Open project"
        nameFilters: ["LumenForge project (*.lfproj)"]
        onAccepted: documentController.loadProject(selectedFile) }
    FileDialog { id: saveProjectDialog; title: "Save project"
        fileMode: FileDialog.SaveFile; defaultSuffix: "lfproj"
        nameFilters: ["LumenForge project (*.lfproj)"]
        onAccepted: documentController.saveProject(selectedFile) }
    FileDialog { id: exportDialog; title: "Export image"
        fileMode: FileDialog.SaveFile; defaultSuffix: "png"
        nameFilters: ["PNG (*.png)","JPEG (*.jpg)","WebP (*.webp)"]
        onAccepted: documentController.exportImage(selectedFile) }
    FileDialog { id: addLayerDialog; title: "Add image layer"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp)"]
        onAccepted: documentController.addImageLayer(selectedFile) }
    Dialog { id: recoveryDialog; title: "Recover unsaved work?"; modal: true
        visible: documentController.hasPendingRecovery
        anchors.centerIn: parent
        Label { text: "An autosaved project was found. Recover it?"; color: "#e2e8f0" }
        footer: DialogButtonBox {
            Button { text: "Recover"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: { documentController.recoverProject(); recoveryDialog.close() } }
            Button { text: "Discard"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: { documentController.discardRecovery(); recoveryDialog.close() } }
        }
    }

    // ---------------------------------------------------------------------------
    // Shortcuts
    // ---------------------------------------------------------------------------
    Shortcut { sequences: [StandardKey.Open];  onActivated: openImageDialog.open() }
    Shortcut { sequences: [StandardKey.Save];  onActivated: saveProjectDialog.open() }
    Shortcut { sequence:  "Ctrl+E";            onActivated: exportDialog.open() }
    Shortcut { sequence:  "Ctrl+0";            onActivated: root.zoom = 1.0 }
    Shortcut { sequence:  "Ctrl++";            onActivated: root.zoom = Math.min(4.0, root.zoom * 1.12) }
    Shortcut { sequence:  "Ctrl+-";            onActivated: root.zoom = Math.max(0.1, root.zoom / 1.12) }
    Shortcut { sequences: [StandardKey.Undo];  onActivated: documentController.undo() }
    Shortcut { sequences: [StandardKey.Redo];  onActivated: documentController.redo() }
    Shortcut { sequence:  "\\";               onActivated: documentController.showOriginal = !documentController.showOriginal }
    Shortcut { sequence:  "Escape";            onActivated: documentController.activeTool = 0 }
    Shortcut { sequence: "B"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool===1?0:1) }
    Shortcut { sequence: "E"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool===2?0:2) }
    Shortcut { sequence: "G"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool===3?0:3) }
    Shortcut { sequence: "R"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool===4?0:4) }
    Shortcut { sequence: "C"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool===5?0:5) }
    Shortcut { sequence: "T"; onActivated: if (documentController.hasDocument) documentController.activeTool = (documentController.activeTool===6?0:6) }
    Shortcut { sequence: "Return"
        enabled: documentController.activeTool===5 && documentController.hasDocument
        onActivated: cropOverlayItem.confirm() }

    // ---------------------------------------------------------------------------
    // Root layout
    // ---------------------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Compact toolbar -- also serves as the custom title bar (Part 4):
        // dragging any empty area of it moves the window, since the
        // native title bar is gone (flags: Qt.FramelessWindowHint above).
        Rectangle {
            Layout.fillWidth: true
            height: 42
            color: "#13161f"
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#1e2438" }
            // Placed BEHIND the RowLayout below (declared first = painted
            // and hit-tested first for non-overlapping regions, so any
            // actual button drawn on top of it still gets priority) --
            // only truly empty toolbar space (the spacer Item, margins)
            // ends up draggable, never a button.
            MouseArea {
                anchors.fill: parent
                onPressed: (mouse) => { if (mouse.button === Qt.LeftButton) root.startSystemMove(); }
                onDoubleClicked: (mouse) => {
                    if (mouse.button === Qt.LeftButton)
                        root.visibility = (root.visibility === Window.Maximized) ? Window.Windowed : Window.Maximized;
                }
            }
            RowLayout {
                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                spacing: 4
                Rectangle { width: 6; height: 24; radius: 3
                    gradient: Gradient { GradientStop{position:0;color:"#6366f1"} GradientStop{position:1;color:"#818cf8"} } }
                Item { width: 6 }
                Button { text: "Open"
                    background: Rectangle { color: parent.hovered?"#1e2438":"#161a28"; radius:6; border.color:"#252d45" }
                    contentItem: Label { text:"Open"; color:"#c8d0e0"; font.pixelSize:12; horizontalAlignment:Text.AlignHCenter }
                    implicitHeight: 28; implicitWidth: 54
                    onClicked: openImageDialog.open() }
                Button { text: "Project"
                    background: Rectangle { color: parent.hovered?"#1e2438":"#161a28"; radius:6; border.color:"#252d45" }
                    contentItem: Label { text:"Project"; color:"#c8d0e0"; font.pixelSize:12; horizontalAlignment:Text.AlignHCenter }
                    implicitHeight: 28; implicitWidth: 60
                    onClicked: openProjectDialog.open() }
                Button { text: "Save"; enabled: documentController.hasDocument
                    background: Rectangle { color: parent.hovered?"#1e2438":"#161a28"; radius:6; border.color:"#252d45" }
                    contentItem: Label { text:"Save"; color:parent.enabled?"#c8d0e0":"#4a5268"; font.pixelSize:12; horizontalAlignment:Text.AlignHCenter }
                    implicitHeight: 28; implicitWidth: 50
                    onClicked: saveProjectDialog.open() }
                Button { text: "Export"; enabled: documentController.hasDocument
                    background: Rectangle { color: parent.hovered?"#252d6a":"#1c2058"; radius:6; border.color:"#3d41a0" }
                    contentItem: Label { text:"Export"; color:parent.enabled?"#c7d2fe":"#4a5268"; font.pixelSize:12; horizontalAlignment:Text.AlignHCenter }
                    implicitHeight: 28; implicitWidth: 58
                    onClicked: exportDialog.open() }
                Item { Layout.fillWidth: true }
                Item { width: 12 }
                Rectangle { width:1; height:20; color:"#252d45" }
                Item { width: 4 }
                // Window controls (Part 4) -- minimize / maximize-restore / close,
                // standard Windows right-to-left ordering, replacing the native
                // title bar's own buttons now that it's gone.
                Button {
                    implicitWidth: 32; implicitHeight: 28; flat: true
                    contentItem: Label { text:"\u2013"; color:"#8892a4"; font.pixelSize:14
                        horizontalAlignment:Text.AlignHCenter; verticalAlignment:Text.AlignVCenter }
                    background: Rectangle { color: parent.hovered?"#1e2438":"transparent"; radius:4 }
                    onClicked: root.visibility = Window.Minimized
                }
                Button {
                    implicitWidth: 32; implicitHeight: 28; flat: true
                    contentItem: Label { text: root.visibility===Window.Maximized?"\u29c9":"\u25a1"
                        color:"#8892a4"; font.pixelSize:11
                        horizontalAlignment:Text.AlignHCenter; verticalAlignment:Text.AlignVCenter }
                    background: Rectangle { color: parent.hovered?"#1e2438":"transparent"; radius:4 }
                    onClicked: root.visibility = (root.visibility===Window.Maximized) ? Window.Windowed : Window.Maximized
                }
                Button {
                    implicitWidth: 32; implicitHeight: 28; flat: true
                    contentItem: Label { text:"\u2715"; color:"#8892a4"; font.pixelSize:12
                        horizontalAlignment:Text.AlignHCenter; verticalAlignment:Text.AlignVCenter }
                    background: Rectangle { color: parent.hovered?"#c0392b":"transparent"; radius:4 }
                    onClicked: root.close()
                }
            }
        }

        // Main SplitView
        SplitView {
            id: mainSplit
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            handle: Rectangle {
                implicitHeight: 5
                color: SplitHandle.pressed ? "#6366f1"
                     : SplitHandle.hovered ? "#2d3050" : "#13161f"
                Rectangle { width:36; height:2; radius:1; anchors.centerIn:parent; color:"#2a3050" }
            }

            Component.onCompleted: {
                bottomPanel.SplitView.preferredHeight = 160;
            }

            // Top: tool rail + canvas + adjustments
            Item {
                SplitView.fillHeight:   true
                SplitView.minimumHeight: 180
                RowLayout { anchors.fill: parent; spacing: 0

                    // Tool rail
                    Rectangle {
                        Layout.preferredWidth: 68; Layout.fillHeight: true
                        color: "#13161f"
                        Rectangle { anchors.right:parent.right; width:1; height:parent.height; color:"#1e2438" }
                        ColumnLayout {
                            anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
                            anchors.topMargin: 12; spacing: 4
                            Repeater {
                                model: [
                                    {icon:"\u2725",tip:"Navigate",          tool:0},
                                    {icon:"\u2b24",tip:"Brush Mask  [B]",   tool:1},
                                    {icon:"\u25ef",tip:"Erase Mask  [E]",   tool:2},
                                    {icon:"\u25ac",tip:"Gradient  [G]",     tool:3},
                                    {icon:"\u25ce",tip:"Radial  [R]",       tool:4},
                                    {icon:"\u2291",tip:"Crop  [C]",         tool:5},
                                    {icon:"\u2b1a",tip:"Transform  [T]",    tool:6},
                                ]
                                delegate: Button {
                                    Layout.preferredWidth:46; Layout.preferredHeight:38
                                    enabled: modelData.tool===0||documentController.hasDocument
                                    checkable: modelData.tool>0
                                    checked: modelData.tool>0&&documentController.activeTool===modelData.tool
                                    ToolTip.visible:hovered; ToolTip.text:modelData.tip; ToolTip.delay:500
                                    onClicked: {
                                        if (modelData.tool===0) documentController.activeTool=0
                                        else documentController.activeTool=(documentController.activeTool===modelData.tool)?0:modelData.tool
                                    }
                                    background: Rectangle { radius:7
                                        color: parent.checked?"#4f46e5":parent.hovered?"#1e2438":"transparent"
                                        Behavior on color { ColorAnimation{duration:120} }
                                    }
                                    contentItem: Label { text:modelData.icon; font.pixelSize:14
                                        color: parent.checked?"#fff":parent.enabled?"#8892a4":"#2a3050"
                                        horizontalAlignment:Text.AlignHCenter; verticalAlignment:Text.AlignVCenter }
                                }
                            }
                            Rectangle { width:42; height:1; color:"#1e2438"; Layout.alignment:Qt.AlignHCenter }
                            Label { text:"SIZE"; color:"#3a4566"; font.pixelSize:8; Layout.alignment:Qt.AlignHCenter }
                            Slider {
                                from:5; to:200; value:root.brushRadius; orientation:Qt.Vertical
                                implicitHeight:80; Layout.alignment:Qt.AlignHCenter
                                visible: documentController.activeTool===1||documentController.activeTool===2
                                onMoved: root.brushRadius=value
                            }
                        }
                    }

                    // Canvas
                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        color: "#0a0c12"
                        Flickable {
                            id: canvasFlick; anchors.fill: parent
                            // Content size is driven by the zoomed source dimensions,
                            // which never change when toggling Before/After.
                            contentWidth:  Math.max(width,
                                (documentController.hasDocument ? documentController.sourceWidth  * root.zoom : 0) + root.canvasPadding)
                            contentHeight: Math.max(height,
                                (documentController.hasDocument ? documentController.sourceHeight * root.zoom : 0) + root.canvasPadding)
                            clip: true
                            property bool handPanActive: false
                            property int panProfileEvents: 0
                            property double panProfileStartMs: 0
                            interactive: documentController.activeTool===0 && !handPanActive

                            WheelHandler {
                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                property int profileEvents: 0
                                property double profileStartMs: 0
                                onWheel: (event) => {
                                    if (profileStartMs === 0) profileStartMs = Date.now();
                                    profileEvents++;
                                    if (profileEvents % 20 === 0)
                                        console.log("PROFILE zoom QML events=", profileEvents,
                                                    "elapsedMs=", Date.now() - profileStartMs)
                                    const fac = event.angleDelta.y > 0 ? 1.12 : (1.0 / 1.12);
                                    const oldZ = root.zoom;
                                    const newZ = Math.min(4.0, Math.max(0.1, oldZ * fac));
                                    if (Math.abs(newZ - oldZ) < 0.001) return;

                                    const srcW = documentController.hasDocument ? documentController.sourceWidth  : 0;
                                    const srcH = documentController.hasDocument ? documentController.sourceHeight : 0;
                                    if (srcW <= 0) { root.zoom = newZ; return; }

                                    // Content box size at current zoom
                                    const cW = Math.max(canvasFlick.width,  srcW * oldZ + root.canvasPadding);
                                    const cH = Math.max(canvasFlick.height, srcH * oldZ + root.canvasPadding);
                                    // Image top-left corner in content space (centred)
                                    const imgLeft = (cW - srcW * oldZ) * 0.5;
                                    const imgTop  = (cH - srcH * oldZ) * 0.5;
                                    // Cursor position in content space
                                    const curCX = canvasFlick.contentX + event.x;
                                    const curCY = canvasFlick.contentY + event.y;
                                    // Same point expressed in source-image pixels (zoom-invariant)
                                    const imgPxX = (curCX - imgLeft) / oldZ;
                                    const imgPxY = (curCY - imgTop)  / oldZ;

                                    root.zoom = newZ;

                                    // New content box size
                                    const nW = Math.max(canvasFlick.width,  srcW * newZ + root.canvasPadding);
                                    const nH = Math.max(canvasFlick.height, srcH * newZ + root.canvasPadding);
                                    // New image top-left
                                    const nImgLeft = (nW - srcW * newZ) * 0.5;
                                    const nImgTop  = (nH - srcH * newZ) * 0.5;
                                    // Keep the same source pixel under the cursor
                                    canvasFlick.contentX = Math.max(0, Math.min(nW - canvasFlick.width,
                                                            nImgLeft + imgPxX * newZ - event.x));
                                    canvasFlick.contentY = Math.max(0, Math.min(nH - canvasFlick.height,
                                                            nImgTop  + imgPxY * newZ - event.y));
                                }
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                width:  Math.max(160,
                                    (documentController.hasDocument ? documentController.sourceWidth  * root.zoom : 0) + root.canvasPadding)
                                height: Math.max(120,
                                    (documentController.hasDocument ? documentController.sourceHeight * root.zoom : 0) + root.canvasPadding)
                                color:"#08090e"; border.color:"#1a1e2e"; radius:3

                                Image {
                                    id: imagePreview; anchors.centerIn: parent
                                    source: documentController.imageUrl
                                    cache:false; fillMode:Image.PreserveAspectFit; asynchronous:true; smooth:true
                                    // Issue 2 fix: size from source document dimensions, NOT sourceSize.
                                    // Before/After toggle changes the URL (different JPEG resolutions) but
                                    // should never change the display size or reset zoom/pan.
                                    width:  documentController.hasDocument ? documentController.sourceWidth  * root.zoom : 0
                                    height: documentController.hasDocument ? documentController.sourceHeight * root.zoom : 0
                                }

                                MaskCanvas {
                                    id: maskOverlay; anchors.centerIn: parent
                                    width: imagePreview.width; height: imagePreview.height
                                    visible: documentController.hasDocument &&
                                             documentController.activeTool>=1 &&
                                             documentController.activeTool<=4
                                    docCtrl:      documentController
                                    brushRadius:  root.brushRadius
                                    eraseMode:    documentController.activeTool===2
                                    paintEnabled: documentController.activeTool===1 ||
                                                  documentController.activeTool===2
                                }
                                CropOverlay {
                                    id: cropOverlayItem; anchors.centerIn: parent
                                    width: imagePreview.width; height: imagePreview.height
                                    visible: documentController.hasDocument &&
                                             documentController.activeTool===5
                                    docCtrl: documentController
                                }
                                LayerTransformOverlay {
                                    id: layerTransformOverlayItem; anchors.centerIn: parent
                                    width: imagePreview.width; height: imagePreview.height
                                    visible: documentController.hasDocument &&
                                             documentController.activeTool===6
                                    docCtrl: documentController
                                }
                                Label { anchors.centerIn:parent
                                    visible:!documentController.hasDocument
                                    text:"Open an image to begin"; color:"#3a4566"; font.pixelSize:20 }
                            }

                            MouseArea {
                                anchors.fill:parent; acceptedButtons:Qt.RightButton|Qt.MiddleButton
                                propagateComposedEvents:true; property real lx:0; property real ly:0
                                cursorShape: pressed?Qt.ClosedHandCursor:Qt.ArrowCursor
                                onPressed: (m)=>{
                                    canvasFlick.handPanActive = true
                                    canvasFlick.panProfileEvents = 0
                                    canvasFlick.panProfileStartMs = Date.now()
                                    lx=m.x;ly=m.y;m.accepted=true
                                }
                                onPositionChanged: (m)=>{
                                    canvasFlick.panProfileEvents++
                                    if (canvasFlick.panProfileEvents % 60 === 0)
                                        console.log("PROFILE pan QML events=", canvasFlick.panProfileEvents,
                                                    "elapsedMs=", Date.now() - canvasFlick.panProfileStartMs)
                                    canvasFlick.contentX=Math.max(0,Math.min(canvasFlick.contentWidth-canvasFlick.width,  canvasFlick.contentX-(m.x-lx)));
                                    canvasFlick.contentY=Math.max(0,Math.min(canvasFlick.contentHeight-canvasFlick.height,canvasFlick.contentY-(m.y-ly)));
                                    lx=m.x;ly=m.y;
                                }
                                onReleased: canvasFlick.handPanActive = false
                                onCanceled: canvasFlick.handPanActive = false
                            }
                        }

                        // Canvas bottom bar
                        Row {
                            anchors.left:parent.left; anchors.bottom:parent.bottom; anchors.margins:12; spacing:5
                            ZoomControl { id: zoomControl }
                            Button {
                                // Issue: label describes what you ARE SEEING
                                // showOriginal=true  → label says "Before" (you're seeing the original)
                                // showOriginal=false → label says "After"  (you're seeing the edited version)
                                text: documentController.showOriginal ? "Before" : "After"
                                enabled:documentController.hasDocument; implicitHeight:26
                                onClicked:documentController.showOriginal=!documentController.showOriginal
                                background:Rectangle{color:parent.hovered?"#1e2438":"#0f1219";radius:5;border.color:"#1e2438"}
                                contentItem:Label{text:parent.text;color:"#6b7a99";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter} }
                            Button { text:"Delete mask"
                                enabled:documentController.hasDocument&&documentController.hasMask; implicitHeight:26
                                onClicked:documentController.clearMask()
                                background:Rectangle{color:parent.hovered?"#2a1414":"#0f1219";radius:5;border.color:"#1e2438"}
                                contentItem:Label{text:"Delete mask";color:"#f07070";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter} }
                        }
                    }

                    // Adjustments panel
                    Rectangle {
                        Layout.preferredWidth: 310; Layout.fillHeight: true
                        color:"#13161f"
                        Rectangle { anchors.left:parent.left; width:1; height:parent.height; color:"#1e2438" }
                        ScrollView { anchors.fill:parent; clip:true
                            ColumnLayout { width:310; spacing:10
                                Item { Layout.fillWidth:true; Layout.preferredHeight:46
                                    Label { anchors.left:parent.left; anchors.leftMargin:16; anchors.verticalCenter:parent.verticalCenter
                                        text:"Adjustments"; color:"#e2e8f0"; font.pixelSize:16; font.weight:Font.DemiBold } }
                                // Issue 5: shows which target the sliders below are currently editing.
                                // Kept in sync with the Masks tab selector further down.
                                RowLayout { Layout.leftMargin:12; Layout.rightMargin:12; Layout.fillWidth:true; spacing:6
                                    Label { text:"Editing:"; color:"#3a4566"; font.pixelSize:10 }
                                    Label {
                                        text: {
                                            const list = documentController.adjustmentTargets;
                                            for (let i = 0; i < list.length; ++i)
                                                if (list[i].id === documentController.activeAdjustmentTarget) return list[i].name;
                                            return "Full Image";
                                        }
                                        color:"#6366f1"; font.pixelSize:10; font.weight:Font.DemiBold
                                        Layout.fillWidth:true; elide:Text.ElideRight
                                    }
                                }
                                Label{text:"TRANSFORM";color:"#3a4566";font.pixelSize:10;Layout.leftMargin:16}
                                GridLayout { Layout.leftMargin:12; Layout.rightMargin:12; Layout.fillWidth:true; columns:2; rowSpacing:5; columnSpacing:5
                                    Button{text:"\u21ba Left";  Layout.fillWidth:true;implicitHeight:28;enabled:documentController.hasDocument;onClicked:documentController.rotateCounterClockwise()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                    Button{text:"\u21bb Right"; Layout.fillWidth:true;implicitHeight:28;enabled:documentController.hasDocument;onClicked:documentController.rotateClockwise()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                    Button{text:"\u21d4 Flip H";Layout.fillWidth:true;implicitHeight:28;enabled:documentController.hasDocument;onClicked:documentController.flipHorizontal()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                    Button{text:"\u21d5 Flip V"; Layout.fillWidth:true;implicitHeight:28;enabled:documentController.hasDocument;onClicked:documentController.flipVertical()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                }
                                RowLayout{Layout.leftMargin:12;Layout.rightMargin:12;Layout.fillWidth:true;spacing:5
                                    Button{Layout.fillWidth:true;text:"Undo";enabled:documentController.canUndo;implicitHeight:28;onClicked:documentController.undo()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                    Button{Layout.fillWidth:true;text:"Redo";enabled:documentController.canRedo;implicitHeight:28;onClicked:documentController.redo()
                                        background:Rectangle{color:parent.hovered?"#1e2438":"#171c2a";radius:6;border.color:"#252d45"}
                                        contentItem:Label{text:parent.text;color:"#8892a4";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                }
                                Label{text:"LIGHT";color:"#3a4566";font.pixelSize:10;Layout.leftMargin:16}
                                AdjustmentSlider{label:"Brightness"; from:-100;to:100; value:documentController.brightness;  onMoved:(v)=>documentController.brightness =v}
                                AdjustmentSlider{label:"Exposure";   from:-3;  to:3;   value:documentController.exposure;    onMoved:(v)=>documentController.exposure   =v}
                                AdjustmentSlider{label:"Contrast";   from:-100;to:100; value:documentController.contrast;    onMoved:(v)=>documentController.contrast   =v}
                                AdjustmentSlider{label:"Highlights"; from:-100;to:100; value:documentController.highlights;  onMoved:(v)=>documentController.highlights =v}
                                AdjustmentSlider{label:"Shadows";    from:-100;to:100; value:documentController.shadows;     onMoved:(v)=>documentController.shadows    =v}
                                AdjustmentSlider{label:"Whites";     from:-100;to:100; value:documentController.whites;      onMoved:(v)=>documentController.whites     =v}
                                // Issue 3 fix: Blacks slider range clamped to -7..+7 (maps to -100..+100 internally).
                                // The raw -100..+100 range was far too strong; this limits it to a usable zone.
                                // NOTE: the backend gain math itself was also fixed (see RenderPipeline.cpp) —
                                // the additive offset was missing a /100 normalization that every sibling
                                // adjustment (contrast, highlights, shadows, whites) already has, causing
                                // even tiny slider moves to blow every pixel to full white/black.
                                AdjustmentSlider{label:"Blacks"; from:-7; to:7;
                                    value:documentController.blacks * 7.0 / 100.0
                                    onMoved:(v) => documentController.blacks = v * 100.0 / 7.0 }
                                Label{text:"COLOR";color:"#3a4566";font.pixelSize:10;Layout.leftMargin:16}
                                AdjustmentSlider{label:"Saturation"; from:-100;to:100; value:documentController.saturation;  onMoved:(v)=>documentController.saturation =v}
                                AdjustmentSlider{label:"Vibrance";   from:-100;to:100; value:documentController.vibrance;    onMoved:(v)=>documentController.vibrance   =v}
                                AdjustmentSlider{label:"Temperature";from:-100;to:100; value:documentController.temperature; onMoved:(v)=>documentController.temperature=v}
                                AdjustmentSlider{label:"Tint";       from:-100;to:100; value:documentController.tint;        onMoved:(v)=>documentController.tint       =v}
                                Label{text:"DETAIL";color:"#3a4566";font.pixelSize:10;Layout.leftMargin:16}
                                AdjustmentSlider{label:"Noise Reduction";from:0;to:100;value:documentController.noiseReduction;onMoved:(v)=>documentController.noiseReduction=v}
                                AdjustmentSlider{label:"Sharpening";     from:0;to:100;value:documentController.sharpening;    onMoved:(v)=>documentController.sharpening    =v}
                                Button{text:"Reset All";enabled:documentController.hasDocument;implicitHeight:28
                                    Layout.leftMargin:12;Layout.rightMargin:12;Layout.fillWidth:true
                                    onClicked:documentController.resetAdjustments()
                                    background:Rectangle{color:parent.hovered?"#2a1414":"#171c2a";radius:6;border.color:"#252d45"}
                                    contentItem:Label{text:"Reset All";color:"#f07070";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                Label{text:"AI TOOLS";color:"#3a4566";font.pixelSize:10;Layout.leftMargin:16}
                                RowLayout { Layout.fillWidth:true; Layout.leftMargin:12; Layout.rightMargin:12; spacing:6
                                    BusyIndicator { running:documentController.aiBusy; visible:documentController.aiBusy; implicitWidth:18;implicitHeight:18 }
                                    Label { text: documentController.aiTool.length>0
                                                  ? documentController.aiTool + ": " + documentController.aiStatus
                                                  : documentController.aiStatus
                                            color:"#f59e0b"; font.pixelSize:10; Layout.fillWidth:true; elide:Text.ElideRight
                                            visible:documentController.aiStatus.length>0 }
                                }
                                Button{text:"Subject mask";enabled:documentController.hasDocument&&!documentController.aiBusy;implicitHeight:28
                                    Layout.leftMargin:12;Layout.rightMargin:12;Layout.fillWidth:true
                                    onClicked:documentController.requestAiMask(imagePreview.width/2,imagePreview.height/2)
                                    background:Rectangle{color:parent.hovered?"#252d6a":"#1c2058";radius:6;border.color:"#3d41a0"}
                                    contentItem:Label{text:parent.text;color:parent.enabled?"#c7d2fe":"#4a5268";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                Button{text:"Object removal";enabled:documentController.hasDocument&&documentController.hasMask&&!documentController.aiBusy;implicitHeight:28
                                    Layout.leftMargin:12;Layout.rightMargin:12;Layout.fillWidth:true
                                    onClicked:documentController.applyInpaint()
                                    background:Rectangle{color:parent.hovered?"#252d6a":"#1c2058";radius:6;border.color:"#3d41a0"}
                                    contentItem:Label{text:parent.text;color:parent.enabled?"#c7d2fe":"#4a5268";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                Button{text:"AI Upscale \u00d74";enabled:documentController.hasDocument&&!documentController.aiBusy;implicitHeight:28
                                    Layout.leftMargin:12;Layout.rightMargin:12;Layout.fillWidth:true
                                    onClicked:documentController.applyUpscale()
                                    background:Rectangle{color:parent.hovered?"#252d6a":"#1c2058";radius:6;border.color:"#3d41a0"}
                                    contentItem:Label{text:parent.text;color:parent.enabled?"#c7d2fe":"#4a5268";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                Item{Layout.preferredHeight:16}
                            }
                        }
                    }
                } // RowLayout
            } // Item (top split)

            // Bottom panel
            Rectangle {
                id: bottomPanel
                SplitView.preferredHeight: 160
                SplitView.minimumHeight:   30
                color: "#13161f"
                Rectangle { anchors.top:parent.top; width:parent.width; height:1; color:"#1e2438" }

                ColumnLayout {
                    anchors.fill: parent; spacing: 0

                    // Tab strip
                    Row {
                        id: tabStrip
                        Layout.fillWidth: true
                        height: 28
                        Repeater {
                            model: ["Layers","History","Masks","Filmstrip"]
                            delegate: Button {
                                text: modelData; flat: true
                                implicitWidth: 80; implicitHeight: 28
                                checked: root.bottomTab===index
                                onClicked: root.bottomTab=index
                                background: Rectangle {
                                    color: parent.checked?"#171c2a":"transparent"
                                    Rectangle { anchors.bottom:parent.bottom; width:parent.width; height:2
                                        color: parent.parent.checked?"#6366f1":"transparent" }
                                }
                                contentItem: Label { text:parent.text; font.pixelSize:11
                                    color:parent.checked?"#c8d0e0":"#4a5268"
                                    horizontalAlignment:Text.AlignHCenter }
                            }
                        }
                        Rectangle { Layout.fillWidth:true; height:1; anchors.bottom:parent.bottom; color:"transparent" }
                    }
                    Rectangle { height:1; Layout.fillWidth:true; color:"#1e2438" }

                    // Tab content
                    StackLayout {
                        currentIndex: root.bottomTab
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true

                        // LAYERS
                        Item {
                            ColumnLayout { anchors.fill:parent; anchors.margins:6; spacing:4
                                RowLayout {
                                    Label{text:"Layers";color:"#c8d0e0";font.pixelSize:11;font.weight:Font.DemiBold;Layout.fillWidth:true}
                                    Button{text:"+";flat:true;implicitWidth:22;implicitHeight:22;enabled:documentController.hasDocument
                                        onClicked:addLayerDialog.open()
                                        contentItem:Label{text:"+";color:"#6366f1";font.pixelSize:16;horizontalAlignment:Text.AlignHCenter}}
                                }
                                ListView { id:layerList; Layout.fillWidth:true; Layout.fillHeight:true; clip:true
                                    model: documentController.layerModel
                                    delegate: Rectangle {
                                        width:layerList.width; height:28
                                        property bool editingName: false
                                        // Layer Transform Gizmo (stage 1): highlight the row that
                                        // matches documentController.selectedLayerId, and let
                                        // clicking anywhere in the row (not just the Buttons/Slider)
                                        // select it too -- gives a second, non-canvas way to select
                                        // a layer, and a visual channel to confirm canvas-click
                                        // selection is really reaching the backend.
                                        color: modelData.realId === documentController.selectedLayerId ? "#1a2040" : "transparent"
                                        Rectangle{anchors.bottom:parent.bottom;width:parent.width;height:1;color:"#13161f"}
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: documentController.selectedLayerId = modelData.realId
                                            onDoubleClicked: {
                                                documentController.selectedLayerId = modelData.realId
                                                editingName = true
                                                nameEditor.text = modelData.name
                                                nameEditor.selectAll()
                                                nameEditor.forceActiveFocus()
                                            }
                                        }
                                        RowLayout{anchors.fill:parent;anchors.margins:2;spacing:3
                                            Button{text:modelData.visible?"\u25c9":"\u25cb";flat:true;implicitWidth:22;implicitHeight:22
                                                onClicked:documentController.setLayerVisible(modelData.realId,!modelData.visible)
                                                contentItem:Label{text:parent.text;color:modelData.visible?"#6366f1":"#4a5268";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter}}
                                            Label{text:modelData.name;color:"#c8d0e0";font.pixelSize:11;Layout.fillWidth:true;elide:Text.ElideRight;visible:!editingName}
                                            TextInput {
                                                id:nameEditor
                                                visible:editingName
                                                text:modelData.name
                                                color:"#e2e8f0"
                                                font.pixelSize:11
                                                Layout.fillWidth:true
                                                selectByMouse:true
                                                clip:true
                                                onAccepted: {
                                                    documentController.renameLayer(modelData.realId, text)
                                                    editingName = false
                                                    focus = false
                                                }
                                                onEditingFinished: if (editingName) {
                                                    documentController.renameLayer(modelData.realId, text)
                                                    editingName = false
                                                    focus = false
                                                }
                                            }
                                            Slider{from:0;to:1;value:modelData.opacity;implicitWidth:55;implicitHeight:18
                                                onMoved:documentController.setLayerOpacity(modelData.realId,value)}
                                            Button{text:"\u2191";visible: !modelData.isBase;flat:true;implicitWidth:20;implicitHeight:22
                                                onClicked:documentController.moveLayerUp(modelData.realId)
                                                contentItem:Label{text:"\u2191";color:"#6b7a99";font.pixelSize:12;horizontalAlignment:Text.AlignHCenter}}
                                            Button{text:"\u2193";visible: !modelData.isBase;flat:true;implicitWidth:20;implicitHeight:22
                                                onClicked:documentController.moveLayerDown(modelData.realId)
                                                contentItem:Label{text:"\u2193";color:"#6b7a99";font.pixelSize:12;horizontalAlignment:Text.AlignHCenter}}
                                            Button{text:"\u2715";visible: !modelData.isBase;flat:true;implicitWidth:20;implicitHeight:22
                                                onClicked:documentController.deleteLayer(modelData.realId)
                                                contentItem:Label{text:"\u2715";color:"#f07070";font.pixelSize:10;horizontalAlignment:Text.AlignHCenter}}
                                        }
                                    }
                                }
                            }
                        }

                        // HISTORY
                        Item {
                            ListView {
                                anchors.fill:parent; clip:true
                                model: documentController.historyLog
                                // historyLog is now oldest-first (a live read of
                                // the actual undo stack, in the order those
                                // actions were performed -- see historyLabels()),
                                // so the CURRENT/most-recent entry is the LAST
                                // one, not index 0 as it was when this list was
                                // newest-first. Auto-scroll there whenever a new
                                // entry is added, otherwise it would land
                                // silently off-screen below the visible area.
                                onCountChanged: positionViewAtEnd()
                                delegate: Rectangle {
                                    width:ListView.view.width; height:22
                                    readonly property bool isCurrent: index===ListView.view.count-1
                                    color: isCurrent?"#1a2040":"transparent"
                                    RowLayout{anchors.fill:parent;anchors.leftMargin:10;anchors.rightMargin:6;spacing:8
                                        Rectangle{width:5;height:5;radius:2.5;color:parent.parent.isCurrent?"#6366f1":"#3a4566"}
                                        Label{text:modelData;color:parent.parent.isCurrent?"#c8d0e0":"#6b7a99";font.pixelSize:11;Layout.fillWidth:true;elide:Text.ElideRight}
                                    }
                                }
                                Label{anchors.centerIn:parent;visible:documentController.historyLog.length===0
                                    text:"No history yet";color:"#3a4566";font.pixelSize:12}
                            }
                        }

                        // MASKS
                        Item {
                            ColumnLayout{anchors.fill:parent;anchors.margins:8;spacing:8
                                // Header: title + the one explicit creation action for this
                                // panel, in the header row -- matching where the Layers tab
                                // already puts its own "+" button, for consistency.
                                RowLayout{
                                    Label{text:"Masks";color:"#c8d0e0";font.pixelSize:12;font.weight:Font.DemiBold;Layout.fillWidth:true}
                                    Button{
                                        id: addMaskButton
                                        text:"+ Add Mask"; implicitHeight:26
                                        enabled: documentController.hasDocument
                                        onClicked: addMaskMenu.open()
                                        background:Rectangle{color:parent.hovered?"#252d6a":"#1c2058";radius:6;border.color:"#3d41a0"}
                                        contentItem:Label{text:"+ Add Mask";color:"#c7d2fe";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter;leftPadding:8;rightPadding:8}

                                        // Part 5: explicit target chooser, opened directly from
                                        // the action that creates a mask -- the target is picked
                                        // HERE, at creation time, never inferred from whichever
                                        // layer happened to be selected earlier. This removes the
                                        // "selected layer silently resets, mask lands on the wrong
                                        // target" failure mode entirely: there is no longer any
                                        // state that can drift out of sync with the user's intent.
                                        Menu {
                                            id: addMaskMenu
                                            y: parent.height + 2
                                            background: Rectangle { implicitWidth: 170; color: "#171c2a"; border.color: "#252d45"; radius: 6 }
                                            MenuItem {
                                                text: "Base Image"
                                                onTriggered: documentController.addNewMaskTarget("")
                                                contentItem: Label{text:"Base Image";color:"#c8d0e0";font.pixelSize:11;leftPadding:10;verticalAlignment:Text.AlignVCenter}
                                            }
                                            Repeater {
                                                model: {
                                                    const layers = documentController.layerModel;
                                                    const overlays = [];
                                                    for (const l of layers) if (!l.isBase) overlays.push(l);
                                                    return overlays;
                                                }
                                                MenuItem {
                                                    text: modelData.name
                                                    onTriggered: documentController.addNewMaskTarget(modelData.realId)
                                                    contentItem: Label{text:modelData.name;color:"#c8d0e0";font.pixelSize:11;leftPadding:10;verticalAlignment:Text.AlignVCenter}
                                                }
                                            }
                                        }
                                    }
                                }
                                // Which target the Adjustments panel's sliders are currently
                                // pointed at -- a separate concern from CREATING a mask (see
                                // the header above), so it gets its own clearly-labeled row
                                // rather than sharing space with the creation action.
                                RowLayout{Layout.fillWidth:true;spacing:8
                                    Label{text:"Editing:";color:"#5a6684";font.pixelSize:10}
                                    ComboBox{
                                        id: targetCombo
                                        Layout.fillWidth:true
                                        Layout.preferredWidth: 170
                                        implicitHeight: 32
                                        model: documentController.adjustmentTargets
                                        textRole: "name"
                                        enabled: documentController.hasDocument
                                        // Re-evaluated whenever adjustmentTargets or activeAdjustmentTarget
                                        // change (both are NOTIFYing Q_PROPERTYs) — keeps the combo in sync
                                        // when addNewMaskTarget() switches the active target programmatically.
                                        currentIndex: {
                                            const list = documentController.adjustmentTargets;
                                            for (let i = 0; i < list.length; ++i)
                                                if (list[i].id === documentController.activeAdjustmentTarget) return i;
                                            return 0;
                                        }
                                        // 'activated' only fires from direct user interaction, not from the
                                        // programmatic currentIndex binding above, so this cannot loop.
                                        onActivated: (index) => {
                                            const list = documentController.adjustmentTargets;
                                            if (index >= 0 && index < list.length)
                                                documentController.activeAdjustmentTarget = list[index].id;
                                        }
                                        background:Rectangle{color:"#171c2a";radius:7;border.color:"#252d45"}
                                        contentItem: Label{
                                            text: targetCombo.displayText; color:"#c8d0e0"; font.pixelSize:11
                                            leftPadding:10; verticalAlignment:Text.AlignVCenter
                                        }
                                    }
                                }
                                ListView{Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:4
                                    model:documentController.maskList
                                    delegate:Rectangle{
                                        width:ListView.view.width;height:48;radius:6
                                        color: modelData.id===documentController.activeAdjustmentTarget ? "#1c2258" : "#171c2a"
                                        border.color: modelData.id===documentController.activeAdjustmentTarget ? "#6366f1" : "transparent"
                                        border.width: 1
                                        // Clicking anywhere on the row (except the delete button,
                                        // which sits on top and consumes its own click) switches
                                        // editing focus to this mask -- a faster, more direct
                                        // complement to the Editing: combo above, matching how the
                                        // Layers panel already lets you click a row to select it.
                                        MouseArea {
                                            anchors.fill: parent
                                            z: -1
                                            onClicked: documentController.activeAdjustmentTarget = modelData.id
                                        }
                                        RowLayout{anchors.fill:parent;anchors.margins:7;spacing:9
                                            Image{Layout.preferredWidth:52;Layout.preferredHeight:34;fillMode:Image.PreserveAspectFit
                                                source:modelData.url||""}
                                            ColumnLayout{spacing:2;Layout.fillWidth:true
                                                Label{text:modelData.name||"Mask";color:"#c8d0e0";font.pixelSize:12}
                                                // Which layer this mask belongs to (see Mask::targetLayerId) --
                                                // without this there's no way to tell layer-scoped masks apart
                                                // from base-image ones just by looking at the list.
                                                Label{text:modelData.ownerLayerName||"Base Image";color:"#6366f1";font.pixelSize:9}
                                            }
                                            Button{
                                                implicitWidth:22;implicitHeight:22;flat:true
                                                ToolTip.visible:hovered;ToolTip.delay:500;ToolTip.text:"Delete this mask"
                                                onClicked: documentController.deleteMask(modelData.id)
                                                contentItem:Label{text:"\u2715";color:"#8892a4";font.pixelSize:10;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
                                                background:Rectangle{color:parent.hovered?"#2a1414":"transparent";radius:4}
                                            }
                                        }
                                    }
                                    Label{anchors.centerIn:parent;visible:documentController.maskList.length===0
                                        text:"No masks yet \u2014 click + Add Mask above";color:"#3a4566";font.pixelSize:11;wrapMode:Text.WordWrap;width:parent.width-20;horizontalAlignment:Text.AlignHCenter}
                                }
                            }
                        }

                        // FILMSTRIP
                        Item {
                            ListView{
                                anchors.fill:parent;orientation:ListView.Horizontal;clip:true;spacing:4
                                anchors.margins:6
                                model:documentController.recentFiles
                                delegate:Rectangle{
                                    width:80;height:ListView.view.height;color:"#171c2a";radius:4
                                    // Issue fix: Qt.fileInfo() does not exist in Qt 6 QML.
                                    // Use the baseFileName() helper defined at the top of ApplicationWindow.
                                    border.color: documentController.sourceName === root.baseFileName(modelData)
                                                  ? "#6366f1" : "transparent"
                                    border.width:2
                                    Column{anchors.fill:parent;anchors.margins:4;spacing:3
                                        Image{width:parent.width;height:parent.width*0.667;fillMode:Image.PreserveAspectCrop
                                            source:"file:///"+modelData;asynchronous:true}
                                        Label{
                                            text: root.baseFileName(modelData)
                                            color:"#8892a4";font.pixelSize:9
                                            width:parent.width;elide:Text.ElideRight;horizontalAlignment:Text.AlignHCenter}
                                    }
                                    MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor
                                        onClicked:documentController.openImage(Qt.resolvedUrl("file:///"+modelData))}
                                }
                                Label{anchors.centerIn:parent;visible:documentController.recentFiles.length===0
                                    text:"Recently opened images appear here";color:"#3a4566";font.pixelSize:11}
                            }
                        }

                    } // StackLayout
                } // ColumnLayout (bottom)
            } // Rectangle (bottom panel)
        } // SplitView
    } // ColumnLayout (root)

    // Edge/corner resize handles (Part 4). A frameless window loses the OS's
    // own resize grips along with its title bar, so these restore that
    // capability -- thin, mostly-invisible hit areas along the four edges
    // and corners, each calling Qt's own startSystemResize() (the same
    // recommended, cross-platform-safe mechanism used for the drag-to-move
    // handling above), so no platform-specific resize logic is needed.
    // z:1000+ keeps them hit-testable above ordinary content near the edges.
    MouseArea { height:4; z:1000; cursorShape:Qt.SizeVerCursor
        anchors { top:parent.top; left:parent.left; right:parent.right }
        onPressed: root.startSystemResize(Qt.TopEdge) }
    MouseArea { height:4; z:1000; cursorShape:Qt.SizeVerCursor
        anchors { bottom:parent.bottom; left:parent.left; right:parent.right }
        onPressed: root.startSystemResize(Qt.BottomEdge) }
    MouseArea { width:4; z:1000; cursorShape:Qt.SizeHorCursor
        anchors { left:parent.left; top:parent.top; bottom:parent.bottom }
        onPressed: root.startSystemResize(Qt.LeftEdge) }
    MouseArea { width:4; z:1000; cursorShape:Qt.SizeHorCursor
        anchors { right:parent.right; top:parent.top; bottom:parent.bottom }
        onPressed: root.startSystemResize(Qt.RightEdge) }
    MouseArea { width:8; height:8; z:1001; cursorShape:Qt.SizeFDiagCursor
        anchors { top:parent.top; left:parent.left }
        onPressed: root.startSystemResize(Qt.TopEdge | Qt.LeftEdge) }
    MouseArea { width:8; height:8; z:1001; cursorShape:Qt.SizeFDiagCursor
        anchors { bottom:parent.bottom; right:parent.right }
        onPressed: root.startSystemResize(Qt.BottomEdge | Qt.RightEdge) }
    MouseArea { width:8; height:8; z:1001; cursorShape:Qt.SizeBDiagCursor
        anchors { top:parent.top; right:parent.right }
        onPressed: root.startSystemResize(Qt.TopEdge | Qt.RightEdge) }
    MouseArea { width:8; height:8; z:1001; cursorShape:Qt.SizeBDiagCursor
        anchors { bottom:parent.bottom; left:parent.left }
        onPressed: root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge) }

    // ---------------------------------------------------------------------------
    // ZoomControl inline component
    // Replaces the previous Fit/100%/-/+ button row plus a separate read-only
    // percentage readout with one compact control, closer to how Photoshop/
    // Lightroom/Affinity present zoom: a single field that always shows the
    // CURRENT zoom (updating live as the user zooms via wheel/pinch, since
    // it's a plain binding to root.zoom), click-to-type a custom value
    // (reusing the same click-to-edit pattern AdjustmentSlider already uses
    // elsewhere in this file), plus a small dropdown for the four presets
    // requested (Fit / 25% / 50% / 100%). Mouse-wheel zoom is untouched --
    // it already just sets root.zoom directly, which this control displays.
    // ---------------------------------------------------------------------------
    component ZoomControl: Rectangle {
        id: zoomRoot
        implicitWidth: 92; implicitHeight: 26
        radius: 5; color: "#0f1219"; border.color: "#1e2438"
        enabled: documentController.hasDocument
        opacity: enabled ? 1.0 : 0.5

        function commitPercent(pct) {
            const clamped = Math.max(10, Math.min(400, pct));
            root.zoom = clamped / 100;
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            spacing: 0

            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                Label {
                    id: zoomLabel
                    anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                    text: Math.round(root.zoom * 100) + "%"
                    color: "#8892a4"; font.pixelSize: 11
                    visible: !zoomInput.visible
                }
                TextInput {
                    id: zoomInput
                    anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                    width: 34; visible: false
                    color: "#c7d2fe"; font.pixelSize: 11
                    selectByMouse: true
                    validator: IntValidator{ bottom: 10; top: 400 }
                    onEditingFinished: {
                        const v = parseInt(text);
                        if (!isNaN(v)) zoomRoot.commitPercent(v);
                        visible = false;
                    }
                    Keys.onEscapePressed: visible = false
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.IBeamCursor
                    onClicked: {
                        zoomInput.text = Math.round(root.zoom * 100);
                        zoomInput.visible = true;
                        zoomInput.forceActiveFocus();
                        zoomInput.selectAll();
                    }
                }
            }

            Button {
                Layout.preferredWidth: 20; Layout.fillHeight: true
                flat: true
                contentItem: Label {
                    text: "\u25be"; color: "#6b7a99"; font.pixelSize: 9
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle { color: parent.hovered ? "#1e2438" : "transparent"; radius: 4 }
                onClicked: zoomPresetMenu.open()
            }
        }

        Menu {
            id: zoomPresetMenu
            y: parent.height + 2
            background: Rectangle { implicitWidth: 92; color: "#171c2a"; border.color: "#252d45"; radius: 6 }
            MenuItem { text: "Fit";  onTriggered: root.zoom = root.fitZoom()
                contentItem: Label{text:"Fit";color:"#c8d0e0";font.pixelSize:11;leftPadding:10} }
            MenuItem { text: "25%";  onTriggered: root.zoom = 0.25
                contentItem: Label{text:"25%";color:"#c8d0e0";font.pixelSize:11;leftPadding:10} }
            MenuItem { text: "50%";  onTriggered: root.zoom = 0.50
                contentItem: Label{text:"50%";color:"#c8d0e0";font.pixelSize:11;leftPadding:10} }
            MenuItem { text: "100%"; onTriggered: root.zoom = 1.0
                contentItem: Label{text:"100%";color:"#c8d0e0";font.pixelSize:11;leftPadding:10} }
        }
    }

    // ---------------------------------------------------------------------------
    // AdjustmentSlider inline component
    // Value label is click-to-edit: single click activates a TextInput for
    // direct numeric entry. Press Enter or click away to commit, Escape to cancel.
    // ---------------------------------------------------------------------------
    component AdjustmentSlider: ColumnLayout {
        id: sliderRoot
        property string label: ""
        property real   from:  0
        property real   to:    1
        property real   value: 0
        signal moved(real nextValue)
        Layout.leftMargin:12; Layout.rightMargin:12; Layout.fillWidth:true; spacing:2
        RowLayout { Layout.fillWidth:true
            Label{text:sliderRoot.label;color:"#8892a4";font.pixelSize:11;Layout.fillWidth:true}
            Item {
                implicitWidth:52; implicitHeight:16
                Label {
                    id:valLabel; anchors.fill:parent
                    text: Number(sl.value).toFixed(sliderRoot.to<=3?2:0)
                    color: sl.value!==0?"#6366f1":"#4a5268"; font.pixelSize:11
                    horizontalAlignment:Text.AlignRight; visible:!valInput.visible
                    MouseArea { anchors.fill:parent; cursorShape:Qt.IBeamCursor
                        onClicked: { valInput.text=valLabel.text; valInput.visible=true; valInput.forceActiveFocus(); valInput.selectAll() }
                    }
                }
                TextInput {
                    id:valInput; anchors.fill:parent; visible:false
                    color:"#c7d2fe"; font.pixelSize:11; horizontalAlignment:Text.AlignRight
                    selectByMouse:true
                    validator:DoubleValidator{bottom:sliderRoot.from;top:sliderRoot.to;decimals:2;notation:DoubleValidator.StandardNotation}
                    onEditingFinished:{
                        const v=parseFloat(text.replace(",","."));
                        if (!isNaN(v)) sliderRoot.moved(Math.max(sliderRoot.from,Math.min(sliderRoot.to,v)));
                        visible=false;
                    }
                    Keys.onEscapePressed: visible=false
                }
            }
        }
        Slider { id:sl; Layout.fillWidth:true; implicitHeight:18
            from:sliderRoot.from; to:sliderRoot.to; value:sliderRoot.value
            enabled:documentController.hasDocument
            // Brackets the whole drag into one undo step with the FINAL
            // value's descriptive label (e.g. "Exposure: 1.50"), instead
            // of one undo step (and one History-panel entry) per tick --
            // see DocumentController::beginAdjustmentEdit()/
            // commitAdjustmentEdit() and DocumentModel's per-tick label
            // refresh. pressed is a plain bool property, so onPressedChanged
            // is the standard QtQuick Controls idiom for "drag started"
            // (true) / "drag ended" (false) -- mirrors exactly how
            // LayerTransformOverlay.qml already brackets move/resize/rotate
            // gestures with begin/commitLayerTransformEdit().
            onPressedChanged: {
                if (pressed) documentController.beginAdjustmentEdit();
                else documentController.commitAdjustmentEdit();
            }
            onMoved:sliderRoot.moved(value)
            background:Rectangle{x:sl.leftPadding;y:sl.topPadding+sl.availableHeight/2-height/2
                width:sl.availableWidth;height:3;radius:1.5;color:"#1a1e2e"
                Rectangle{width:sl.visualPosition*parent.width;height:parent.height;radius:1.5;color:sl.value!==0?"#6366f1":"#252d45"}}
            handle:Rectangle{
                x:sl.leftPadding+sl.visualPosition*(sl.availableWidth-width)
                y:sl.topPadding+sl.availableHeight/2-height/2
                width:13;height:13;radius:6.5
                color:sl.pressed?"#818cf8":sl.hovered?"#818cf8":"#6366f1"
                Behavior on color{ColorAnimation{duration:100}}
            }
        }
    }

} // ApplicationWindow
```

---

# File: `app\resources\qml\LayerTransformOverlay.qml`
```qml
import QtQuick

// Layer Transform overlay.
//
// STAGE 1 (done): click-to-select. Clicking an overlay layer's bounding box
// on the canvas sets documentController.selectedLayerId; clicking empty
// space clears it. Selected/unselected layers get a thin border so their
// clickable regions are visible while the Transform tool is active.
//
// STAGE 2 (done): drag-to-move.
//
// ── Root-cause fix for the "drag lags / resets every tick" bug ───────────
// The first version of this file put a MouseArea INSIDE each Repeater
// delegate (one per layer) and tracked the drag gesture as properties on
// THAT MouseArea. That was wrong: every setLayerTransform() call emits
// changed() -> layersChanged() -> documentController.layerModel is re-read
// -> a BRAND NEW QVariantList comes back every time -> this file's own
// overlayModel builds another brand-new JS array -> QML's Repeater (bound
// to a plain JS array, no identity-preserving diff) destroys and recreates
// ALL delegates on every tick, including whatever MouseArea/state lived
// inside them. The drag would move a tiny amount, then silently lose its
// grab and reset.
//
// The fix, still in effect for stage 3: the ENTIRE interactive gesture
// (hit-testing, press, drag-tracking, resize-tracking) lives in a single
// MouseArea covering the whole overlay (`interactionArea` below), which is
// NOT part of any Repeater and is therefore never destroyed by a model
// rebuild. Everything else (box borders, resize-handle squares) is purely
// presentational, safe to rebuild every tick, since none of it holds state
// that needs to survive across ticks.
//
// STAGE 3 (this file): resize handles for the selected layer.
// 8 handles (4 corners + 4 edges), matching CropOverlay.qml's handle
// layout. Corner handles resize both axes; aspect ratio is LOCKED by
// default (uniform scale, derived by projecting the mouse onto the box's
// diagonal from the fixed/opposite corner -- the standard technique for
// "closest uniform scale to an arbitrary drag direction"), with Shift held
// as the free-transform escape hatch (independent X/Y resize). Edge
// handles always resize a single axis only, same as CropOverlay.
//
// All resize math is done in the box's own LOCAL (unrotated) coordinate
// frame -- the mouse position is un-rotated into that frame first (same
// inverse-rotation used by hitTest()), the new box bounds are computed
// there, and only the resulting CENTER OFFSET is rotated back out to
// world/canvas space at the end. This was written rotation-safe before
// Stage 4 (rotation) existed, specifically so that once rotation could be
// set, this file's resize math would not need to be revisited -- and
// indeed it wasn't; see Stage 4 below.
//
// Resize (like move) is bracketed with begin/commitLayerTransformEdit(),
// so a whole resize drag is one undo step, reusing the exact same
// transaction machinery as move -- no separate mechanism.
//
// STAGE 4 (this file): rotation handle.
// A small circle sits above the box's top edge, connected by a thin
// stalk line, for the SELECTED layer only -- purely presentational,
// drawn inside the existing `handleAnchor` Item (which already rotates
// with the layer, so no manual sin/cos is needed for the drawing itself
// -- only interactionArea's manual JS hit-testing needs to replicate the
// rotation, same as it already does for the 8 resize handles).
//
// Dragging the handle computes an angle from the box CENTER to the
// cursor and writes it straight to Layer::rotation via
// setLayerTransform() -- position and scale are read fresh from the
// model each tick and passed through unchanged, the same defensive
// "don't trust a stale cached copy" approach the move handler already
// uses for scale/rotation.
//
// A fixed angle OFFSET (mouse-angle-at-press minus the layer's rotation
// at press) is captured once in onPressed and held for the whole
// gesture, so the box rotates smoothly relative to wherever the user
// actually grabbed the handle rather than snapping instantly to point
// straight at the cursor.
//
// Holding Shift snaps to 15-degree increments. Unlike resize's
// freeTransform flag (captured once at press and held for the whole
// gesture), Shift here is sampled live on every onPositionChanged tick
// -- a deliberate difference, not an inconsistency: rotate benefits from
// being toggled mid-drag (rotate freely, then hold Shift near the end to
// land exactly on a common angle), matching the rotate-snap behavior in
// tools like Figma/Illustrator. Resize's aspect lock doesn't have the
// same "fine-tune at the end" use case, which is why that one stayed
// press-time-only.
//
// Reuses begin/commitLayerTransformEdit() for undo -- no new undo
// mechanism, per the same rule Stage 3 already followed.
Item {
    id: root
    property var docCtrl: null

    // Defensive cleanup: Main.qml hides this whole overlay (visible: false)
    // whenever the Transform tool stops being active -- including via a
    // keyboard shortcut (Escape, or the B/E/G/R/C/T tool-switch keys in
    // Main.qml) fired while the mouse button is still physically held down
    // over a resize/rotate/move gesture in progress. Whether Qt Quick
    // itself always releases interactionArea's active mouse grab purely
    // from this visibility change isn't something this file wants to
    // depend on either way -- explicitly finalizing here guarantees
    // begin/commitLayerTransformEdit() always pairs up. Without this, an
    // unclosed beginLayerTransformEdit() would leave DocumentModel's
    // transaction permanently open (see beginHistoryTransaction()'s
    // "nested begin -- first begin wins" guard), silently swallowing
    // every undo step for the rest of the session -- so this cleanup
    // costs nothing when the grab was already released normally, and
    // guards against a severe failure mode when it wasn't.
    onVisibleChanged: {
        if (!visible && interactionArea.dragLayerId.length > 0) {
            interactionArea.ungrabMouse();
            if (root.docCtrl) root.docCtrl.commitLayerTransformEdit();
            interactionArea.dragLayerId  = "";
            interactionArea.mode         = "";
            interactionArea.activeHandle = "";
            interactionArea.dragging     = false;
        }
    }

    // Canvas-pixels-per-source-pixel -- this overlay is sized to match
    // imagePreview (sourceWidth/Height * zoom), so this recovers the same
    // factor RenderPipeline calls `previewScale`.
    readonly property real canvasScale: (docCtrl && docCtrl.sourceWidth > 0)
        ? width / docCtrl.sourceWidth : 1.0

    // documentController.layerModel is topmost-first; reverse + drop the
    // base layer so index 0 = lowest overlay order, last = topmost. Used
    // both for Repeater paint order and for hit-test priority below
    // (checked topmost-first).
    readonly property var overlayModel: {
        const list = docCtrl ? docCtrl.layerModel : [];
        const result = [];
        for (let i = list.length - 1; i >= 0; --i)
            if (!list[i].isBase) result.push(list[i]);
        return result;
    }

    // Full current data for whichever layer is selected, or null. Re-reads
    // overlayModel fresh each time (including every tick during a drag or
    // resize), so both the visual handles/box and any in-progress gesture
    // math always see the layer's latest posX/posY/scaleX/scaleY.
    readonly property var selectedLayerData: {
        if (!docCtrl) return null;
        const id = docCtrl.selectedLayerId;
        if (!id) return null;
        for (const l of overlayModel) if (l.realId === id) return l;
        return null;
    }

    // Stage 4: rotation handle geometry, in canvas/screen pixels (same
    // fixed-pixel-regardless-of-zoom convention as the 10x10 resize
    // handle squares and their HIT_R=9 hit radius below -- NOT multiplied
    // by canvasScale again, since canvasScale is already folded into
    // every box/handle position these are offset from).
    readonly property real rotateHandleDistance: 28
    readonly property real rotateHandleHitRadius: 10

    // Single, STABLE MouseArea for the whole overlay -- see file header.
    // Holds ALL interaction state (move drag AND resize drag) so nothing
    // is ever lost to a mid-gesture model rebuild.
    MouseArea {
        id: interactionArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: {
            if (mode === "resize") return cursorForHandle(activeHandle);
            if (mode === "move")   return Qt.SizeAllCursor;
            if (mode === "rotate") return Qt.ClosedHandCursor;
            // Not currently dragging: preview a resize/rotate cursor on
            // hover over a handle, otherwise default arrow.
            const hovered = root.selectedLayerData ? hitTestHandle(mouseX, mouseY) : "";
            return hovered.length > 0 ? cursorForHandle(hovered) : Qt.ArrowCursor;
        }

        property string dragLayerId:  ""
        property string mode:         ""      // "" | "move" | "resize" | "rotate"
        property string activeHandle: ""       // "" | nw/n/ne/e/se/s/sw/w/rotate
        property real   pressX:       0
        property real   pressY:       0
        property real   startPosX:    0
        property real   startPosY:    0
        property real   startScaleX:  1
        property real   startScaleY:  1
        property real   startRotation:0
        property real   startImgW:    0
        property real   startImgH:    0
        property bool   freeTransform:false    // Shift held at press time (corner handles only)
        property bool   dragging:     false
        property int    profileEvents: 0
        property double profileStartMs: 0
        property real   rotateAngleOffset: 0   // Stage 4: angle(center->pressMouse) - startRotation, held for the whole rotate gesture

        function cursorForHandle(h) {
            switch (h) {
            case "nw": case "se": return Qt.SizeFDiagCursor;
            case "ne": case "sw": return Qt.SizeBDiagCursor;
            case "n":  case "s":  return Qt.SizeVerCursor;
            case "e":  case "w":  return Qt.SizeHorCursor;
            // Stage 4: no native "rotate" cursor shape exists in Qt --
            // OpenHand (hover) / ClosedHand (actively dragging, see
            // cursorShape below) is the closest available "grab and
            // turn" metaphor.
            case "rotate":         return Qt.OpenHandCursor;
            default:               return Qt.ArrowCursor;
            }
        }

        // Topmost-first hit test against each overlay layer's (possibly
        // rotated) bounding box, in this Item's own (== root's) local
        // coordinates.
        function hitTest(px, py) {
            const list = root.overlayModel;
            for (let i = list.length - 1; i >= 0; --i) {
                const l = list[i];
                const w  = Math.max(1, l.imgWidth  * l.scaleX * root.canvasScale);
                const h  = Math.max(1, l.imgHeight * l.scaleY * root.canvasScale);
                const cx = root.width  * 0.5 + l.posX * root.canvasScale;
                const cy = root.height * 0.5 + l.posY * root.canvasScale;
                const dx = px - cx, dy = py - cy;
                // Un-rotate the click point by -rotation around the box's
                // center, then test against the axis-aligned box.
                const rad  = -l.rotation * Math.PI / 180;
                const cosR = Math.cos(rad), sinR = Math.sin(rad);
                const localX = dx * cosR - dy * sinR;
                const localY = dx * sinR + dy * cosR;
                if (Math.abs(localX) <= w / 2 && Math.abs(localY) <= h / 2)
                    return l;
            }
            return null;
        }

        // Returns which of the 8 handles (if any) of the CURRENTLY
        // SELECTED layer is under (px, py), using a small circular hit
        // radius around each handle's world position (rotation-aware: the
        // handle's local offset is rotated by the layer's own rotation
        // before comparing against the click point).
        function hitTestHandle(px, py) {
            const l = root.selectedLayerData;
            if (!l) return "";
            const hw = Math.max(1, l.imgWidth  * l.scaleX * root.canvasScale) / 2;
            const hh = Math.max(1, l.imgHeight * l.scaleY * root.canvasScale) / 2;
            const cx = root.width  * 0.5 + l.posX * root.canvasScale;
            const cy = root.height * 0.5 + l.posY * root.canvasScale;
            const rad = l.rotation * Math.PI / 180;
            const cosR = Math.cos(rad), sinR = Math.sin(rad);
            const handles = [
                ["nw", -hw, -hh], ["n", 0, -hh], ["ne", hw, -hh], ["e", hw, 0],
                ["se", hw, hh],   ["s", 0, hh],   ["sw", -hw, hh], ["w", -hw, 0],
                // Stage 4: sits beyond the N handle along the same local
                // up-axis, rotated out to world space by the same
                // cosR/sinR as every other handle above.
                ["rotate", 0, -hh - root.rotateHandleDistance]
            ];
            const HIT_R = 9;
            for (const [name, lx, ly] of handles) {
                const wx = cx + (lx * cosR - ly * sinR);
                const wy = cy + (lx * sinR + ly * cosR);
                const hitR = name === "rotate" ? root.rotateHandleHitRadius : HIT_R;
                if (Math.hypot(px - wx, py - wy) <= hitR) return name;
            }
            return "";
        }

        onPressed: (mouse) => {
            const handle = root.selectedLayerData ? hitTestHandle(mouse.x, mouse.y) : "";
            if (handle === "rotate") {
                const l = root.selectedLayerData;
                mode          = "rotate";
                activeHandle  = "rotate";
                dragLayerId   = l.realId;
                startPosX     = l.posX;    startPosY     = l.posY;
                startScaleX   = l.scaleX;  startScaleY   = l.scaleY;
                startRotation = l.rotation;
                startImgW     = l.imgWidth; startImgH    = l.imgHeight;
                const cx = root.width  * 0.5 + startPosX * root.canvasScale;
                const cy = root.height * 0.5 + startPosY * root.canvasScale;
                // Fixed offset held for the whole gesture -- see file
                // header's Stage 4 note on why this isn't recomputed
                // every tick.
                rotateAngleOffset = Math.atan2(mouse.y - cy, mouse.x - cx) * 180 / Math.PI - startRotation;
                pressX = mouse.x; pressY = mouse.y;
                dragging = false;
                // Brackets the whole rotate gesture into one undo step --
                // same mechanism as move/resize, see onReleased.
                if (root.docCtrl) root.docCtrl.beginLayerTransformEdit();
                return;
            }

            if (handle.length > 0) {
                const l = root.selectedLayerData;
                mode          = "resize";
                activeHandle  = handle;
                dragLayerId   = l.realId;
                startPosX     = l.posX;    startPosY     = l.posY;
                startScaleX   = l.scaleX;  startScaleY   = l.scaleY;
                startRotation = l.rotation;
                startImgW     = l.imgWidth; startImgH    = l.imgHeight;
                freeTransform = (mouse.modifiers & Qt.ShiftModifier) !== 0;
                pressX = mouse.x; pressY = mouse.y;
                dragging = false;
                // Brackets the whole resize gesture into one undo step --
                // same mechanism as move, see onReleased.
                if (root.docCtrl) root.docCtrl.beginLayerTransformEdit();
                return;
            }

            const hit = hitTest(mouse.x, mouse.y);
            mode = hit ? "move" : "";
            if (root.docCtrl) root.docCtrl.selectedLayerId = hit ? hit.realId : "";
            dragLayerId = hit ? hit.realId : "";
            pressX = mouse.x; pressY = mouse.y;
            if (hit) {
                startPosX = hit.posX; startPosY = hit.posY;
                // Brackets the whole gesture (however many setLayerTransform()
                // ticks it produces) into a single undo step, mirroring how
                // Main.qml's AdjustmentSlider calls beginAdjustmentEdit()/
                // commitAdjustmentEdit() on press/release. Safe even if the
                // press turns out to be a plain click with no drag --
                // DocumentModel's transactionChangedAnything() (now
                // layer-aware) detects nothing changed and skips the undo
                // step, same as it already does for a no-op slider press.
                if (root.docCtrl) root.docCtrl.beginLayerTransformEdit();
            }
            dragging = false;
        }

        onPositionChanged: (mouse) => {
            if (!pressed || dragLayerId.length === 0) return;
            if (profileStartMs === 0) profileStartMs = Date.now();
            profileEvents++;
            if (profileEvents % 60 === 0)
                console.log("PROFILE transform QML events=", profileEvents,
                            "elapsedMs=", Date.now() - profileStartMs);
            if (mode === "resize") { doResize(mouse.x, mouse.y); return; }
            if (mode === "rotate") { doRotate(mouse.x, mouse.y, mouse.modifiers); return; }

            const dx = mouse.x - pressX;
            const dy = mouse.y - pressY;
            // Dead-zone: a plain click (no real movement) must not emit a
            // spurious near-zero setLayerTransform call.
            if (!dragging && Math.abs(dx) < 2 && Math.abs(dy) < 2) return;
            dragging = true;
            // Re-read current scale/rotation each tick rather than caching
            // a copy from press-time, in case they change mid-drag from
            // elsewhere.
            let cur = null;
            for (const l of root.overlayModel) { if (l.realId === dragLayerId) { cur = l; break; } }
            if (!cur || !root.docCtrl) return;
            const newPosX = startPosX + dx / root.canvasScale;
            const newPosY = startPosY + dy / root.canvasScale;
            root.docCtrl.setLayerTransform(dragLayerId, newPosX, newPosY,
                                            cur.scaleX, cur.scaleY, cur.rotation);
        }

        // Minimum box size, in canvas pixels, that a resize is allowed to
        // shrink to -- prevents a drag past the opposite corner/edge from
        // producing a degenerate zero/negative/inverted layer.
        readonly property real minSizePx: 12

        function doResize(mouseX, mouseY) {
            if (!root.docCtrl) return;
            // Dead-zone, same 2px threshold and reasoning as move/doRotate:
            // a plain click on a resize handle (no real movement) must not
            // emit a spurious near-zero resize. This was previously
            // missing here -- doResize was the only one of the three
            // gesture handlers (move, doResize, doRotate) without it,
            // meaning a click-and-release on a resize handle with even
            // sub-pixel mouse jitter between press and release could
            // register a spurious "Transform layer" undo step where the
            // equivalent click on the move hit-area or rotate handle
            // correctly no-ops.
            const ddx0 = mouseX - pressX, ddy0 = mouseY - pressY;
            if (!dragging && Math.abs(ddx0) < 2 && Math.abs(ddy0) < 2) return;
            dragging = true;

            const startW = Math.max(1, startImgW * startScaleX * root.canvasScale);
            const startH = Math.max(1, startImgH * startScaleY * root.canvasScale);
            const startHw = startW / 2, startHh = startH / 2;
            const startCx = root.width  * 0.5 + startPosX * root.canvasScale;
            const startCy = root.height * 0.5 + startPosY * root.canvasScale;

            // Un-rotate the current mouse position into the box's own
            // local (unrotated) frame, centered on the box's PRESS-TIME
            // center -- same technique as hitTest()/hitTestHandle().
            const rad  = -startRotation * Math.PI / 180;
            const cosR = Math.cos(rad), sinR = Math.sin(rad);
            const ddx = mouseX - startCx, ddy = mouseY - startCy;
            const localMouseX = ddx * cosR - ddy * sinR;
            const localMouseY = ddx * sinR + ddy * cosR;

            // Local-frame box bounds, relative to the PRESS-TIME center.
            // Start from the unchanged box, then let the active handle
            // override whichever edges it controls.
            let left = -startHw, right = startHw, top = -startHh, bottom = startHh;

            const isCorner = (activeHandle === "nw" || activeHandle === "ne" ||
                               activeHandle === "se" || activeHandle === "sw");

            if (isCorner && !freeTransform) {
                // Aspect-locked corner resize: project the mouse (in local
                // frame) onto the diagonal from the FIXED (opposite)
                // corner to the corner being dragged, and use that
                // projection as a single uniform scale factor `t`. This is
                // the standard "closest uniform scale to an arbitrary drag
                // direction" technique -- dragging exactly along the
                // diagonal gives an exact scale; dragging off-diagonal
                // still gives a sensible, stable uniform scale rather than
                // fighting between two different per-axis factors.
                const fixedX = (activeHandle === "ne" || activeHandle === "se") ? -startHw : startHw;
                const fixedY = (activeHandle === "se" || activeHandle === "sw") ? -startHh : startHh;
                const draggedX = -fixedX, draggedY = -fixedY;
                const diagX = draggedX - fixedX, diagY = draggedY - fixedY;
                const vX = localMouseX - fixedX, vY = localMouseY - fixedY;
                const diagLenSq = diagX * diagX + diagY * diagY;
                let t = diagLenSq > 0 ? (vX * diagX + vY * diagY) / diagLenSq : 1.0;
                t = Math.max(0.05, t); // never let the uniform scale collapse to ~0 or invert
                const newDraggedX = fixedX + t * diagX;
                const newDraggedY = fixedY + t * diagY;
                left   = Math.min(fixedX, newDraggedX); right  = Math.max(fixedX, newDraggedX);
                top    = Math.min(fixedY, newDraggedY); bottom = Math.max(fixedY, newDraggedY);
            } else {
                // Free (non-uniform) resize: each edge the active handle
                // touches moves independently to the mouse; edges it
                // doesn't touch stay at their press-time position.
                if (activeHandle.indexOf("w") !== -1) left   = localMouseX;
                if (activeHandle.indexOf("e") !== -1) right  = localMouseX;
                if (activeHandle.indexOf("n") !== -1) top    = localMouseY;
                if (activeHandle.indexOf("s") !== -1) bottom = localMouseY;
            }

            let newW = right - left, newH = bottom - top;
            // Clamp to the minimum size, anchored at whichever side is
            // NOT being dragged (so the fixed edge/corner truly stays put
            // even once clamped).
            if (newW < minSizePx) {
                if (activeHandle.indexOf("w") !== -1) left  = right - minSizePx;
                else                                   right = left + minSizePx;
                newW = minSizePx;
            }
            if (newH < minSizePx) {
                if (activeHandle.indexOf("n") !== -1) top    = bottom - minSizePx;
                else                                   bottom = top + minSizePx;
                newH = minSizePx;
            }

            // New local-frame center offset from the press-time center,
            // rotated back out to world/canvas space.
            const localOffsetX = (left + right) / 2;
            const localOffsetY = (top + bottom) / 2;
            const fRad = startRotation * Math.PI / 180;
            const fCos = Math.cos(fRad), fSin = Math.sin(fRad);
            const worldOffsetX = localOffsetX * fCos - localOffsetY * fSin;
            const worldOffsetY = localOffsetX * fSin + localOffsetY * fCos;

            const newCx = startCx + worldOffsetX;
            const newCy = startCy + worldOffsetY;
            const newPosX = (newCx - root.width  * 0.5) / root.canvasScale;
            const newPosY = (newCy - root.height * 0.5) / root.canvasScale;
            const newScaleX = newW / (startImgW * root.canvasScale);
            const newScaleY = newH / (startImgH * root.canvasScale);

            root.docCtrl.setLayerTransform(dragLayerId, newPosX, newPosY,
                                            newScaleX, newScaleY, startRotation);
        }

        // Stage 4: rotation. Position and scale are re-read fresh from
        // the live model each tick and passed through UNCHANGED -- same
        // "don't trust a stale press-time copy for whatever this gesture
        // isn't changing" approach the move handler already uses for
        // scale/rotation (see its "cur" lookup above). Only rotation
        // itself is computed, from the fixed angle offset captured in
        // onPressed.
        function doRotate(mouseX, mouseY, modifiers) {
            if (!root.docCtrl) return;

            const dx = mouseX - pressX, dy = mouseY - pressY;
            // Dead-zone, same 2px threshold and reasoning as the move
            // handler: a plain click (no real movement) must not emit a
            // spurious near-zero rotation change.
            if (!dragging && Math.abs(dx) < 2 && Math.abs(dy) < 2) return;
            dragging = true;

            let cur = null;
            for (const l of root.overlayModel) { if (l.realId === dragLayerId) { cur = l; break; } }
            if (!cur) return;

            const cx = root.width  * 0.5 + cur.posX * root.canvasScale;
            const cy = root.height * 0.5 + cur.posY * root.canvasScale;
            const angleDeg = Math.atan2(mouseY - cy, mouseX - cx) * 180 / Math.PI;
            let newRotation = angleDeg - rotateAngleOffset;
            // Normalize into [0, 360) -- keeps the value bounded across a
            // long multi-turn drag and makes the snap rounding below
            // behave predictably.
            newRotation = ((newRotation % 360) + 360) % 360;

            // Live modifier check (sampled every tick) -- see file
            // header's Stage 4 note on why this differs from resize's
            // press-time-only freeTransform.
            if (modifiers & Qt.ShiftModifier) {
                const SNAP_DEG = 15;
                newRotation = Math.round(newRotation / SNAP_DEG) * SNAP_DEG;
                if (newRotation >= 360) newRotation -= 360;
            }

            root.docCtrl.setLayerTransform(dragLayerId, cur.posX, cur.posY,
                                            cur.scaleX, cur.scaleY, newRotation);
        }

        onReleased: {
            // Safe to call unconditionally even if beginLayerTransformEdit()
            // was never called this press (e.g. clicked empty space) --
            // DocumentController guards on its own m_layerTransformEditOpen
            // flag and no-ops.
            if (root.docCtrl) root.docCtrl.commitLayerTransformEdit();
            dragLayerId  = "";
            mode         = "";
            activeHandle = "";
            dragging     = false;
        }
    }

    Repeater {
        model: root.overlayModel
        delegate: Item {
            id: handle
            readonly property bool isSelected: root.docCtrl && root.docCtrl.selectedLayerId === modelData.realId

            width:  Math.max(1, modelData.imgWidth  * modelData.scaleX * root.canvasScale)
            height: Math.max(1, modelData.imgHeight * modelData.scaleY * root.canvasScale)
            x: (root.width  * 0.5 + modelData.posX * root.canvasScale) - width  * 0.5
            y: (root.height * 0.5 + modelData.posY * root.canvasScale) - height * 0.5
            rotation: modelData.rotation
            transformOrigin: Item.Center

            // Purely presentational: no MouseArea, no state of its own. It
            // is fine for this Item to be destroyed and recreated on every
            // model rebuild (i.e. every drag tick) -- unlike the previous
            // design, nothing here needs to survive across ticks.
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.width: handle.isSelected ? 2 : 1
                border.color: handle.isSelected ? "#6366f1" : "#ffffff33"
            }
        }
    }

    // Resize-handle squares for the SELECTED layer only. Purely
    // presentational (no MouseArea/state of their own -- all interaction
    // lives in interactionArea above), so it's fine for these to rebuild
    // every tick. The inner Repeater's model is a fixed 8-element JS array
    // literal (not derived from docCtrl.layerModel), so it never changes
    // identity and Qt Quick never destroys/recreates these delegates on
    // its own account either way -- doubly safe.
    Item {
        id: handleAnchor
        visible: root.selectedLayerData !== null
        width: 0
        height: 0
        x: root.selectedLayerData ? (root.width  * 0.5 + root.selectedLayerData.posX * root.canvasScale) : 0
        y: root.selectedLayerData ? (root.height * 0.5 + root.selectedLayerData.posY * root.canvasScale) : 0
        rotation: root.selectedLayerData ? root.selectedLayerData.rotation : 0

        readonly property real hw: root.selectedLayerData
            ? Math.max(1, root.selectedLayerData.imgWidth  * root.selectedLayerData.scaleX * root.canvasScale) / 2 : 0
        readonly property real hh: root.selectedLayerData
            ? Math.max(1, root.selectedLayerData.imgHeight * root.selectedLayerData.scaleY * root.canvasScale) / 2 : 0

        Repeater {
            model: [
                { lx: -1, ly: -1 }, { lx: 0, ly: -1 }, { lx: 1, ly: -1 },
                { lx: 1,  ly: 0  },
                { lx: 1,  ly: 1  }, { lx: 0, ly: 1 }, { lx: -1, ly: 1 },
                { lx: -1, ly: 0  }
            ]
            delegate: Rectangle {
                width: 10; height: 10; radius: 2
                color: "#ffffff"
                border.color: "#6366f1"
                border.width: 1.5
                x: handleAnchor.hw * modelData.lx - width  / 2
                y: handleAnchor.hh * modelData.ly - height / 2
            }
        }

        // Stage 4: rotation handle -- a thin stalk plus a circular grip,
        // local offset (0, -hh - rotateHandleDistance), same convention
        // as the 8 squares just above. Purely presentational, no
        // MouseArea/state of its own -- all interaction lives in
        // interactionArea; QML's own `rotation` on handleAnchor rotates
        // these along with everything else, no manual sin/cos needed
        // here (unlike interactionArea's hitTestHandle, which must
        // replicate the rotation manually for its own hit-testing).
        Rectangle {
            width: 1
            height: root.rotateHandleDistance
            color: "#ffffff88"
            x: -0.5
            y: -handleAnchor.hh - root.rotateHandleDistance
        }
        Rectangle {
            width: 14; height: 14; radius: 7
            color: "#ffffff"
            border.color: "#6366f1"
            border.width: 1.5
            x: -width / 2
            y: -handleAnchor.hh - root.rotateHandleDistance - height / 2
        }
    }
}
```

---

# File: `app\resources\qml\CropOverlay.qml`
```qml
import QtQuick
import QtQuick.Controls

// Resizable, draggable crop overlay. Placed over the image preview at the same
// size. Visible only when documentController.activeTool === 5.
//
// Coordinate system: boxX/Y/W/H are in canvas pixels (same space as width/height).
// On confirm, they are scaled to source-image space before calling applyCrop().
Item {
    id: cropOverlay

    property var  docCtrl:  null
    property real minSize:  40       // minimum side length in canvas pixels

    property real boxX: 0
    property real boxY: 0
    property real boxW: width
    property real boxH: height
    property real rotation: 0

    // Reset to 90% of the canvas when the overlay appears
    onVisibleChanged: {
        if (visible) {
            boxX = width  * 0.05;
            boxY = height * 0.05;
            boxW = width  * 0.90;
            boxH = height * 0.90;
        }
    }

    // Called by the Enter shortcut in Main.qml and by the Crop button
    function confirm() {
        if (!docCtrl) return;
        const sw = docCtrl.sourceWidth;
        const sh = docCtrl.sourceHeight;
        docCtrl.applyCrop(
            Math.round(boxX / width  * sw),
            Math.round(boxY / height * sh),
            Math.round(boxW / width  * sw),
            Math.round(boxH / height * sh),
            rotation
        );
    }

    // ── Dark vignette outside the crop box (four rectangles) ─────────────────
    Rectangle {
        x: 0; y: 0
        width: parent.width; height: cropOverlay.boxY
        color: Qt.rgba(0, 0, 0, 0.55)
    }
    Rectangle {
        x: 0
        y: cropOverlay.boxY + cropOverlay.boxH
        width: parent.width
        height: Math.max(0, parent.height - cropOverlay.boxY - cropOverlay.boxH)
        color: Qt.rgba(0, 0, 0, 0.55)
    }
    Rectangle {
        x: 0; y: cropOverlay.boxY
        width: cropOverlay.boxX; height: cropOverlay.boxH
        color: Qt.rgba(0, 0, 0, 0.55)
    }
    Rectangle {
        x: cropOverlay.boxX + cropOverlay.boxW; y: cropOverlay.boxY
        width: Math.max(0, parent.width - cropOverlay.boxX - cropOverlay.boxW)
        height: cropOverlay.boxH
        color: Qt.rgba(0, 0, 0, 0.55)
    }

    // ── Crop-box border ───────────────────────────────────────────────────────
    Rectangle {
        id: cropBorder
        x: cropOverlay.boxX; y: cropOverlay.boxY
        width: cropOverlay.boxW; height: cropOverlay.boxH
        color: "transparent"
        border.color: "white"; border.width: 1
        rotation: cropOverlay.rotation
        transformOrigin: Item.Center

        // Rule-of-thirds grid — native Rectangle lines (GPU-composited), not a
        // Canvas. The previous Canvas-based version did a full CPU
        // ctx.reset() + 4-line redraw on every pixel of a resize-drag (its
        // onWidthChanged/onHeightChanged fired every frame), which is a real,
        // known source of jank for Canvas items during continuous resizing.
        // Plain Rectangles let the scene graph move/resize them with zero
        // CPU rasterization.
        Item {
            anchors.fill: parent
            opacity: 0.35
            // Two vertical thirds lines
            Repeater {
                model: 2
                Rectangle {
                    x: parent.width * (index + 1) / 3 - width / 2
                    y: 0
                    width: 1
                    height: parent.height
                    color: "white"
                }
            }
            // Two horizontal thirds lines
            Repeater {
                model: 2
                Rectangle {
                    x: 0
                    y: parent.height * (index + 1) / 3 - height / 2
                    width: parent.width
                    height: 1
                    color: "white"
                }
            }
        }

        // Interior drag — moves the entire box.
        // margins:12 leaves a dead zone so the handle MouseAreas get priority
        // at corners and edges where they overlap with the border area.
        MouseArea {
            anchors { fill: parent; margins: 12 }
            cursorShape: Qt.SizeAllCursor
            property real sx: 0; property real sy: 0
            property real sbx: 0; property real sby: 0
            onPressed:  (mouse) => {
                sx = mouse.x; sy = mouse.y;
                sbx = cropOverlay.boxX; sby = cropOverlay.boxY;
            }
            onPositionChanged: (mouse) => {
                const dx = mouse.x - sx, dy = mouse.y - sy;
                cropOverlay.boxX = Math.max(0, Math.min(cropOverlay.width  - cropOverlay.boxW, sbx + dx));
                cropOverlay.boxY = Math.max(0, Math.min(cropOverlay.height - cropOverlay.boxH, sby + dy));
            }
        }
    }

    // ── Eight resize handles ──────────────────────────────────────────────────
    // Index:  0=NW  1=N  2=NE  3=E  4=SE  5=S  6=SW  7=W
    Repeater {
        model: 8
        delegate: Rectangle {
            id: handleRect
            readonly property int  idx:      index
            readonly property bool onLeft:   idx === 0 || idx === 6 || idx === 7
            readonly property bool onRight:  idx === 2 || idx === 3 || idx === 4
            readonly property bool onTop:    idx === 0 || idx === 1 || idx === 2
            readonly property bool onBottom: idx === 4 || idx === 5 || idx === 6
            readonly property real localHandleX:
                onLeft ? -cropOverlay.boxW / 2 :
                onRight ? cropOverlay.boxW / 2 : 0
            readonly property real localHandleY:
                onTop ? -cropOverlay.boxH / 2 :
                onBottom ? cropOverlay.boxH / 2 : 0
            readonly property real handleCos: Math.cos(cropOverlay.rotation * Math.PI / 180)
            readonly property real handleSin: Math.sin(cropOverlay.rotation * Math.PI / 180)

            x: cropOverlay.boxX + cropOverlay.boxW / 2
               + localHandleX * handleCos - localHandleY * handleSin - width / 2
            y: cropOverlay.boxY + cropOverlay.boxH / 2
               + localHandleX * handleSin + localHandleY * handleCos - height / 2

            width: 10; height: 10; radius: 2
            color: "white"
            z: 10   // above cropBorder

            MouseArea {
                anchors.fill: parent
                cursorShape: {
                    if (handleRect.onLeft  && handleRect.onTop)    return Qt.SizeFDiagCursor;
                    if (handleRect.onRight && handleRect.onBottom) return Qt.SizeFDiagCursor;
                    if (handleRect.onRight && handleRect.onTop)    return Qt.SizeBDiagCursor;
                    if (handleRect.onLeft  && handleRect.onBottom) return Qt.SizeBDiagCursor;
                    if (handleRect.onTop   || handleRect.onBottom) return Qt.SizeVerCursor;
                    return Qt.SizeHorCursor;
                }
                property real startX:  0; property real startY:  0
                property real startBX: 0; property real startBY: 0
                property real startBW: 0; property real startBH: 0

                onPressed: (mouse) => {
                    startX  = mouse.x; startY  = mouse.y;
                    startBX = cropOverlay.boxX; startBY = cropOverlay.boxY;
                    startBW = cropOverlay.boxW; startBH = cropOverlay.boxH;
                }
                onPositionChanged: (mouse) => {
                    const dx = mouse.x - startX;
                    const dy = mouse.y - startY;
                    let nx = startBX, ny = startBY;
                    let nw = startBW, nh = startBH;

                    // Left edge
                    if (handleRect.onLeft) {
                        nw = Math.max(cropOverlay.minSize, startBW - dx);
                        nx = startBX + startBW - nw;
                    }
                    // Right edge
                    if (handleRect.onRight) {
                        nw = Math.max(cropOverlay.minSize, startBW + dx);
                    }
                    // Top edge
                    if (handleRect.onTop) {
                        nh = Math.max(cropOverlay.minSize, startBH - dy);
                        ny = startBY + startBH - nh;
                    }
                    // Bottom edge
                    if (handleRect.onBottom) {
                        nh = Math.max(cropOverlay.minSize, startBH + dy);
                    }

                    // Clamp to canvas bounds
                    nx = Math.max(0, nx);
                    ny = Math.max(0, ny);
                    nw = Math.min(nw, cropOverlay.width  - nx);
                    nh = Math.min(nh, cropOverlay.height - ny);

                    cropOverlay.boxX = nx; cropOverlay.boxY = ny;
                    cropOverlay.boxW = nw; cropOverlay.boxH = nh;
                }
            }
        }
    }

    // ── Confirm / Cancel buttons ──────────────────────────────────────────────
    Row {
        id: actionRow
        // Centre below the crop box; clamp so it stays on screen
        x: Math.round(cropOverlay.boxX + (cropOverlay.boxW - width) * 0.5)
        y: Math.min(cropOverlay.boxY + cropOverlay.boxH + 12,
                    cropOverlay.height - height - 8)
        spacing: 8
        z: 20

        Button {
            text: "\u21ba"
            implicitWidth: 30; implicitHeight: 30
            onClicked: cropOverlay.rotation -= 1
            background: Rectangle { color: parent.hovered ? "#1e2438" : "#171c2a"; radius: 5; border.color: "#252d45" }
            contentItem: Label { text: parent.text; color: "#c8d0e0"; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter }
        }
        Button {
            text: "\u21bb"
            implicitWidth: 30; implicitHeight: 30
            onClicked: cropOverlay.rotation += 1
            background: Rectangle { color: parent.hovered ? "#1e2438" : "#171c2a"; radius: 5; border.color: "#252d45" }
            contentItem: Label { text: parent.text; color: "#c8d0e0"; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter }
        }
        Button {
            text: "\u2713  Crop"
            implicitWidth: 86; implicitHeight: 30
            onClicked: cropOverlay.confirm()
            background: Rectangle {
                color: parent.hovered ? "#252d6a" : "#1c2058"
                radius: 7; border.color: "#3d41a0"
            }
            contentItem: Label {
                text: "\u2713  Crop"; color: "#c7d2fe"
                font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter
            }
        }
        Button {
            text: "\u2715  Cancel"
            implicitWidth: 86; implicitHeight: 30
            onClicked: { if (cropOverlay.docCtrl) cropOverlay.docCtrl.activeTool = 0; }
            background: Rectangle {
                color: parent.hovered ? "#1e2438" : "#171c2a"
                radius: 7; border.color: "#252d45"
            }
            contentItem: Label {
                text: "\u2715  Cancel"; color: "#8892a4"
                font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
```

---

# File: `core\image-core\RenderPipeline.hpp`
```cpp
#pragma once
#include "editor-core/DocumentModel.hpp"
#include "shared-types/Adjustment.hpp"
#include "shared-types/Layer.hpp"
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QSize>
#include <QTransform>
#include <QVector>
#include <atomic>
#include <memory>
#include <vector>

namespace lumen {

// Carries one mask + the adjustments that should only apply inside that mask.
// Used for issue-5 per-mask local adjustments.
struct MaskAdjLayer {
    QImage              mask;
    QVector<Adjustment> adjustments;
    // Empty = base image (see Mask::targetLayerId, which this mirrors).
    QString             targetLayerId;
};

class RenderPipeline {
public:
    // ── Legacy preview (single global adjustment set, single optional mask) ──
    [[nodiscard]] QImage renderPreview(
        const DocumentModel& document, QSize maximumSize,
        std::shared_ptr<std::atomic<bool>> cancelled = nullptr) const;

    [[nodiscard]] QImage renderPreviewFromData(
        const QImage& source,
        const QVector<Adjustment>& adjustments,
        QSize maximumSize,
        const QImage& mask = {},
        std::shared_ptr<std::atomic<bool>> cancelled = nullptr) const;

    // ── Full pipeline preview (issue 5 + 6) ───────────────────────────────────
    // globalAdjustments : applied to the whole composite (targetMaskId == "")
    // maskAdjLayers     : per-mask local adjustments (issue 5)
    // overlayLayers     : additional image layers to composite on top (issue 6)
    // layerImages       : keyed by layer.id
    [[nodiscard]] QImage renderWithLayers(
        const QImage& baseSource,
        const QVector<Adjustment>& globalAdjustments,
        const std::vector<MaskAdjLayer>& maskAdjLayers,
        const QVector<Layer>& overlayLayers,
        const QHash<QString, QImage>& layerImages,
        QSize maximumSize,
        std::shared_ptr<std::atomic<bool>> cancelled = nullptr) const;

    // ── Full-resolution export ─────────────────────────────────────────────────
    [[nodiscard]] QImage renderFullResolution(const DocumentModel& document) const;

    // Root-cause fix for "overlay masks don't follow the overlay": layer-
    // scoped mask pixel data is baked into the OWNING layer's own native
    // pixel space at paint-commit time (see
    // DocumentController::bakeMaskForTarget()/unbakeMaskFromTarget()),
    // rather than staying in fixed base-image canvas space and being
    // re-sampled against the layer's CURRENT transform on every render
    // (which was the original, incorrect design -- it made the mask
    // sample "whatever's now under the layer's current position" instead
    // of staying attached to the layer's own surface as it moves).
    //
    // This is the shared geometry both directions of that bake/un-bake
    // need, and is also public so DocumentController can call it directly
    // (matches how DocumentController already holds a RenderPipeline
    // instance for preview rendering).
    //
    // Returns the transform mapping a point in BASE-IMAGE canvas pixel
    // space to `layer`'s own native pixel space -- the exact inverse of
    // how compositeOverlayLayers() places that layer's image onto the
    // canvas. *ok is set to false (and an identity transform returned) if
    // the layer's effective on-canvas size would be degenerate.
    [[nodiscard]] static QTransform canvasToLayerLocalTransform(
        const Layer& layer, QSize baseImageSize, QSize nativeImgSize, bool* ok = nullptr);

private:
    [[nodiscard]] QImage applyAdjustments(
        QImage image,
        const QVector<Adjustment>& adjustments,
        const QImage& mask = {},
        std::shared_ptr<std::atomic<bool>> cancelled = nullptr) const;

    void blendOnto(QImage& canvas, const QImage& layer,
                   BlendMode mode, double opacity) const;

    // Composite overlay layers on top of canvas.
    // scale: factor from source coords to canvas coords (for preview downsample).
    // maskAdjLayers: the full set (base- AND layer-scoped); each overlay
    // layer applies only the ones whose targetLayerId matches its own id
    // -- applied directly to that layer's own pixels with no further
    // warping, since layer-scoped masks are already baked into that
    // layer's own native pixel space by the time they get here (see
    // canvasToLayerLocalTransform()'s comment above).
    void compositeOverlayLayers(QImage& canvas,
                                const QVector<Layer>& layers,
                                const QHash<QString, QImage>& layerImages,
                                double scale,
                                const std::vector<MaskAdjLayer>& maskAdjLayers) const;

    static std::vector<double> buildCurveLut(const QJsonArray& points);
};

} // namespace lumen
```

---

# File: `core\image-core\RenderPipeline.cpp`
```cpp
#include "image-core/RenderPipeline.hpp"
#include "image-core/BlendModes.hpp"
#include "image-core/ColorManager.hpp"
#include <QJsonArray>
#include <QJsonValue>
#include <QPainter>
#include <QTransform>
#include <QElapsedTimer>
#include <QDebug>
#include <QtMath>
#include <algorithm>
#ifdef HAVE_OPENCV
#  include <opencv2/core.hpp>
#  include <opencv2/imgproc.hpp>
#  include <opencv2/photo.hpp>
#endif
namespace lumen {
namespace {
quint16 clamp16(double v) { return static_cast<quint16>(qBound(0.0,v,65535.0)); }
double scalarAdj(const QVector<Adjustment>& adjs, AdjustmentType t) {
    for (const auto& a : adjs) if (a.type==t && a.enabled) return a.parameters.value("value").toDouble(0.0);
    return 0.0;
}
const Adjustment* findAdj(const QVector<Adjustment>& adjs, AdjustmentType t) {
    for (const auto& a : adjs) if (a.type==t && a.enabled) return &a;
    return nullptr;
}
} // namespace

// ── Curve LUT ─────────────────────────────────────────────────────────────────

std::vector<double> RenderPipeline::buildCurveLut(const QJsonArray& points) {
    std::vector<double> lut(65536);
    if (points.isEmpty()) { for (int i=0;i<65536;++i) lut[i]=i; return lut; }
    QVector<QPair<double,double>> pts; pts.append({0.0,0.0});
    for (const QJsonValue& v : points) { const QJsonArray p=v.toArray(); if (p.size()>=2) pts.append({p[0].toDouble(),p[1].toDouble()}); }
    pts.append({1.0,1.0});
    std::sort(pts.begin(),pts.end(),[](auto& a,auto& b){return a.first<b.first;});
    for (int i=0;i<65536;++i) {
        const double x=i/65535.0; double y=x;
        for (int k=1;k<pts.size();++k) {
            if (x<=pts[k].first) { const double t=(x-pts[k-1].first)/(pts[k].first-pts[k-1].first+1e-9);
                y=pts[k-1].second+t*(pts[k].second-pts[k-1].second); break; }
        }
        lut[i]=qBound(0.0,y*65535.0,65535.0);
    }
    return lut;
}

// ── Legacy preview paths ───────────────────────────────────────────────────────

QImage RenderPipeline::renderPreview(const DocumentModel& doc, QSize sz, std::shared_ptr<std::atomic<bool>> c) const {
    if (!doc.hasDocument()) return {};
    return renderPreviewFromData(doc.sourceImage(),doc.adjustments(),sz,doc.activeMask(),c);
}

QImage RenderPipeline::renderPreviewFromData(const QImage& src, const QVector<Adjustment>& adjs,
    QSize maxSz, const QImage& mask, std::shared_ptr<std::atomic<bool>> cancelled) const {
    if (src.isNull()) return {};
    QImage s=src;
    if (maxSz.isValid()&&(s.width()>maxSz.width()||s.height()>maxSz.height()))
        s=s.scaled(maxSz,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    QImage sm=mask.isNull()?QImage():mask.scaled(s.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    return applyAdjustments(s,adjs,sm,cancelled);
}

// ── Full pipeline render (issues 5 + 6) ──────────────────────────────────────

QImage RenderPipeline::renderWithLayers(
    const QImage& baseSource,
    const QVector<Adjustment>& globalAdjustments,
    const std::vector<MaskAdjLayer>& maskAdjLayers,
    const QVector<Layer>& overlayLayers,
    const QHash<QString, QImage>& layerImages,
    QSize maximumSize,
    std::shared_ptr<std::atomic<bool>> cancelled) const
{
    const bool profile = qEnvironmentVariableIsSet("LUMEN_PROFILE");
    QElapsedTimer profileTimer;
    if (profile) profileTimer.start();
    if (baseSource.isNull()) return {};

    // -- Step 1: scale source to preview size ----------------------------------
    double previewScale = 1.0;
    QImage scaledSrc = baseSource;
    if (maximumSize.isValid() &&
        (baseSource.width() > maximumSize.width() || baseSource.height() > maximumSize.height())) {
        previewScale = std::min(
            static_cast<double>(maximumSize.width())  / baseSource.width(),
            static_cast<double>(maximumSize.height()) / baseSource.height());
        scaledSrc = baseSource.scaled(maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (cancelled && *cancelled) return {};

    // -- Step 2: apply global adjustments (no mask) ---------------------------
    QImage result = applyAdjustments(scaledSrc, globalAdjustments, {}, cancelled);
    if (result.isNull() || (cancelled && *cancelled)) return {};

    // -- Step 3: apply per-mask local adjustments, BASE-IMAGE-scoped masks
    //    only (issue 5). Masks scoped to a specific overlay layer (see
    //    Mask::targetLayerId / MaskAdjLayer::targetLayerId) must never
    //    affect the base composite -- those are applied to that layer's
    //    own pixels inside compositeOverlayLayers() below instead. -------
    for (const auto& ml : maskAdjLayers) {
        if (!ml.targetLayerId.isEmpty()) continue;
        if (cancelled && *cancelled) return {};
        if (ml.adjustments.isEmpty()) continue;
        QImage scaledMask = ml.mask.isNull() ? QImage()
            : ml.mask.scaled(result.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        result = applyAdjustments(result, ml.adjustments, scaledMask, cancelled);
        if (result.isNull()) return {};
    }

    // -- Step 4: composite overlay layers (issue 6), each applying its own
    //    layer-scoped masked adjustments to its own pixels first --------
    if (overlayLayers.size() > 1) {
        compositeOverlayLayers(result, overlayLayers, layerImages, previewScale, maskAdjLayers);
    }

    if (profile)
        qInfo().noquote() << "PROFILE renderWithLayers ms="
                          << profileTimer.nsecsElapsed()/1000000.0
                          << "size=" << result.size()
                          << "layers=" << overlayLayers.size();
    return result;
}

// ── Overlay layer compositing (issue 6) ───────────────────────────────────────

QTransform RenderPipeline::canvasToLayerLocalTransform(
    const Layer& layer, QSize baseImageSize, QSize nativeImgSize, bool* ok)
{
    if (ok) *ok = false;
    if (nativeImgSize.width() <= 0 || nativeImgSize.height() <= 0)
        return QTransform();

    const double cxNative = baseImageSize.width()  * 0.5 + layer.posX;
    const double cyNative = baseImageSize.height() * 0.5 + layer.posY;
    const double dwNative = nativeImgSize.width()  * layer.scaleX;
    const double dhNative = nativeImgSize.height() * layer.scaleY;
    if (dwNative < 1.0 || dhNative < 1.0) return QTransform();

    QTransform localToBase;
    localToBase.translate(cxNative, cyNative);
    if (!qFuzzyIsNull(layer.rotation)) localToBase.rotate(layer.rotation);
    localToBase.translate(-dwNative * 0.5, -dhNative * 0.5);
    localToBase.scale(dwNative / nativeImgSize.width(), dhNative / nativeImgSize.height());

    if (ok) *ok = true;
    return localToBase.inverted();
}

void RenderPipeline::compositeOverlayLayers(
    QImage& canvas,
    const QVector<Layer>& layers,
    const QHash<QString, QImage>& layerImages,
    double scale,
    const std::vector<MaskAdjLayer>& maskAdjLayers) const
{
    const bool profile = qEnvironmentVariableIsSet("LUMEN_PROFILE");
    QElapsedTimer profileTimer;
    if (profile) profileTimer.start();
    // Sort by order so base (order==0) is bottom, overlays are on top.
    QVector<Layer> sorted = layers;
    std::sort(sorted.begin(), sorted.end(),
              [](const Layer& a, const Layer& b){ return a.order < b.order; });

    // QPainter needs a premultiplied surface for correct alpha compositing.
    QImage comp = canvas.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&comp);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    for (const Layer& layer : sorted) {
        if (layer.isBaseLayer() || !layer.visible) continue;  // skip base layer
        QImage img = layerImages.value(layer.id);
        if (img.isNull()) continue;

        // Apply this layer's OWN masked adjustments to its OWN pixels
        // before compositing. Layer-scoped masks are already baked into
        // THIS layer's own native pixel space at paint-commit time (see
        // DocumentController::bakeMaskForTarget()) -- they are applied
        // directly here, with no further warping, and are therefore
        // naturally carried along by the SAME placement transform used
        // to draw `img` onto the canvas below. This is the fix for
        // "overlay masks don't follow the overlay": the previous design
        // re-derived the mask's placement from the layer's CURRENT
        // transform on every render (via what's now
        // canvasToLayerLocalTransform()), which meant moving the layer
        // sampled "whatever's now under the new position" instead of the
        // mask staying attached to the layer's own surface.
        for (const auto& ml : maskAdjLayers) {
            if (ml.targetLayerId != layer.id || ml.adjustments.isEmpty() || ml.mask.isNull()) continue;
            const QImage localMask = (ml.mask.size() == img.size())
                ? ml.mask
                : ml.mask.scaled(img.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            img = applyAdjustments(img, ml.adjustments, localMask);
        }

        painter.save();
        painter.setOpacity(layer.opacity);

        // Centre of placement in canvas coords.
        // posX/posY are in BASE-IMAGE pixels; scale converts to canvas pixels.
        const double cx = comp.width()  * 0.5 + layer.posX * scale;
        const double cy = comp.height() * 0.5 + layer.posY * scale;

        // Destination size at current preview scale.
        const int dw = qMax(1, qRound(img.width()  * layer.scaleX * scale));
        const int dh = qMax(1, qRound(img.height() * layer.scaleY * scale));

        painter.translate(cx, cy);
        if (!qFuzzyIsNull(layer.rotation))
            painter.rotate(layer.rotation);
        painter.drawImage(QRectF(-dw * 0.5, -dh * 0.5, dw, dh), img);
        painter.restore();
    }
    painter.end();

    // Convert back to RGBA64 to stay consistent with the rest of the pipeline.
    canvas = comp.convertToFormat(QImage::Format_RGBA64);
    if (profile)
        qInfo().noquote() << "PROFILE compositeOverlayLayers ms="
                          << profileTimer.nsecsElapsed()/1000000.0
                          << "layers=" << layers.size();
}

// ── Full-resolution export ────────────────────────────────────────────────────

QImage RenderPipeline::renderFullResolution(const DocumentModel& doc) const {
    if (!doc.hasDocument()) return {};

    // Global adjustments are those with targetMaskId=="" — adjustmentsForLayer()
    // filters on targetLayerId instead, which every slider-set adjustment leaves
    // empty, so it was returning EVERY adjustment (global AND mask-scoped) and
    // applying it to the whole image. That double-applied mask-scoped edits
    // (once here, once more in the old per-mask loop below) and made exported
    // files not match the on-screen preview, which already uses
    // adjustmentsForTarget("") correctly. Use the same call here.
    const QVector<Adjustment> globalAdjustments = doc.adjustmentsForTarget(QString());

    std::vector<MaskAdjLayer> maskAdjLayers;
    for (const Mask& mask : doc.masks()) {
        if (mask.mask.isNull()) continue;
        const QVector<Adjustment> maskAdjs = doc.adjustmentsForTarget(mask.id);
        if (maskAdjs.isEmpty()) continue;
        maskAdjLayers.push_back({mask.mask, maskAdjs, mask.targetLayerId});
    }

    const QVector<Layer> layers = doc.layers();
    QHash<QString, QImage> layerImages;
    for (const Layer& layer : layers)
        layerImages.insert(layer.id, doc.layerImage(layer.id));

    // QSize() is deliberately invalid (not QSize(0,0)) so renderWithLayers'
    // "only downscale if maximumSize.isValid()" check is skipped entirely,
    // producing a native-resolution render through the exact same
    // adjustment/mask/overlay pipeline DocumentController::rebuildPreview()
    // and buildHqPreview() already use for the on-screen preview -- including
    // compositeOverlayLayers(), which this export path never called before.
    return renderWithLayers(doc.sourceImage(), globalAdjustments, maskAdjLayers,
                             layers, layerImages, QSize());
}

// ── Blend onto canvas ─────────────────────────────────────────────────────────

void RenderPipeline::blendOnto(QImage& canvas,const QImage& layer,BlendMode mode,double opacity) const {
    const int W=qMin(canvas.width(),layer.width()),H=qMin(canvas.height(),layer.height());
    for (int y=0;y<H;++y) {
        auto* dst=reinterpret_cast<QRgba64*>(canvas.scanLine(y));
        const auto* src=reinterpret_cast<const QRgba64*>(layer.constScanLine(y));
        for (int x=0;x<W;++x) {
            const double bR=dst[x].red()/65535.0,bG=dst[x].green()/65535.0,bB=dst[x].blue()/65535.0,bA=dst[x].alpha()/65535.0;
            const double lR=src[x].red()/65535.0,lG=src[x].green()/65535.0,lB=src[x].blue()/65535.0,lA=src[x].alpha()/65535.0;
            const double eff=opacity*lA;
            dst[x]=QRgba64::fromRgba64(clamp16(blend::compose(bR,lR,eff,mode)*65535),clamp16(blend::compose(bG,lG,eff,mode)*65535),
                clamp16(blend::compose(bB,lB,eff,mode)*65535),clamp16((bA+lA*opacity*(1.0-bA))*65535));
        }
    }
}

// ── Core pixel pipeline ───────────────────────────────────────────────────────

QImage RenderPipeline::applyAdjustments(QImage image,const QVector<Adjustment>& adjustments,
    const QImage& mask,std::shared_ptr<std::atomic<bool>> cancelled) const {
    image=image.convertToFormat(QImage::Format_RGBA64);
    const int rot=int(scalarAdj(adjustments,AdjustmentType::RotationDegrees))%360;
    const bool fh=scalarAdj(adjustments,AdjustmentType::FlipHorizontal)>0.5;
    const bool fv=scalarAdj(adjustments,AdjustmentType::FlipVertical)>0.5;
    if (fh||fv) image=image.mirrored(fh,fv);
    if (rot!=0) { QTransform t; t.rotate(rot); image=image.transformed(t,Qt::SmoothTransformation); }

    const double combinedGain = qPow(2.0, scalarAdj(adjustments,AdjustmentType::Exposure))
                               * qPow(2.0, scalarAdj(adjustments,AdjustmentType::Brightness)/100.0);
    const double cg  = 1.0+scalarAdj(adjustments,AdjustmentType::Contrast)/100.0;
    const double hl  = scalarAdj(adjustments,AdjustmentType::Highlights)/100.0;
    const double sh  = scalarAdj(adjustments,AdjustmentType::Shadows)/100.0;
    const double wg  = 1.0+scalarAdj(adjustments,AdjustmentType::Whites)*0.005;
    const double bof = (scalarAdj(adjustments,AdjustmentType::Blacks)/100.0)*0.15*65535.0;
    const double sg  = 1.0+scalarAdj(adjustments,AdjustmentType::Saturation)/100.0;
    const double vib = scalarAdj(adjustments,AdjustmentType::Vibrance)/100.0;
    const double tmp = scalarAdj(adjustments,AdjustmentType::Temperature);
    const double tnt = scalarAdj(adjustments,AdjustmentType::Tint);
    const double rbal=1.0+tmp/400.0, bbal=1.0-tmp/400.0, gbal=1.0+tnt/400.0;

    bool hL=false,hR=false,hG=false,hB=false;
    std::vector<double> lumaLut,rLut,gLut,bLut;
    if (const Adjustment* a=findAdj(adjustments,AdjustmentType::ToneCurveLuma)){lumaLut=buildCurveLut(a->parameters.value("points").toArray());hL=true;}
    if (const Adjustment* a=findAdj(adjustments,AdjustmentType::ToneCurveR)){rLut=buildCurveLut(a->parameters.value("points").toArray());hR=true;}
    if (const Adjustment* a=findAdj(adjustments,AdjustmentType::ToneCurveG)){gLut=buildCurveLut(a->parameters.value("points").toArray());hG=true;}
    if (const Adjustment* a=findAdj(adjustments,AdjustmentType::ToneCurveB)){bLut=buildCurveLut(a->parameters.value("points").toArray());hB=true;}

    const bool hasMask=!mask.isNull();
    QImage sm=hasMask?mask.scaled(image.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32):QImage();

    for (int y=0;y<image.height();++y) {
        if (cancelled&&*cancelled) return {};
        auto* sc=reinterpret_cast<QRgba64*>(image.scanLine(y));
        for (int x=0;x<image.width();++x) {
            const QRgba64 op=sc[x];
            double r=op.red()*combinedGain, g=op.green()*combinedGain, b=op.blue()*combinedGain;
            r=(r*wg)+bof; g=(g*wg)+bof; b=(b*wg)+bof;
            r=((r/65535.0-0.5)*cg+0.5)*65535.0; g=((g/65535.0-0.5)*cg+0.5)*65535.0; b=((b/65535.0-0.5)*cg+0.5)*65535.0;
            double lm=(0.2126*r+0.7152*g+0.0722*b)/65535.0;
            if (lm>0.5){double bl=(lm-0.5)*2.0,gn=1.0+hl*bl; r*=gn;g*=gn;b*=gn;}
            lm=(0.2126*r+0.7152*g+0.0722*b)/65535.0;
            if (lm<0.5){double bl=(0.5-lm)*2.0,gn=1.0+sh*bl; r*=gn;g*=gn;b*=gn;}
            const double l16=0.2126*r+0.7152*g+0.0722*b;
            r=l16+(r-l16)*sg; g=l16+(g-l16)*sg; b=l16+(b-l16)*sg;
            const double sm2=qMax(r,qMax(g,b)),smi=qMin(r,qMin(g,b));
            const double cf=(sm2-smi)/(sm2+1e-6),vg=1.0+vib*(1.0-cf);
            r=l16+(r-l16)*vg; g=l16+(g-l16)*vg; b=l16+(b-l16)*vg;
            r*=rbal; g*=gbal; b*=bbal;
            if (hR) r=rLut[clamp16(r)]; if (hG) g=gLut[clamp16(g)]; if (hB) b=bLut[clamp16(b)];
            if (hL){const double l2=0.2126*r+0.7152*g+0.0722*b,l2m=lumaLut[clamp16(l2)],sc2=l2>0?l2m/l2:1.0; r*=sc2;g*=sc2;b*=sc2;}
            if (hasMask){
                const QRgb mp=reinterpret_cast<const QRgb*>(sm.constScanLine(y))[x];
                const double al=qAlpha(mp)/255.0;
                r=op.red()*(1.0-al)+r*al; g=op.green()*(1.0-al)+g*al; b=op.blue()*(1.0-al)+b*al;
            }
            sc[x]=QRgba64::fromRgba64(clamp16(r),clamp16(g),clamp16(b),op.alpha());
        }
    }
#ifdef HAVE_OPENCV
    const double nr=scalarAdj(adjustments,AdjustmentType::NoiseReduction);
    const double shp=scalarAdj(adjustments,AdjustmentType::Sharpening);
    if (nr>0.0||shp>0.0) {
        QImage img8=image.convertToFormat(QImage::Format_RGB888);
        cv::Mat bgrMat;
        { cv::Mat tmp(img8.height(),img8.width(),CV_8UC3,const_cast<uchar*>(img8.constBits()),static_cast<size_t>(img8.bytesPerLine()));
          cv::cvtColor(tmp,bgrMat,cv::COLOR_RGB2BGR); }
        cv::Mat work=bgrMat;
        if (nr>0.0){cv::Mat f; cv::bilateralFilter(bgrMat,f,-1,8.0+nr*0.4,4.0+nr*0.12); work=std::move(f);}
        if (shp>0.0){
            const double sigma=0.6+shp*0.025,amount=shp*0.018;
            const int thresh=static_cast<int>(shp*0.3);
            cv::Mat blurred; cv::GaussianBlur(work,blurred,cv::Size(0,0),sigma);
            cv::Mat sc2=work.clone();
            for (int y=0;y<work.rows;++y){
                const uchar* s=sc2.ptr<uchar>(y); const uchar* bl=blurred.ptr<uchar>(y); uchar* d=work.ptr<uchar>(y);
                for (int x=0;x<work.cols*3;++x){
                    const int e=int(s[x])-int(bl[x]);
                    d[x]=std::abs(e)>thresh?static_cast<uchar>(std::clamp(int(s[x])+int(std::round(amount*e)),0,255)):s[x];
                }
            }
        }
        cv::Mat ro; cv::cvtColor(work,ro,cv::COLOR_BGR2RGB);
        QImage res(ro.data,ro.cols,ro.rows,static_cast<int>(ro.step),QImage::Format_RGB888);
        image=res.copy().convertToFormat(QImage::Format_RGBA64);
    }
#endif
    return image;
}
} // namespace lumen
```

---

# File: `app\CMakeLists.txt`
```text
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
        resources/qml/CropOverlay.qml
        resources/qml/LayerTransformOverlay.qml
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
target_include_directories(LumenForge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(LumenForge PROPERTIES
    WIN32_EXECUTABLE $<CONFIG:Release>
    MACOSX_BUNDLE TRUE
)
if(MSVC AND LUMEN_ENABLE_ONNX AND DEFINED onnxruntime_DIR)
    get_filename_component(ONNXRUNTIME_PREFIX "${onnxruntime_DIR}/../.." ABSOLUTE)
    set(ONNXRUNTIME_RUNTIME_DIR "${ONNXRUNTIME_PREFIX}/bin")

    if(NOT EXISTS "${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime.dll")
        message(FATAL_ERROR "ONNX Runtime DLL not found: ${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime.dll")
    endif()

    target_link_options(LumenForge PRIVATE /DELAYLOAD:onnxruntime.dll)
    target_link_libraries(LumenForge PRIVATE delayimp)

    # Qt's deployment step only copies Qt dependencies.  Copy all DLLs CMake
    # resolves for this executable as well, including ONNX Runtime and its
    # vcpkg-provided transitive dependencies.
    add_custom_command(TARGET LumenForge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:LumenForge>
            $<TARGET_FILE_DIR:LumenForge>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime_providers_shared.dll
            ${ONNXRUNTIME_RUNTIME_DIR}/re2.dll
            ${ONNXRUNTIME_RUNTIME_DIR}/libprotobuf.dll
            $<TARGET_FILE_DIR:LumenForge>
        COMMAND_EXPAND_LISTS
    )
endif()



install(TARGETS LumenForge BUNDLE DESTINATION . RUNTIME DESTINATION bin)
```

---

# File: `CURRENT_STATE.md`
```markdown
# LumenForge — Current Engineering State

Updated: 2026-07-22

This note records the current uncommitted edits and their verification state
for the next development pass.

## Working-tree edits

### Base-layer insertion correction

`core/editor-core/DocumentModel.cpp` now calls `m_layers.push_back(base)`
immediately after assigning `kBaseLayerSourceAssetId` in
`DocumentModel::openSourceImage()`.

The prior committed layer-identity change accidentally placed this call after
a `//` comment, so opening a source image created its pixel entry without
adding the base `Layer` to `m_layers`. This one-line correction restores the
base-layer invariant. The broader layer-identity milestone is otherwise in
commit `436e84f` (`Fix base layer identity and reorder handling`).

### ONNX Runtime Release deployment

`app/CMakeLists.txt` now deploys the ONNX Runtime DLLs needed by a Release
build when `LUMEN_ENABLE_ONNX=ON`:

- `onnxruntime.dll`
- `onnxruntime_providers_shared.dll`
- `abseil_dll.dll`
- `libprotobuf-lite.dll`
- `libprotobuf.dll`
- `re2.dll`

Qt's normal deployment step does not include these non-Qt runtime
dependencies. The CMake change resolves the ONNX Runtime package location,
fails configuration clearly if its primary DLL is unavailable, and copies the
runtime files after building `LumenForge`.

## Real-ESRGAN diagnostic result

The existing `models/realesrgan-x4plus.onnx` was not modified or regenerated.

Verified diagnostics:

| Item | Result |
| --- | --- |
| ONNX IR version | 6 |
| ONNX opset | `ai.onnx` 11 |
| Python `onnx` package | 1.22.0 |
| vcpkg ONNX Runtime | 1.23.2 |
| Python ONNX Runtime | 1.27.0 |
| `onnx.checker` | Passes |
| Tensor interface | `input [1,3,512,512]` → `output [1,3,2048,2048]` |

The model session loads successfully through the actual project
`OnnxSession` implementation with the deployed vcpkg DLL set:

```text
loaded=true
lastError=[]
```

Therefore, the current model is not incompatible with the project runtime.
The confirmed issue was Release deployment: required ONNX Runtime DLLs were
not copied alongside the executable. The earlier
`Invalid model` / `/conv_first/Conv` message is not reproducible from the unchanged current
model and should not be treated as a model-export defect.

## Layer renaming milestone

Layer renaming is now implemented across the model, controller, and layer
list UI:

- `DocumentModel::setLayerName()` trims input, rejects empty names, and records
  a single undoable `Rename layer: …` history step.
- `DocumentController::renameLayer()` exposes the operation to QML.
- The layer row enters an inline `TextInput` on double-click; Enter or focus
  loss commits the trimmed name.

The Release build completed successfully after this change. No manual UI
verification has been performed; that remains a developer-side check.

## Drag responsiveness milestone

Transform drags now avoid preview-composite churn while the gesture is active:

- `DocumentController` still emits `layersChanged()` on every transform tick,
  so the QML transform overlay remains live.
- While `m_layerTransformEditOpen` is true, the document-change handler skips
  starting the asynchronous preview and HQ-preview timers.
- Beginning a transform cancels stale preview work and invalidates its request
  id; committing the transform schedules one debounced preview rebuild.

This preserves the existing single undo transaction for move/resize/rotate,
while preventing a full 1400×1050 composite from being launched repeatedly
during a drag. Crop-box movement was already QML-only until confirmation, and
canvas pan remains local `Flickable` state, so neither path needed backend
changes. The Release build passes after this optimization; manual interaction
verification remains outstanding.

## Verification performed

- Configured with `LUMEN_ENABLE_ONNX=ON`.
- Built the Release solution successfully after the deployment change.
- Confirmed all six ONNX Runtime DLLs listed above exist in `build/Release`.
- Invoked `OnnxSession::ensureLoaded()` against the current model using that
  deployed runtime set; it returned `true` and left `lastError()` empty.
- Ran `git diff --check`; no whitespace errors were reported for the current
  edits.

## Next safe work

No Real-ESRGAN model or export-script change is required. The next AI task,
if desired, is a functional inference/performance pass (including the known
tile-overlap blending limitation) after manual app-level verification.

For UI work, resume the roadmap's design-system milestone (`Theme.qml`, icon
assets, and shared QML controls). Keep it independent from the AI work.

## Working-tree status at handoff

- Modified: `core/editor-core/DocumentModel.cpp`
- Modified: `app/CMakeLists.txt`
- Modified: `core/editor-core/DocumentModel.hpp`
- Modified: `app/src/editor/DocumentController.hpp`
- Modified: `app/src/editor/DocumentController.cpp`
- Modified: `app/resources/qml/Main.qml`
- Modified: `app/src/editor/DocumentController.cpp`
- New: `CURRENT_STATE.md`
- Untracked: `Real-ESRGAN/` (existing local checkout; not modified by this
  handoff)

These edits are intentionally uncommitted.
```
