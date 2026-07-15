#pragma once

#include <QImage>
#include <QString>

namespace lumen {

enum class MaskKind {
    Brush,
    Gradient,
    Radial,
    AiGenerated
};

struct Mask {
    QString id;
    QString name;
    MaskKind kind = MaskKind::Brush;
    QString assetPath;
    QImage mask;
    double featherRadius = 0.0;
    bool inverted = false;
    // Which layer this mask's adjustments apply to -- empty means the
    // base image (the only option before this field existed, and still
    // the default for newly-created masks when no overlay layer is
    // selected). A non-empty value is a Layer::id, matching whichever
    // overlay layer was selected (DocumentController::m_selectedLayerId)
    // at the moment this mask was created. Masks are always painted at
    // base-image pixel resolution regardless of this value (MaskCanvas.qml
    // doesn't change based on layer selection) -- RenderPipeline is what
    // warps a layer-scoped mask into that layer's own pixel space at
    // render time.
    QString targetLayerId;
};

} // namespace lumen
