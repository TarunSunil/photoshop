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
};

} // namespace lumen
