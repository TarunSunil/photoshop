#pragma once

#include <QJsonObject>
#include <QString>

namespace lumen {

enum class AdjustmentType {
    Exposure,
    Contrast,
    Highlights,
    Shadows,
    Whites,
    Blacks,
    Saturation,
    Vibrance,
    Temperature,
    Tint,
    RotationDegrees,
    FlipHorizontal,
    FlipVertical
};

struct Adjustment {
    QString id;
    AdjustmentType type = AdjustmentType::Exposure;
    QJsonObject parameters;
    QString targetLayerId;
    QString targetMaskId;
    bool enabled = true;
    int order = 0;
};

QString adjustmentTypeToString(AdjustmentType type);
AdjustmentType adjustmentTypeFromString(const QString& value);

} // namespace lumen
