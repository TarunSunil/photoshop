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