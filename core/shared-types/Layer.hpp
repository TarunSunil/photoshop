#pragma once
#include <QColor>
#include <QRectF>
#include <QString>
namespace lumen {
enum class LayerKind {
    Image,
    Adjustment,
    Text,
    Shape,
    Group
};
enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    SoftLight,
    HardLight,
    Difference
};
struct Layer {
    QString   id;
    QString   name;
    LayerKind kind          = LayerKind::Image;
    BlendMode blendMode     = BlendMode::Normal;
    QString   sourceAssetId;
    double    opacity       = 1.0;
    bool      visible       = true;
    bool      locked        = false;
    int       order         = 0;
    // Text layers
    QString   text;
    QColor    textColor     = Qt::white;
    int       textSize      = 24;
    // Shape layers
    QColor    fillColor     = Qt::black;
    QRectF    shapeRect;
};
} // namespace lumen