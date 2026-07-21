#pragma once
#include <QColor>
#include <QRectF>
#include <QString>
namespace lumen {
enum class LayerKind { Image, Adjustment, Text, Shape, Group };
enum class BlendMode  { Normal, Multiply, Screen, Overlay, SoftLight, HardLight, Difference };

// Identity marker for the document's base/background layer. Set once by
// DocumentModel::openSourceImage(). Callers should use Layer::isBaseLayer()
// rather than referencing this directly.
inline const QString kBaseLayerSourceAssetId = QStringLiteral("source");

struct Layer {
    QString   id;
    QString   name;
    LayerKind kind          = LayerKind::Image;
    BlendMode blendMode     = BlendMode::Normal;
    QString   sourceAssetId;
    QString   sourcePath;
    double    opacity       = 1.0;
    bool      visible       = true;
    bool      locked        = false;
    int       order         = 0;

    QString   text;
    QColor    textColor     = Qt::white;
    int       textSize      = 24;

    QColor    fillColor     = Qt::black;
    QRectF    shapeRect;

    double    posX          = 0.0;
    double    posY          = 0.0;
    double    scaleX        = 1.0;
    double    scaleY        = 1.0;
    double    rotation      = 0.0;

    // Identity check, independent of render-stacking order -- use this
    // instead of "order == 0" anywhere the base layer needs to be identified.
    [[nodiscard]] bool isBaseLayer() const { return sourceAssetId == kBaseLayerSourceAssetId; }

    bool operator==(const Layer&) const = default;
};
} // namespace lumen