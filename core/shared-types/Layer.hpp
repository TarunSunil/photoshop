#pragma once
#include <QColor>
#include <QRectF>
#include <QString>
namespace lumen {
enum class LayerKind { Image, Adjustment, Text, Shape, Group };
enum class BlendMode  { Normal, Multiply, Screen, Overlay, SoftLight, HardLight, Difference };

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

    // Overlay transform (issue 6) — applies to order > 0 image layers.
    // Positions are in base-image pixels relative to the base canvas centre.
    // scaleX/scaleY are factors relative to the layer's own pixel dimensions.
    // rotation is in degrees, clockwise.
    double    posX          = 0.0;
    double    posY          = 0.0;
    double    scaleX        = 1.0;
    double    scaleY        = 1.0;
    double    rotation      = 0.0;
};
} // namespace lumen