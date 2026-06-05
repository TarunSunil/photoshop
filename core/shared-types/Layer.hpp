#pragma once

#include <QString>

namespace lumen {

enum class LayerKind {
    Image,
    Adjustment,
    Text,
    Shape,
    Group
};

struct Layer {
    QString id;
    QString name;
    LayerKind kind = LayerKind::Image;
    QString sourceAssetId;
    double opacity = 1.0;
    bool visible = true;
    bool locked = false;
    int order = 0;
};

} // namespace lumen
