#pragma once
#include "editor-core/DocumentModel.hpp"
#include "shared-types/Adjustment.hpp"
#include "shared-types/Layer.hpp"
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QSize>
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
    void compositeOverlayLayers(QImage& canvas,
                                const QVector<Layer>& layers,
                                const QHash<QString, QImage>& layerImages,
                                double scale) const;

    static std::vector<double> buildCurveLut(const QJsonArray& points);
};

} // namespace lumen