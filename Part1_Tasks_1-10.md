# PART 1 of 4 — Foundation — mask engine, shared types, image-core basics
# Tasks 1–10 (10 files)

## Checklist (mark [x] as each file is completed)
- [ ] Task 1: REPLACE `.gitignore`
- [ ] Task 2: REPLACE `core/mask-core/BrushEngine.hpp`
- [ ] Task 3: REPLACE `core/mask-core/BrushEngine.cpp`
- [ ] Task 4: REPLACE `core/shared-types/Layer.hpp`
- [ ] Task 5: REPLACE `core/shared-types/Adjustment.hpp`
- [ ] Task 6: CREATE `core/image-core/BlendModes.hpp`
- [ ] Task 7: CREATE `core/image-core/RawImporter.hpp`
- [ ] Task 8: CREATE `core/image-core/RawImporter.cpp`
- [ ] Task 9: CREATE `core/image-core/ColorManager.hpp`
- [ ] Task 10: CREATE `core/image-core/ColorManager.cpp`

---

## TASK 1 — REPLACE `.gitignore`

```
build/
out/
.qt/
.vs/
.vscode/
CMakeUserPresets.json
*.user
*.lfautosave
*.tmp
*.log
models/*.onnx
```


---

## TASK 2 — REPLACE `core/mask-core/BrushEngine.hpp`

```cpp
#pragma once
#include <QImage>
#include <QPointF>
#include <QSize>
namespace lumen {
class BrushEngine {
public:
    explicit BrushEngine(QSize size);
    void paintStroke(QPointF center, double radius, double opacity, bool erase);
    void feather(double radius);
    void clear();
    void resize(QSize size);
    [[nodiscard]] const QImage& mask() const;
    [[nodiscard]] QImage& mask();
private:
    QImage m_mask;
};
} // namespace lumen
```


---

## TASK 3 — REPLACE `core/mask-core/BrushEngine.cpp`

```cpp
#include "mask-core/BrushEngine.hpp"
#include <QPainter>
#include <QRadialGradient>
#include <cmath>
namespace lumen {
BrushEngine::BrushEngine(QSize size)
    : m_mask(size, QImage::Format_ARGB32)
{
    m_mask.fill(Qt::transparent);
}
void BrushEngine::paintStroke(QPointF center, double radius, double opacity, bool erase)
{
    if (m_mask.isNull()) return;
    QPainter painter(&m_mask);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(
        erase ? QPainter::CompositionMode_Clear
              : QPainter::CompositionMode_SourceOver);
    painter.setOpacity(opacity);
    QRadialGradient gradient(center, radius);
    gradient.setColorAt(0.0, Qt::white);
    gradient.setColorAt(0.75, Qt::white);
    gradient.setColorAt(1.0, Qt::transparent);
    painter.setBrush(QBrush(gradient));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(center, radius, radius);
}
void BrushEngine::feather(double radius)
{
    if (m_mask.isNull() || radius < 1.0) return;
    const int kernelSize = qMax(3, (static_cast<int>(radius * 6)) | 1);
    const double sigma   = radius;
    const int halfSize   = kernelSize / 2;
    QVector<double> kernel(kernelSize);
    double total = 0.0;
    for (int i = 0; i < kernelSize; ++i) {
        const double x = i - halfSize;
        kernel[i] = std::exp(-(x * x) / (2.0 * sigma * sigma));
        total += kernel[i];
    }
    for (int i = 0; i < kernelSize; ++i) kernel[i] /= total;
    QImage tmp(m_mask.size(), QImage::Format_ARGB32);
    tmp.fill(Qt::transparent);
    for (int y = 0; y < m_mask.height(); ++y) {
        const auto* src = reinterpret_cast<const QRgb*>(m_mask.constScanLine(y));
        auto*       dst = reinterpret_cast<QRgb*>(tmp.scanLine(y));
        for (int x = 0; x < m_mask.width(); ++x) {
            double r = 0, g = 0, b = 0, a = 0;
            for (int k = 0; k < kernelSize; ++k) {
                const int sx = qBound(0, x + k - halfSize, m_mask.width() - 1);
                const QRgb p = src[sx];
                r += qRed(p)   * kernel[k];
                g += qGreen(p) * kernel[k];
                b += qBlue(p)  * kernel[k];
                a += qAlpha(p) * kernel[k];
            }
            dst[x] = qRgba(int(r), int(g), int(b), int(a));
        }
    }
    for (int x = 0; x < m_mask.width(); ++x) {
        for (int y = 0; y < m_mask.height(); ++y) {
            double r = 0, g = 0, b = 0, a = 0;
            for (int k = 0; k < kernelSize; ++k) {
                const int sy = qBound(0, y + k - halfSize, m_mask.height() - 1);
                const QRgb p = reinterpret_cast<const QRgb*>(tmp.constScanLine(sy))[x];
                r += qRed(p)   * kernel[k];
                g += qGreen(p) * kernel[k];
                b += qBlue(p)  * kernel[k];
                a += qAlpha(p) * kernel[k];
            }
            reinterpret_cast<QRgb*>(m_mask.scanLine(y))[x] = qRgba(int(r), int(g), int(b), int(a));
        }
    }
}
void BrushEngine::clear()
{
    m_mask.fill(Qt::transparent);
}
void BrushEngine::resize(QSize size)
{
    if (m_mask.size() == size) return;
    QImage next(size, QImage::Format_ARGB32);
    next.fill(Qt::transparent);
    QPainter p(&next);
    p.drawImage(QRect(QPoint(0,0), size),
                m_mask.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    m_mask = next;
}
const QImage& BrushEngine::mask() const { return m_mask; }
QImage&       BrushEngine::mask()       { return m_mask; }
} // namespace lumen
```


---

## TASK 4 — REPLACE `core/shared-types/Layer.hpp`

```cpp
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
```


---

## TASK 5 — REPLACE `core/shared-types/Adjustment.hpp`

```cpp
#pragma once
#include <QJsonObject>
#include <QString>
namespace lumen {
enum class AdjustmentType {
    // Tone
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
```


---

## TASK 6 — CREATE `core/image-core/BlendModes.hpp`

```cpp
#pragma once
#include "shared-types/Layer.hpp"
#include <cmath>
#include <algorithm>
namespace lumen::blend {
inline double normal    (double,    double b) { return b; }
inline double multiply  (double a,  double b) { return a * b; }
inline double screen    (double a,  double b) { return 1.0 - (1.0 - a) * (1.0 - b); }
inline double overlay   (double a,  double b) { return a < 0.5 ? 2*a*b : 1-2*(1-a)*(1-b); }
inline double softLight (double a,  double b) {
    if (b <= 0.5) return a - (1 - 2*b) * a * (1 - a);
    double d = (a <= 0.25) ? ((16*a - 12)*a + 4)*a : std::sqrt(a);
    return a + (2*b - 1) * (d - a);
}
inline double hardLight (double a,  double b) { return overlay(b, a); }
inline double difference(double a,  double b) { return std::abs(a - b); }
inline double compose(double base, double blend, double opacity, BlendMode mode)
{
    double blended;
    switch (mode) {
        case BlendMode::Multiply:   blended = multiply(base, blend);   break;
        case BlendMode::Screen:     blended = screen(base, blend);     break;
        case BlendMode::Overlay:    blended = overlay(base, blend);    break;
        case BlendMode::SoftLight:  blended = softLight(base, blend);  break;
        case BlendMode::HardLight:  blended = hardLight(base, blend);  break;
        case BlendMode::Difference: blended = difference(base, blend); break;
        default:                    blended = normal(base, blend);     break;
    }
    return base * (1.0 - opacity) + blended * opacity;
}
} // namespace lumen::blend
```


---

## TASK 7 — CREATE `core/image-core/RawImporter.hpp`

```cpp
#pragma once
#include <QImage>
#include <QString>
#include <QStringList>
namespace lumen {
class RawImporter {
public:
    [[nodiscard]] static QStringList supportedExtensions();
    [[nodiscard]] QImage load(const QString& path);
};
} // namespace lumen
```


---

## TASK 8 — CREATE `core/image-core/RawImporter.cpp`

```cpp
#include "image-core/RawImporter.hpp"
#ifdef HAVE_LIBRAW
#  include <libraw/libraw.h>
#endif
namespace lumen {
QStringList RawImporter::supportedExtensions()
{
    return {"cr2","cr3","nef","arw","dng","raf","orf","rw2","pef","srw","nrw"};
}
QImage RawImporter::load(const QString& path)
{
#ifdef HAVE_LIBRAW
    libraw_data_t* raw = libraw_init(0);
    if (!raw) return {};
    if (libraw_open_file(raw, path.toLocal8Bit().constData()) != LIBRAW_SUCCESS) {
        libraw_close(raw);
        return {};
    }
    raw->params.use_camera_wb  = 1;
    raw->params.output_bps     = 16;
    raw->params.no_auto_bright = 1;
    raw->params.gamm[0]        = 1.0;
    raw->params.gamm[1]        = 1.0;
    raw->params.output_color   = 1; // sRGB
    if (libraw_unpack(raw) != LIBRAW_SUCCESS ||
        libraw_dcraw_process(raw) != LIBRAW_SUCCESS) {
        libraw_close(raw);
        return {};
    }
    int errCode = 0;
    libraw_processed_image_t* img = libraw_dcraw_make_mem_image(raw, &errCode);
    if (!img || errCode != LIBRAW_SUCCESS) {
        libraw_close(raw);
        return {};
    }
    const int W = img->width;
    const int H = img->height;
    QImage result(W, H, QImage::Format_RGBA64);
    const quint16* src = reinterpret_cast<const quint16*>(img->data);
    for (int y = 0; y < H; ++y) {
        auto* dst = reinterpret_cast<QRgba64*>(result.scanLine(y));
        for (int x = 0; x < W; ++x) {
            const quint16 r = src[(y * W + x) * 3 + 0];
            const quint16 g = src[(y * W + x) * 3 + 1];
            const quint16 b = src[(y * W + x) * 3 + 2];
            dst[x] = QRgba64::fromRgba64(r, g, b, 65535);
        }
    }
    libraw_dcraw_clear_mem(img);
    libraw_close(raw);
    return result;
#else
    Q_UNUSED(path)
    return {};
#endif
}
} // namespace lumen
```


---

## TASK 9 — CREATE `core/image-core/ColorManager.hpp`

```cpp
#pragma once
#include <QImage>
#include <QString>
namespace lumen {
class ColorManager {
public:
    ColorManager();
    \~ColorManager();
    bool loadProfiles(const QString& inputProfile, const QString& outputProfile);
    [[nodiscard]] bool hasTransform() const;
    void applyTransform(QImage& image) const;
private:
    void* m_inputProfile  = nullptr;
    void* m_outputProfile = nullptr;
    void* m_transform     = nullptr;
};
} // namespace lumen
```


---

## TASK 10 — CREATE `core/image-core/ColorManager.cpp`

```cpp
#include "image-core/ColorManager.hpp"
#ifdef HAVE_LCMS2
#  include <lcms2.h>
#endif
namespace lumen {
ColorManager::ColorManager()  = default;
ColorManager::\~ColorManager()
{
#ifdef HAVE_LCMS2
    if (m_transform)     cmsDeleteTransform(static_cast<cmsHTRANSFORM>(m_transform));
    if (m_inputProfile)  cmsCloseProfile(static_cast<cmsHPROFILE>(m_inputProfile));
    if (m_outputProfile) cmsCloseProfile(static_cast<cmsHPROFILE>(m_outputProfile));
#endif
}
bool ColorManager::loadProfiles(const QString& inputProfile, const QString& outputProfile)
{
#ifdef HAVE_LCMS2
    if (m_transform)     cmsDeleteTransform(static_cast<cmsHTRANSFORM>(m_transform));
    if (m_inputProfile)  cmsCloseProfile(static_cast<cmsHPROFILE>(m_inputProfile));
    if (m_outputProfile) cmsCloseProfile(static_cast<cmsHPROFILE>(m_outputProfile));
    m_transform = m_inputProfile = m_outputProfile = nullptr;
    cmsHPROFILE in  = cmsOpenProfileFromFile(inputProfile.toLocal8Bit().constData(),  "r");
    cmsHPROFILE out = cmsOpenProfileFromFile(outputProfile.toLocal8Bit().constData(), "r");
    if (!in || !out) {
        if (in)  cmsCloseProfile(in);
        if (out) cmsCloseProfile(out);
        return false;
    }
    cmsHTRANSFORM xf = cmsCreateTransform(
        in,  TYPE_RGBA_16,
        out, TYPE_RGBA_16,
        INTENT_PERCEPTUAL, 0);
    if (!xf) {
        cmsCloseProfile(in);
        cmsCloseProfile(out);
        return false;
    }
    m_inputProfile  = in;
    m_outputProfile = out;
    m_transform     = xf;
    return true;
#else
    Q_UNUSED(inputProfile) Q_UNUSED(outputProfile)
    return false;
#endif
}
bool ColorManager::hasTransform() const { return m_transform != nullptr; }
void ColorManager::applyTransform(QImage& image) const
{
#ifdef HAVE_LCMS2
    if (!m_transform) return;
    image = image.convertToFormat(QImage::Format_RGBA64);
    for (int y = 0; y < image.height(); ++y) {
        void* line = image.scanLine(y);
        cmsDoTransformLineStride(
            static_cast<cmsHTRANSFORM>(m_transform),
            line, line, image.width(), 1,
            image.bytesPerLine(), image.bytesPerLine(), 0, 0);
    }
#else
    Q_UNUSED(image)
#endif
}
} // namespace lumen
```
