#pragma once
#include <QJsonObject>
#include <QString>
namespace lumen {
enum class AdjustmentType {
    // Tone
    Brightness,   // simple additive midtone lift/pull — appears above Exposure in the panel
    Exposure,
    Contrast,
    Highlights,
    Shadows,
    Whites,
    Blacks,
    // Color
    Saturation,
    Vibrance,
    Temperature,
    Tint,
    // Transform
    RotationDegrees,
    FlipHorizontal,
    FlipVertical,
    // Tone curves (parameters: JSON array [{x,y},...])
    ToneCurveLuma,
    ToneCurveR,
    ToneCurveG,
    ToneCurveB,
    // Detail
    NoiseReduction,
    Sharpening,
    // HSL per-channel (6 hues: R Y G C B M)
    HueShiftR, HueShiftY, HueShiftG, HueShiftC, HueShiftB, HueShiftM,
    SatShiftR,  SatShiftY,  SatShiftG,  SatShiftC,  SatShiftB,  SatShiftM,
    LumShiftR,  LumShiftY,  LumShiftG,  LumShiftC,  LumShiftB,  LumShiftM,
};
struct Adjustment {
    QString        id;
    AdjustmentType type          = AdjustmentType::Exposure;
    QJsonObject    parameters;
    QString        targetLayerId;
    QString        targetMaskId;
    bool           enabled       = true;
    int            order         = 0;
};
QString        adjustmentTypeToString(AdjustmentType type);
AdjustmentType adjustmentTypeFromString(const QString& value);
} // namespace lumen