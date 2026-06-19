Looking at every file in the codebase against the full milestone plan, here is the complete, ordered task list. Each task is self-contained. Run them in sequence — dependencies are respected by ordering.

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

---

## TASK 11 — CREATE `core/ai-core/OnnxSession.hpp`

```cpp
#pragma once
#include <QImage>
#include <QString>
#include <vector>
#include <cstdint>
#include <memory>
namespace lumen {
class OnnxSession {
public:
    explicit OnnxSession(const QString& modelPath);
    \~OnnxSession();
    [[nodiscard]] bool isLoaded() const;
    bool ensureLoaded();
    std::vector<float> run(
        const std::vector<std::vector<float>>&    inputData,
        const std::vector<std::vector<int64_t>>&  inputShapes,
        const std::vector<const char*>&           inputNames,
        const std::vector<const char*>&           outputNames);
private:
    struct Impl;
    std::unique_ptr<Impl> d;
    QString m_modelPath;
    bool    m_loaded = false;
};
} // namespace lumen
```

---

## TASK 12 — CREATE `core/ai-core/OnnxSession.cpp`

```cpp
#include "ai-core/OnnxSession.hpp"
#ifdef HAVE_ONNXRUNTIME
#  include <onnxruntime_cxx_api.h>
#endif
namespace lumen {
struct OnnxSession::Impl {
#ifdef HAVE_ONNXRUNTIME
    Ort::Env            env{ ORT_LOGGING_LEVEL_WARNING, "lumen" };
    Ort::SessionOptions opts;
    std::unique_ptr<Ort::Session> session;
    Ort::AllocatorWithDefaultOptions allocator;
#endif
};
OnnxSession::OnnxSession(const QString& modelPath)
    : d(std::make_unique<Impl>()), m_modelPath(modelPath)
{}
OnnxSession::\~OnnxSession() = default;
bool OnnxSession::isLoaded() const { return m_loaded; }
bool OnnxSession::ensureLoaded()
{
    if (m_loaded) return true;
#ifdef HAVE_ONNXRUNTIME
    try {
        d->opts.SetIntraOpNumThreads(4);
        d->opts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
#ifdef _WIN32
        d->session = std::make_unique<Ort::Session>(
            d->env,
            m_modelPath.toStdWString().c_str(),
            d->opts);
#else
        d->session = std::make_unique<Ort::Session>(
            d->env,
            m_modelPath.toLocal8Bit().constData(),
            d->opts);
#endif
        m_loaded = true;
    } catch (...) {
        return false;
    }
    return true;
#else
    return false;
#endif
}
std::vector<float> OnnxSession::run(
    const std::vector<std::vector<float>>&   inputData,
    const std::vector<std::vector<int64_t>>& inputShapes,
    const std::vector<const char*>&          inputNames,
    const std::vector<const char*>&          outputNames)
{
    if (!ensureLoaded()) return {};
#ifdef HAVE_ONNXRUNTIME
    auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<Ort::Value> inputs;
    for (size_t i = 0; i < inputData.size(); ++i) {
        inputs.push_back(Ort::Value::CreateTensor<float>(
            memInfo,
            const_cast<float*>(inputData[i].data()),
            inputData[i].size(),
            inputShapes[i].data(),
            inputShapes[i].size()));
    }
    try {
        auto outputs = d->session->Run(
            Ort::RunOptions{nullptr},
            inputNames.data(), inputs.data(), inputs.size(),
            outputNames.data(), outputNames.size());
        if (outputs.empty()) return {};
        const float* ptr   = outputs[0].GetTensorData<float>();
        const size_t count = outputs[0].GetTensorTypeAndShapeInfo().GetElementCount();
        return std::vector<float>(ptr, ptr + count);
    } catch (...) {
        return {};
    }
#else
    return {};
#endif
}
} // namespace lumen
```

---

## TASK 13 — CREATE `core/ai-core/MaskPredictor.hpp`

```cpp
#pragma once
#include "ai-core/OnnxSession.hpp"
#include <QImage>
#include <QPointF>
namespace lumen {
class MaskPredictor {
public:
    MaskPredictor();
    [[nodiscard]] QImage predict(const QImage& source, QPointF promptPoint);
private:
    OnnxSession m_session;
    static constexpr int MODEL_SIZE = 1024;
};
} // namespace lumen
```

---

## TASK 14 — CREATE `core/ai-core/MaskPredictor.cpp`

```cpp
#include "ai-core/MaskPredictor.hpp"
#include <QImage>
#include <algorithm>
#include <cmath>
namespace lumen {
MaskPredictor::MaskPredictor()
    : m_session("models/mobile_sam.onnx")
{}
QImage MaskPredictor::predict(const QImage& source, QPointF promptPoint)
{
    if (source.isNull()) return {};
    const int W = source.width();
    const int H = source.height();
    // Normalize prompt to model space
    const float px = static_cast<float>(promptPoint.x() / W * MODEL_SIZE);
    const float py = static_cast<float>(promptPoint.y() / H * MODEL_SIZE);
    // Preprocess image → [1,3,1024,1024] float32 CHW, ImageNet normalized
    QImage resized = source
        .scaled(MODEL_SIZE, MODEL_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_RGB888);
    const float mean[3] = {0.485f, 0.456f, 0.406f};
    const float std_[3] = {0.229f, 0.224f, 0.225f};
    std::vector<float> imgTensor(3 * MODEL_SIZE * MODEL_SIZE);
    for (int y = 0; y < MODEL_SIZE; ++y) {
        const uchar* row = resized.constScanLine(y);
        for (int x = 0; x < MODEL_SIZE; ++x) {
            for (int c = 0; c < 3; ++c) {
                imgTensor[c * MODEL_SIZE * MODEL_SIZE + y * MODEL_SIZE + x] =
                    (row[x * 3 + c] / 255.0f - mean[c]) / std_[c];
            }
        }
    }
    // Point prompt tensors: coords [1,1,2], labels [1,1]
    std::vector<float> pointCoords = {px, py};
    std::vector<float> pointLabels = {1.0f}; // foreground point
    // Mask input (empty), has_mask_input
    std::vector<float> maskInput(256 * 256, 0.0f);
    std::vector<float> hasMaskInput = {0.0f};
    std::vector<float> origSize = {static_cast<float>(H), static_cast<float>(W)};
    const std::vector<std::vector<float>> inputs = {
        imgTensor, pointCoords, pointLabels, maskInput, hasMaskInput, origSize
    };
    const std::vector<std::vector<int64_t>> shapes = {
        {1, 3, MODEL_SIZE, MODEL_SIZE},
        {1, 1, 2},
        {1, 1},
        {1, 1, 256, 256},
        {1},
        {2}
    };
    const std::vector<const char*> inputNames  = {
        "image", "point_coords", "point_labels",
        "mask_input", "has_mask_input", "orig_im_size"
    };
    const std::vector<const char*> outputNames = {"masks", "iou_predictions"};
    const std::vector<float> output = m_session.run(inputs, shapes, inputNames, outputNames);
    if (output.empty()) return {};
    // Output masks: [1,4,H,W] — pick the mask with highest IOU (first)
    // For simplicity: threshold at 0 → binary mask at source resolution
    QImage mask(W, H, QImage::Format_ARGB32);
    mask.fill(Qt::transparent);
    const int outH = H, outW = W;
    const int maskPx = static_cast<int>(output.size() / 4);
    const int mH = static_cast<int>(std::sqrt(static_cast<double>(maskPx)));
    const int mW = mH;
    // Scale from model output to source resolution
    for (int y = 0; y < H; ++y) {
        auto* dst = reinterpret_cast<QRgb*>(mask.scanLine(y));
        for (int x = 0; x < W; ++x) {
            const int my = qBound(0, y * mH / H, mH - 1);
            const int mx = qBound(0, x * mW / W, mW - 1);
            const float val = output[my * mW + mx];
            dst[x] = val > 0.0f ? qRgba(255, 255, 255, 255) : qRgba(0, 0, 0, 0);
        }
    }
    return mask;
}
} // namespace lumen
```

---

## TASK 15 — CREATE `core/ai-core/InpaintEngine.hpp`

```cpp
#pragma once
#include "ai-core/OnnxSession.hpp"
#include <QImage>
namespace lumen {
class InpaintEngine {
public:
    InpaintEngine();
    [[nodiscard]] QImage inpaint(const QImage& source, const QImage& mask);
private:
    OnnxSession m_session;
    static constexpr int MODEL_SIZE = 512;
    std::vector<float> imageToTensor(const QImage& img) const;
    std::vector<float> maskToTensor(const QImage& mask) const;
    QImage tensorToImage(const std::vector<float>& t, int w, int h) const;
};
} // namespace lumen
```

---

## TASK 16 — CREATE `core/ai-core/InpaintEngine.cpp`

```cpp
#include "ai-core/InpaintEngine.hpp"
#include <algorithm>
#include <cmath>
namespace lumen {
InpaintEngine::InpaintEngine()
    : m_session("models/big-lama.onnx")
{}
std::vector<float> InpaintEngine::imageToTensor(const QImage& img) const
{
    QImage rgb = img.scaled(MODEL_SIZE, MODEL_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                     .convertToFormat(QImage::Format_RGB888);
    std::vector<float> t(3 * MODEL_SIZE * MODEL_SIZE);
    for (int y = 0; y < MODEL_SIZE; ++y) {
        const uchar* row = rgb.constScanLine(y);
        for (int x = 0; x < MODEL_SIZE; ++x) {
            t[0 * MODEL_SIZE * MODEL_SIZE + y * MODEL_SIZE + x] = row[x*3+0] / 255.0f;
            t[1 * MODEL_SIZE * MODEL_SIZE + y * MODEL_SIZE + x] = row[x*3+1] / 255.0f;
            t[2 * MODEL_SIZE * MODEL_SIZE + y * MODEL_SIZE + x] = row[x*3+2] / 255.0f;
        }
    }
    return t;
}
std::vector<float> InpaintEngine::maskToTensor(const QImage& mask) const
{
    QImage m = mask.scaled(MODEL_SIZE, MODEL_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                   .convertToFormat(QImage::Format_ARGB32);
    std::vector<float> t(MODEL_SIZE * MODEL_SIZE);
    for (int y = 0; y < MODEL_SIZE; ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(m.constScanLine(y));
        for (int x = 0; x < MODEL_SIZE; ++x) {
            t[y * MODEL_SIZE + x] = qAlpha(row[x]) > 127 ? 1.0f : 0.0f;
        }
    }
    return t;
}
QImage InpaintEngine::tensorToImage(const std::vector<float>& t, int w, int h) const
{
    QImage img(MODEL_SIZE, MODEL_SIZE, QImage::Format_RGB888);
    for (int y = 0; y < MODEL_SIZE; ++y) {
        uchar* row = img.scanLine(y);
        for (int x = 0; x < MODEL_SIZE; ++x) {
            row[x*3+0] = static_cast<uchar>(qBound(0.0f, t[0*MODEL_SIZE*MODEL_SIZE+y*MODEL_SIZE+x]*255, 255.0f));
            row[x*3+1] = static_cast<uchar>(qBound(0.0f, t[1*MODEL_SIZE*MODEL_SIZE+y*MODEL_SIZE+x]*255, 255.0f));
            row[x*3+2] = static_cast<uchar>(qBound(0.0f, t[2*MODEL_SIZE*MODEL_SIZE+y*MODEL_SIZE+x]*255, 255.0f));
        }
    }
    return img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
              .convertToFormat(QImage::Format_RGBA64);
}
QImage InpaintEngine::inpaint(const QImage& source, const QImage& mask)
{
    if (source.isNull() || mask.isNull()) return source;
    const auto imgT  = imageToTensor(source);
    const auto maskT = maskToTensor(mask);
    const std::vector<const char*> inputNames  = {"image", "mask"};
    const std::vector<const char*> outputNames = {"inpainted"};
    const std::vector<std::vector<float>>    inputs = {imgT, maskT};
    const std::vector<std::vector<int64_t>>  shapes = {
        {1, 3, MODEL_SIZE, MODEL_SIZE},
        {1, 1, MODEL_SIZE, MODEL_SIZE}
    };
    const auto output = m_session.run(inputs, shapes, inputNames, outputNames);
    if (output.empty()) return source;
    QImage result = tensorToImage(output, source.width(), source.height());
    // Composite: paste inpainted region over original using mask
    QImage final = source.convertToFormat(QImage::Format_RGBA64);
    QImage scaledMask = mask.scaled(source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < final.height(); ++y) {
        auto*       dst = reinterpret_cast<QRgba64*>(final.scanLine(y));
        const auto* src = reinterpret_cast<const QRgba64*>(result.constScanLine(y));
        const auto* msk = reinterpret_cast<const QRgb*>(scaledMask.constScanLine(y));
        for (int x = 0; x < final.width(); ++x) {
            const double a = qAlpha(msk[x]) / 255.0;
            if (a > 0.5) dst[x] = src[x];
        }
    }
    return final;
}
} // namespace lumen
```

---

## TASK 17 — CREATE `core/ai-core/UpscaleEngine.hpp`

```cpp
#pragma once
#include "ai-core/OnnxSession.hpp"
#include <QImage>
namespace lumen {
class UpscaleEngine {
public:
    UpscaleEngine();
    [[nodiscard]] QImage upscale(const QImage& source);
private:
    OnnxSession m_session;
    static constexpr int TILE    = 512;
    static constexpr int OVERLAP = 32;
    static constexpr int SCALE   = 4;
    QImage runTile(const QImage& tile);
    void blendTile(QImage& canvas, const QImage& tile, int dstX, int dstY, int overlap);
};
} // namespace lumen
```

---

## TASK 18 — CREATE `core/ai-core/UpscaleEngine.cpp`

```cpp
#include "ai-core/UpscaleEngine.hpp"
#include <algorithm>
namespace lumen {
UpscaleEngine::UpscaleEngine()
    : m_session("models/realesrgan-x4plus.onnx")
{}
QImage UpscaleEngine::runTile(const QImage& tile)
{
    const int H = tile.height(), W = tile.width();
    QImage rgb = tile.convertToFormat(QImage::Format_RGB888);
    std::vector<float> input(3 * H * W);
    for (int y = 0; y < H; ++y) {
        const uchar* row = rgb.constScanLine(y);
        for (int x = 0; x < W; ++x) {
            input[0*H*W + y*W + x] = row[x*3+0] / 255.0f;
            input[1*H*W + y*W + x] = row[x*3+1] / 255.0f;
            input[2*H*W + y*W + x] = row[x*3+2] / 255.0f;
        }
    }
    const std::vector<std::vector<float>>   inputs = {input};
    const std::vector<std::vector<int64_t>> shapes = {{1,3,H,W}};
    const std::vector<const char*> inNames  = {"input"};
    const std::vector<const char*> outNames = {"output"};
    const auto out = m_session.run(inputs, shapes, inNames, outNames);
    if (out.empty()) return tile.scaled(W*SCALE, H*SCALE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    const int OH = H * SCALE, OW = W * SCALE;
    QImage result(OW, OH, QImage::Format_RGB888);
    for (int y = 0; y < OH; ++y) {
        uchar* row = result.scanLine(y);
        for (int x = 0; x < OW; ++x) {
            row[x*3+0] = static_cast<uchar>(qBound(0.0f, out[0*OH*OW+y*OW+x]*255, 255.0f));
            row[x*3+1] = static_cast<uchar>(qBound(0.0f, out[1*OH*OW+y*OW+x]*255, 255.0f));
            row[x*3+2] = static_cast<uchar>(qBound(0.0f, out[2*OH*OW+y*OW+x]*255, 255.0f));
        }
    }
    return result;
}
void UpscaleEngine::blendTile(QImage& canvas, const QImage& tile, int dstX, int dstY, int overlap)
{
    QPainter p(&canvas);
    p.drawImage(QPoint(dstX, dstY), tile);
}
QImage UpscaleEngine::upscale(const QImage& source)
{
    if (source.isNull()) return {};
    const int W = source.width(), H = source.height();
    QImage result(W * SCALE, H * SCALE, QImage::Format_RGB888);
    result.fill(Qt::black);
    for (int ty = 0; ty < H; ty += TILE - OVERLAP) {
        for (int tx = 0; tx < W; tx += TILE - OVERLAP) {
            const int tw = qMin(TILE, W - tx);
            const int th = qMin(TILE, H - ty);
            const QImage tile = source.copy(tx, ty, tw, th);
            const QImage up   = runTile(tile);
            blendTile(result, up, tx * SCALE, ty * SCALE, OVERLAP * SCALE);
        }
    }
    return result.convertToFormat(QImage::Format_RGBA64);
}
} // namespace lumen
```

---

## TASK 19 — REPLACE `core/ai-core/AiRuntime.hpp`

```cpp
#pragma once
#include <QObject>
#include <QString>
#include <QImage>
#include <QPointF>
#include <functional>
namespace lumen {
enum class AiBackend { Cpu, DirectMl, Cuda, CoreMl };
class AiRuntime final : public QObject {
    Q_OBJECT
public:
    explicit AiRuntime(QObject* parent = nullptr);
    [[nodiscard]] bool isModelAvailable(const QString& modelId) const;
    [[nodiscard]] AiBackend activeBackend() const;
    void predictMask(const QImage& source, QPointF point,
                     std::function<void(QImage)> callback);
signals:
    void progressChanged(QString jobId, double progress);
    void busyChanged(bool busy);
private:
    AiBackend m_backend = AiBackend::Cpu;
};
} // namespace lumen
```

---

## TASK 20 — REPLACE `core/ai-core/AiRuntime.cpp`

```cpp
#include "ai-core/AiRuntime.hpp"
#include "ai-core/MaskPredictor.hpp"
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>
namespace lumen {
AiRuntime::AiRuntime(QObject* parent)
    : QObject(parent)
{}
bool AiRuntime::isModelAvailable(const QString& modelId) const
{
    return QFileInfo::exists(modelId);
}
AiBackend AiRuntime::activeBackend() const { return m_backend; }
void AiRuntime::predictMask(const QImage& source,
                             QPointF point,
                             std::function<void(QImage)> callback)
{
    emit busyChanged(true);
    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this,
        [this, watcher, cb = std::move(callback)]() mutable {
            cb(watcher->result());
            emit busyChanged(false);
            watcher->deleteLater();
        });
    watcher->setFuture(QtConcurrent::run([src = source, pt = point]() -> QImage {
        MaskPredictor predictor;
        return predictor.predict(src, pt);
    }));
}
} // namespace lumen
```

---

## TASK 21 — REPLACE `core/editor-core/DocumentModel.hpp`

```cpp
#pragma once
#include "shared-types/Adjustment.hpp"
#include "shared-types/Layer.hpp"
#include "shared-types/Mask.hpp"
#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>
namespace lumen {
class DocumentModel final : public QObject {
    Q_OBJECT
public:
    explicit DocumentModel(QObject* parent = nullptr);
    bool openSourceImage(const QString& path);
    void replaceSourceImage(const QImage& newImage);
    void clear();
    [[nodiscard]] bool    hasDocument()  const;
    [[nodiscard]] QString sourcePath()   const;
    [[nodiscard]] QSize   sourceSize()   const;
    [[nodiscard]] const QImage& sourceImage() const;
    [[nodiscard]] QVector<Adjustment> adjustments()             const;
    [[nodiscard]] QVector<Adjustment> adjustmentsForLayer(const QString& layerId) const;
    [[nodiscard]] QVector<Layer>      layers()                  const;
    [[nodiscard]] QVector<Mask>       masks()                   const;
    [[nodiscard]] QImage              layerImage(const QString& layerId) const;
    [[nodiscard]] bool    canUndo()      const;
    [[nodiscard]] bool    canRedo()      const;
    [[nodiscard]] bool    isDownsampled() const;
    void setActiveMask(const QImage& mask);
    [[nodiscard]] const QImage& activeMask() const;
    void setScalarAdjustment(AdjustmentType type, double value);
    [[nodiscard]] double scalarAdjustment(AdjustmentType type) const;
    void rotateClockwise();
    void rotateCounterClockwise();
    void flipHorizontal();
    void flipVertical();
    void undo();
    void redo();
    // Layer management (M8)
    void addImageLayer(const QString& path);
    void moveLayer(int fromIndex, int toIndex);
    void setLayerOpacity(const QString& id, double opacity);
    void setLayerVisible(const QString& id, bool visible);
    void setLayerBlendMode(const QString& id, BlendMode mode);
    void deleteLayer(const QString& id);
signals:
    void changed();
    void historyChanged();
private:
    Adjustment*       findAdjustment(AdjustmentType type);
    const Adjustment* findAdjustment(AdjustmentType type) const;
    Layer*            findLayer(const QString& id);
    void pushHistorySnapshot();
    void restoreAdjustments(const QVector<Adjustment>& adjustments);
    QString   m_projectId;
    QString   m_sourcePath;
    QImage    m_sourceImage;
    bool      m_isDownsampled = false;
    QVector<Layer>      m_layers;
    QVector<Mask>       m_masks;
    QVector<Adjustment> m_adjustments;
    QHash<QString, QImage> m_layerImages;
    QVector<QVector<Adjustment>>  m_undoStack;
    QVector<QVector<Adjustment>>  m_redoStack;
    QVector<QImage>               m_sourceImageHistory; // for replaceSourceImage undo
};
} // namespace lumen
```

---

## TASK 22 — REPLACE `core/editor-core/DocumentModel.cpp`

```cpp
#include "editor-core/DocumentModel.hpp"
#include "image-core/RawImporter.hpp"
#include <QFileInfo>
#include <QUuid>
namespace lumen {
namespace {
QString makeId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
} // namespace
QString adjustmentTypeToString(AdjustmentType type)
{
    switch (type) {
    case AdjustmentType::Exposure:       return "exposure";
    case AdjustmentType::Contrast:       return "contrast";
    case AdjustmentType::Highlights:     return "highlights";
    case AdjustmentType::Shadows:        return "shadows";
    case AdjustmentType::Whites:         return "whites";
    case AdjustmentType::Blacks:         return "blacks";
    case AdjustmentType::Saturation:     return "saturation";
    case AdjustmentType::Vibrance:       return "vibrance";
    case AdjustmentType::Temperature:    return "temperature";
    case AdjustmentType::Tint:           return "tint";
    case AdjustmentType::RotationDegrees:return "rotationDegrees";
    case AdjustmentType::FlipHorizontal: return "flipHorizontal";
    case AdjustmentType::FlipVertical:   return "flipVertical";
    case AdjustmentType::ToneCurveLuma:  return "toneCurveLuma";
    case AdjustmentType::ToneCurveR:     return "toneCurveR";
    case AdjustmentType::ToneCurveG:     return "toneCurveG";
    case AdjustmentType::ToneCurveB:     return "toneCurveB";
    case AdjustmentType::NoiseReduction: return "noiseReduction";
    case AdjustmentType::Sharpening:     return "sharpening";
    default:                             return "exposure";
    }
}
AdjustmentType adjustmentTypeFromString(const QString& v)
{
    if (v == "contrast")        return AdjustmentType::Contrast;
    if (v == "highlights")      return AdjustmentType::Highlights;
    if (v == "shadows")         return AdjustmentType::Shadows;
    if (v == "whites")          return AdjustmentType::Whites;
    if (v == "blacks")          return AdjustmentType::Blacks;
    if (v == "saturation")      return AdjustmentType::Saturation;
    if (v == "vibrance")        return AdjustmentType::Vibrance;
    if (v == "temperature")     return AdjustmentType::Temperature;
    if (v == "tint")            return AdjustmentType::Tint;
    if (v == "rotationDegrees") return AdjustmentType::RotationDegrees;
    if (v == "flipHorizontal")  return AdjustmentType::FlipHorizontal;
    if (v == "flipVertical")    return AdjustmentType::FlipVertical;
    if (v == "toneCurveLuma")   return AdjustmentType::ToneCurveLuma;
    if (v == "toneCurveR")      return AdjustmentType::ToneCurveR;
    if (v == "toneCurveG")      return AdjustmentType::ToneCurveG;
    if (v == "toneCurveB")      return AdjustmentType::ToneCurveB;
    if (v == "noiseReduction")  return AdjustmentType::NoiseReduction;
    if (v == "sharpening")      return AdjustmentType::Sharpening;
    return AdjustmentType::Exposure;
}
DocumentModel::DocumentModel(QObject* parent) : QObject(parent) {}
bool DocumentModel::openSourceImage(const QString& path)
{
    QImage image;
    const QString ext = QFileInfo(path).suffix().toLower();
    if (RawImporter::supportedExtensions().contains(ext)) {
        RawImporter importer;
        image = importer.load(path);
    }
    if (image.isNull()) {
        image.load(path);
    }
    if (image.isNull()) return false;
    clear();
    m_projectId  = makeId();
    m_sourcePath = QFileInfo(path).absoluteFilePath();
    constexpr int MAX_MP = 100'000'000;
    if (image.width() * image.height() > MAX_MP) {
        m_isDownsampled = true;
        const double scale = std::sqrt(static_cast<double>(MAX_MP) /
                                       (image.width() * image.height()));
        m_sourceImage = image
            .scaled(image.size() * scale, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_RGBA64);
    } else {
        m_isDownsampled = false;
        m_sourceImage   = image.convertToFormat(QImage::Format_RGBA64);
    }
    Layer base;
    base.id           = makeId();
    base.name         = QFileInfo(path).completeBaseName();
    base.kind         = LayerKind::Image;
    base.sourceAssetId= m_projectId;
    m_layers.push_back(base);
    m_layerImages[base.id] = m_sourceImage;
    emit changed();
    return true;
}
void DocumentModel::replaceSourceImage(const QImage& newImage)
{
    if (m_sourceImageHistory.size() >= 5)
        m_sourceImageHistory.removeFirst();
    m_sourceImageHistory.push_back(m_sourceImage);
    m_sourceImage = newImage.convertToFormat(QImage::Format_RGBA64);
    if (!m_layers.isEmpty())
        m_layerImages[m_layers.first().id] = m_sourceImage;
    emit changed();
}
void DocumentModel::clear()
{
    m_projectId.clear(); m_sourcePath.clear();
    m_sourceImage = {}; m_isDownsampled = false;
    m_layers.clear(); m_masks.clear(); m_adjustments.clear();
    m_layerImages.clear(); m_sourceImageHistory.clear();
    m_undoStack.clear(); m_redoStack.clear();
    emit changed(); emit historyChanged();
}
bool    DocumentModel::hasDocument()   const { return !m_sourceImage.isNull(); }
QString DocumentModel::sourcePath()    const { return m_sourcePath; }
QSize   DocumentModel::sourceSize()    const { return m_sourceImage.size(); }
bool    DocumentModel::isDownsampled() const { return m_isDownsampled; }
const QImage& DocumentModel::sourceImage() const { return m_sourceImage; }
QVector<Adjustment> DocumentModel::adjustments() const { return m_adjustments; }
QVector<Layer>      DocumentModel::layers()       const { return m_layers; }
QVector<Mask>       DocumentModel::masks()        const { return m_masks; }
QVector<Adjustment> DocumentModel::adjustmentsForLayer(const QString& layerId) const
{
    QVector<Adjustment> result;
    for (const Adjustment& a : m_adjustments) {
        if (a.targetLayerId == layerId || (layerId.isEmpty() && a.targetLayerId.isEmpty()))
            result.push_back(a);
    }
    return result;
}
QImage DocumentModel::layerImage(const QString& layerId) const
{
    if (m_layerImages.contains(layerId))
        return m_layerImages[layerId];
    return m_sourceImage;
}
const QImage& DocumentModel::activeMask() const
{
    if (!m_masks.isEmpty()) return m_masks.first().mask;
    static const QImage nullMask;
    return nullMask;
}
void DocumentModel::setActiveMask(const QImage& mask)
{
    if (m_masks.isEmpty()) {
        Mask m; m.id = makeId(); m.name = "Active Mask"; m.mask = mask;
        m_masks.push_back(m);
    } else {
        m_masks.first().mask = mask;
    }
    emit changed();
}
bool DocumentModel::canUndo() const { return !m_undoStack.isEmpty(); }
bool DocumentModel::canRedo() const { return !m_redoStack.isEmpty(); }
void DocumentModel::setScalarAdjustment(AdjustmentType type, double value)
{
    if (qFuzzyCompare(scalarAdjustment(type) + 1.0, value + 1.0)) return;
    pushHistorySnapshot();
    Adjustment* adj = findAdjustment(type);
    if (!adj) {
        Adjustment next; next.id = makeId(); next.type = type;
        next.order = m_adjustments.size();
        m_adjustments.push_back(next);
        adj = &m_adjustments.last();
    }
    adj->parameters["value"] = value;
    m_redoStack.clear();
    emit changed(); emit historyChanged();
}
double DocumentModel::scalarAdjustment(AdjustmentType type) const
{
    const Adjustment* a = findAdjustment(type);
    return a ? a->parameters.value("value").toDouble(0.0) : 0.0;
}
void DocumentModel::rotateClockwise()
{ setScalarAdjustment(AdjustmentType::RotationDegrees,
    (int(scalarAdjustment(AdjustmentType::RotationDegrees)) + 90) % 360); }
void DocumentModel::rotateCounterClockwise()
{ setScalarAdjustment(AdjustmentType::RotationDegrees,
    (int(scalarAdjustment(AdjustmentType::RotationDegrees)) + 270) % 360); }
void DocumentModel::flipHorizontal()
{ setScalarAdjustment(AdjustmentType::FlipHorizontal,
    scalarAdjustment(AdjustmentType::FlipHorizontal) > 0.5 ? 0.0 : 1.0); }
void DocumentModel::flipVertical()
{ setScalarAdjustment(AdjustmentType::FlipVertical,
    scalarAdjustment(AdjustmentType::FlipVertical) > 0.5 ? 0.0 : 1.0); }
void DocumentModel::undo()
{
    if (!canUndo()) return;
    m_redoStack.push_back(m_adjustments);
    restoreAdjustments(m_undoStack.takeLast());
}
void DocumentModel::redo()
{
    if (!canRedo()) return;
    m_undoStack.push_back(m_adjustments);
    restoreAdjustments(m_redoStack.takeLast());
}
// Layer management
void DocumentModel::addImageLayer(const QString& path)
{
    QImage img; img.load(path);
    if (img.isNull()) return;
    pushHistorySnapshot();
    Layer layer;
    layer.id    = makeId();
    layer.name  = QFileInfo(path).completeBaseName();
    layer.kind  = LayerKind::Image;
    layer.order = m_layers.size();
    m_layers.push_back(layer);
    m_layerImages[layer.id] = img.scaled(m_sourceImage.size(),
        Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_RGBA64);
    emit changed();
}
void DocumentModel::moveLayer(int from, int to)
{
    if (from < 0 || from >= m_layers.size() || to < 0 || to >= m_layers.size()) return;
    pushHistorySnapshot();
    m_layers.move(from, to);
    for (int i = 0; i < m_layers.size(); ++i) m_layers[i].order = i;
    emit changed();
}
Layer* DocumentModel::findLayer(const QString& id)
{
    for (Layer& l : m_layers) if (l.id == id) return &l;
    return nullptr;
}
void DocumentModel::setLayerOpacity(const QString& id, double opacity)
{
    if (Layer* l = findLayer(id)) { l->opacity = opacity; emit changed(); }
}
void DocumentModel::setLayerVisible(const QString& id, bool visible)
{
    if (Layer* l = findLayer(id)) { l->visible = visible; emit changed(); }
}
void DocumentModel::setLayerBlendMode(const QString& id, BlendMode mode)
{
    if (Layer* l = findLayer(id)) { l->blendMode = mode; emit changed(); }
}
void DocumentModel::deleteLayer(const QString& id)
{
    if (m_layers.size() <= 1) return;
    pushHistorySnapshot();
    m_layers.removeIf([&](const Layer& l){ return l.id == id; });
    m_layerImages.remove(id);
    emit changed();
}
Adjustment* DocumentModel::findAdjustment(AdjustmentType type)
{
    for (Adjustment& a : m_adjustments) if (a.type == type) return &a;
    return nullptr;
}
const Adjustment* DocumentModel::findAdjustment(AdjustmentType type) const
{
    for (const Adjustment& a : m_adjustments) if (a.type == type) return &a;
    return nullptr;
}
void DocumentModel::pushHistorySnapshot()
{
    m_undoStack.push_back(m_adjustments);
    if (m_undoStack.size() > 100) m_undoStack.removeFirst();
}
void DocumentModel::restoreAdjustments(const QVector<Adjustment>& adjs)
{
    m_adjustments = adjs;
    emit changed(); emit historyChanged();
}
} // namespace lumen
```

---

## TASK 23 — REPLACE `core/image-core/RenderPipeline.hpp`

```cpp
#pragma once
#include "editor-core/DocumentModel.hpp"
#include "shared-types/Layer.hpp"
#include <QImage>
#include <QSize>
#include <QVector>
#include <atomic>
#include <memory>
namespace lumen {
class RenderPipeline {
public:
    [[nodiscard]] QImage renderPreview(
        const DocumentModel& document, QSize maximumSize,
        std::shared_ptr<std::atomic<bool>> cancelled = nullptr) const;
    [[nodiscard]] QImage renderPreviewFromData(
        const QImage& source,
        const QVector<Adjustment>& adjustments,
        QSize maximumSize,
        const QImage& mask = {},
        std::shared_ptr<std::atomic<bool>> cancelled = nullptr) const;
    [[nodiscard]] QImage renderFullResolution(const DocumentModel& document) const;
private:
    [[nodiscard]] QImage applyAdjustments(
        QImage image,
        const QVector<Adjustment>& adjustments,
        const QImage& mask = {},
        std::shared_ptr<std::atomic<bool>> cancelled = nullptr) const;
    void blendOnto(QImage& canvas, const QImage& layer,
                   BlendMode mode, double opacity) const;
    static std::array<double, 65536> buildCurveLut(const QJsonArray& points);
};
} // namespace lumen
```

---

## TASK 24 — REPLACE `core/image-core/RenderPipeline.cpp`

```cpp
#include "image-core/RenderPipeline.hpp"
#include "image-core/BlendModes.hpp"
#include "image-core/ColorManager.hpp"
#include <QJsonArray>
#include <QJsonValue>
#include <QTransform>
#include <QtMath>
#include <algorithm>
#ifdef HAVE_OPENCV
#  include <opencv2/core.hpp>
#  include <opencv2/imgproc.hpp>
#  include <opencv2/photo.hpp>
#endif
namespace lumen {
namespace {
quint16 clamp16(double v) { return static_cast<quint16>(qBound(0.0, v, 65535.0)); }
double scalarAdj(const QVector<Adjustment>& adjs, AdjustmentType type)
{
    for (const Adjustment& a : adjs)
        if (a.type == type && a.enabled)
            return a.parameters.value("value").toDouble(0.0);
    return 0.0;
}
const Adjustment* findAdj(const QVector<Adjustment>& adjs, AdjustmentType type)
{
    for (const Adjustment& a : adjs)
        if (a.type == type && a.enabled) return &a;
    return nullptr;
}
} // namespace
std::array<double, 65536> RenderPipeline::buildCurveLut(const QJsonArray& points)
{
    std::array<double, 65536> lut;
    if (points.isEmpty()) {
        for (int i = 0; i < 65536; ++i) lut[i] = i;
        return lut;
    }
    // Parse control points
    QVector<QPair<double,double>> pts;
    pts.append({0.0, 0.0});
    for (const QJsonValue& v : points) {
        const QJsonArray p = v.toArray();
        if (p.size() >= 2)
            pts.append({p[0].toDouble(), p[1].toDouble()});
    }
    pts.append({1.0, 1.0});
    std::sort(pts.begin(), pts.end(), [](auto& a, auto& b){ return a.first < b.first; });
    // Piecewise linear interpolation
    for (int i = 0; i < 65536; ++i) {
        const double x = i / 65535.0;
        double y = x;
        for (int k = 1; k < pts.size(); ++k) {
            if (x <= pts[k].first) {
                const double t = (x - pts[k-1].first) / (pts[k].first - pts[k-1].first + 1e-9);
                y = pts[k-1].second + t * (pts[k].second - pts[k-1].second);
                break;
            }
        }
        lut[i] = qBound(0.0, y * 65535.0, 65535.0);
    }
    return lut;
}
QImage RenderPipeline::renderPreview(const DocumentModel& doc, QSize maxSize,
                                      std::shared_ptr<std::atomic<bool>> cancelled) const
{
    if (!doc.hasDocument()) return {};
    return renderPreviewFromData(doc.sourceImage(), doc.adjustments(),
                                  maxSize, doc.activeMask(), cancelled);
}
QImage RenderPipeline::renderPreviewFromData(
    const QImage& sourceImage,
    const QVector<Adjustment>& adjustments,
    QSize maximumSize,
    const QImage& mask,
    std::shared_ptr<std::atomic<bool>> cancelled) const
{
    if (sourceImage.isNull()) return {};
    QImage source = sourceImage;
    if (maximumSize.isValid() &&
        (source.width() > maximumSize.width() || source.height() > maximumSize.height()))
        source = source.scaled(maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage scaledMask = mask.isNull() ? QImage() :
        mask.scaled(source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return applyAdjustments(source, adjustments, scaledMask, cancelled);
}
QImage RenderPipeline::renderFullResolution(const DocumentModel& doc) const
{
    if (!doc.hasDocument()) return {};
    auto layers = doc.layers();
    if (layers.isEmpty())
        return applyAdjustments(doc.sourceImage(), doc.adjustmentsForLayer(QString()),
                                doc.activeMask());
    std::sort(layers.begin(), layers.end(),
              [](const Layer& a, const Layer& b){ return a.order < b.order; });
    const QSize sz = doc.sourceImage().size();
    QImage canvas(sz, QImage::Format_RGBA64);
    canvas.fill(Qt::transparent);
    const QVector<Adjustment> globalAdjs = doc.adjustmentsForLayer(QString());
    for (const Layer& layer : layers) {
        if (!layer.visible) continue;
        QImage layerImg = doc.layerImage(layer.id);
        if (layerImg.isNull()) continue;
        QVector<Adjustment> adjs = globalAdjs;
        adjs.append(doc.adjustmentsForLayer(layer.id));
        layerImg = applyAdjustments(
            layerImg.convertToFormat(QImage::Format_RGBA64), adjs);
        blendOnto(canvas, layerImg, layer.blendMode, layer.opacity);
    }
    // Apply mask to final composite
    if (!doc.activeMask().isNull()) {
        const QImage scaledMask = doc.activeMask().scaled(
            sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < canvas.height(); ++y) {
            auto* dst = reinterpret_cast<QRgba64*>(canvas.scanLine(y));
            const auto* msk = reinterpret_cast<const QRgb*>(scaledMask.constScanLine(y));
            for (int x = 0; x < canvas.width(); ++x) {
                const double a = qAlpha(msk[x]) / 255.0;
                dst[x] = QRgba64::fromRgba64(
                    dst[x].red(), dst[x].green(), dst[x].blue(),
                    static_cast<quint16>(dst[x].alpha() * a));
            }
        }
    }
    return canvas;
}
void RenderPipeline::blendOnto(QImage& canvas, const QImage& layer,
                                BlendMode mode, double opacity) const
{
    const int W = qMin(canvas.width(),  layer.width());
    const int H = qMin(canvas.height(), layer.height());
    for (int y = 0; y < H; ++y) {
        auto*       dst = reinterpret_cast<QRgba64*>(canvas.scanLine(y));
        const auto* src = reinterpret_cast<const QRgba64*>(layer.constScanLine(y));
        for (int x = 0; x < W; ++x) {
            const double bR = dst[x].red()   / 65535.0, bG = dst[x].green() / 65535.0;
            const double bB = dst[x].blue()  / 65535.0, bA = dst[x].alpha() / 65535.0;
            const double lR = src[x].red()   / 65535.0, lG = src[x].green() / 65535.0;
            const double lB = src[x].blue()  / 65535.0, lA = src[x].alpha() / 65535.0;
            const double eff = opacity * lA;
            const double oR  = blend::compose(bR, lR, eff, mode);
            const double oG  = blend::compose(bG, lG, eff, mode);
            const double oB  = blend::compose(bB, lB, eff, mode);
            const double oA  = bA + lA * opacity * (1.0 - bA);
            dst[x] = QRgba64::fromRgba64(
                clamp16(oR*65535), clamp16(oG*65535),
                clamp16(oB*65535), clamp16(oA*65535));
        }
    }
}
QImage RenderPipeline::applyAdjustments(
    QImage image,
    const QVector<Adjustment>& adjustments,
    const QImage& mask,
    std::shared_ptr<std::atomic<bool>> cancelled) const
{
    image = image.convertToFormat(QImage::Format_RGBA64);
    // Transform ops
    const int  rotation      = int(scalarAdj(adjustments, AdjustmentType::RotationDegrees)) % 360;
    const bool flipH         = scalarAdj(adjustments, AdjustmentType::FlipHorizontal) > 0.5;
    const bool flipV         = scalarAdj(adjustments, AdjustmentType::FlipVertical)   > 0.5;
    if (flipH || flipV) image = image.mirrored(flipH, flipV);
    if (rotation != 0) {
        QTransform t; t.rotate(rotation);
        image = image.transformed(t, Qt::SmoothTransformation);
    }
    // Tone scalars
    const double exposureGain  = qPow(2.0, scalarAdj(adjustments, AdjustmentType::Exposure));
    const double contrastGain  = 1.0 + scalarAdj(adjustments, AdjustmentType::Contrast) / 100.0;
    const double highlights    = scalarAdj(adjustments, AdjustmentType::Highlights) / 100.0;
    const double shadows       = scalarAdj(adjustments, AdjustmentType::Shadows)    / 100.0;
    const double whites        = scalarAdj(adjustments, AdjustmentType::Whites)     / 100.0;
    const double blacks        = scalarAdj(adjustments, AdjustmentType::Blacks)     / 100.0;
    const double satGain       = 1.0 + scalarAdj(adjustments, AdjustmentType::Saturation) / 100.0;
    const double vibrance      = scalarAdj(adjustments, AdjustmentType::Vibrance)   / 100.0;
    const double temperature   = scalarAdj(adjustments, AdjustmentType::Temperature);
    const double tint          = scalarAdj(adjustments, AdjustmentType::Tint);
    const double whitesGain    = 1.0 + whites * 0.5;
    const double blacksOffset  = blacks * 0.15 * 65535.0;
    const double redBalance    = 1.0 + temperature / 400.0;
    const double blueBalance   = 1.0 - temperature / 400.0;
    const double greenBalance  = 1.0 + tint        / 400.0;
    // Tone curve LUTs
    bool hasLumaLut = false, hasRLut = false, hasGLut = false, hasBLut = false;
    std::array<double,65536> lumaLut{}, rLut{}, gLut{}, bLut{};
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveLuma)) {
        lumaLut = buildCurveLut(a->parameters.value("points").toArray()); hasLumaLut = true; }
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveR)) {
        rLut = buildCurveLut(a->parameters.value("points").toArray()); hasRLut = true; }
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveG)) {
        gLut = buildCurveLut(a->parameters.value("points").toArray()); hasGLut = true; }
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveB)) {
        bLut = buildCurveLut(a->parameters.value("points").toArray()); hasBLut = true; }
    // Scaled mask for compositing
    const bool hasMask = !mask.isNull();
    QImage scaledMask = hasMask ?
        mask.scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_ARGB32) : QImage();
    for (int y = 0; y < image.height(); ++y) {
        if (cancelled && *cancelled) return {};
        auto* scanline = reinterpret_cast<QRgba64*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgba64 origPixel = scanline[x];
            double r = origPixel.red()   * exposureGain;
            double g = origPixel.green() * exposureGain;
            double b = origPixel.blue()  * exposureGain;
            r = (r * whitesGain) + blacksOffset;
            g = (g * whitesGain) + blacksOffset;
            b = (b * whitesGain) + blacksOffset;
            r = ((r/65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            g = ((g/65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            b = ((b/65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            double luma = (0.2126*r + 0.7152*g + 0.0722*b) / 65535.0;
            if (luma > 0.5) {
                const double blend = (luma - 0.5) * 2.0;
                const double gain  = 1.0 + highlights * blend;
                r *= gain; g *= gain; b *= gain;
            }
            luma = (0.2126*r + 0.7152*g + 0.0722*b) / 65535.0;
            if (luma < 0.5) {
                const double blend = (0.5 - luma) * 2.0;
                const double gain  = 1.0 + shadows * blend;
                r *= gain; g *= gain; b *= gain;
            }
            const double luma16 = 0.2126*r + 0.7152*g + 0.0722*b;
            r = luma16 + (r - luma16) * satGain;
            g = luma16 + (g - luma16) * satGain;
            b = luma16 + (b - luma16) * satGain;
            const double satMax = qMax(r, qMax(g, b));
            const double satMin = qMin(r, qMin(g, b));
            const double colorfulness = (satMax - satMin) / (satMax + 1e-6);
            const double vibGain = 1.0 + vibrance * (1.0 - colorfulness);
            r = luma16 + (r - luma16) * vibGain;
            g = luma16 + (g - luma16) * vibGain;
            b = luma16 + (b - luma16) * vibGain;
            r *= redBalance; g *= greenBalance; b *= blueBalance;
            // Tone curves
            if (hasRLut) r = rLut[clamp16(r)];
            if (hasGLut) g = gLut[clamp16(g)];
            if (hasBLut) b = bLut[clamp16(b)];
            if (hasLumaLut) {
                const double l2  = 0.2126*r + 0.7152*g + 0.0722*b;
                const double l2m = lumaLut[clamp16(l2)];
                const double scale = l2 > 0 ? l2m / l2 : 1.0;
                r *= scale; g *= scale; b *= scale;
            }
            // Mask compositing
            if (hasMask) {
                const QRgb mPx = reinterpret_cast<const QRgb*>(
                    scaledMask.constScanLine(y))[x];
                const double alpha = qAlpha(mPx) / 255.0;
                r = origPixel.red()   * (1.0-alpha) + r * alpha;
                g = origPixel.green() * (1.0-alpha) + g * alpha;
                b = origPixel.blue()  * (1.0-alpha) + b * alpha;
            }
            scanline[x] = QRgba64::fromRgba64(
                clamp16(r), clamp16(g), clamp16(b), origPixel.alpha());
        }
    }
    // OpenCV NR and Sharpening (applied post pixel-loop)
#ifdef HAVE_OPENCV
    const double nr = scalarAdj(adjustments, AdjustmentType::NoiseReduction);
    const double sh = scalarAdj(adjustments, AdjustmentType::Sharpening);
    if (nr > 0.0 || sh > 0.0) {
        QImage img8 = image.convertToFormat(QImage::Format_RGB888);
        cv::Mat mat(img8.height(), img8.width(), CV_8UC3,
                    img8.bits(), img8.bytesPerLine());
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
        if (nr > 0.0) {
            float h = static_cast<float>(nr * 0.5);
            cv::fastNlMeansDenoisingColored(mat, mat, h, h, 7, 21);
        }
        if (sh > 0.0) {
            cv::Mat blurred;
            cv::GaussianBlur(mat, blurred, cv::Size(0,0), 3);
            cv::addWeighted(mat, 1.0 + sh/100.0,
                            blurred, -(sh/100.0), 0, mat);
        }
        cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
        QImage result(mat.data, mat.cols, mat.rows,
                      static_cast<int>(mat.step), QImage::Format_RGB888);
        image = result.copy().convertToFormat(QImage::Format_RGBA64);
    }
#endif
    return image;
}
} // namespace lumen
```

---

## TASK 25 — REPLACE `core/export-core/ExportService.hpp`

```cpp
#pragma once
#include "editor-core/DocumentModel.hpp"
#include "image-core/RenderPipeline.hpp"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
namespace lumen {
class ExportService final : public QObject {
    Q_OBJECT
public:
    explicit ExportService(QObject* parent = nullptr);
    [[nodiscard]] bool exportImage(const DocumentModel& document,
                                   const QString& path, int quality = 92) const;
    void exportBatch(const DocumentModel& document,
                     const QString& directory,
                     const QStringList& formats);
signals:
    void batchComplete();
    void batchFailed(QString reason);
private:
    RenderPipeline m_renderPipeline;
};
} // namespace lumen
```

---

## TASK 26 — REPLACE `core/export-core/ExportService.cpp`

```cpp
#include "export-core/ExportService.hpp"
#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>
namespace lumen {
ExportService::ExportService(QObject* parent) : QObject(parent) {}
bool ExportService::exportImage(const DocumentModel& document,
                                 const QString& path, int quality) const
{
    const QImage rendered = m_renderPipeline.renderFullResolution(document);
    if (rendered.isNull()) return false;
    return rendered.save(path, nullptr, quality);
}
void ExportService::exportBatch(const DocumentModel& document,
                                 const QString& directory,
                                 const QStringList& formats)
{
    const QString baseName = QFileInfo(document.sourcePath()).baseName();
    const QImage rendered  = m_renderPipeline.renderFullResolution(document);
    if (rendered.isNull()) { emit batchFailed("Render failed"); return; }
    QtConcurrent::run([this, rendered, directory, baseName, formats]() {
        bool allOk = true;
        for (const QString& fmt : formats) {
            const QString path = directory + "/" + baseName + "." + fmt.toLower();
            const int quality  = fmt.toLower() == "jpg" ? 92 : -1;
            if (!rendered.save(path, nullptr, quality)) allOk = false;
        }
        QMetaObject::invokeMethod(this,
            allOk ? &ExportService::batchComplete
                  : [this]{ emit batchFailed("One or more formats failed"); },
            Qt::QueuedConnection);
    });
}
} // namespace lumen
```

---

## TASK 27 — REPLACE `core/CMakeLists.txt`

```cmake
add_library(lumen_core STATIC
    shared-types/Adjustment.hpp
    shared-types/Layer.hpp
    shared-types/Mask.hpp
    editor-core/DocumentModel.cpp
    editor-core/DocumentModel.hpp
    image-core/RenderPipeline.cpp
    image-core/RenderPipeline.hpp
    image-core/BlendModes.hpp
    image-core/RawImporter.cpp
    image-core/RawImporter.hpp
    image-core/ColorManager.cpp
    image-core/ColorManager.hpp
    mask-core/BrushEngine.cpp
    mask-core/BrushEngine.hpp
    mask-core/MaskDocument.cpp
    mask-core/MaskDocument.hpp
    ai-core/AiRuntime.cpp
    ai-core/AiRuntime.hpp
    ai-core/OnnxSession.cpp
    ai-core/OnnxSession.hpp
    ai-core/MaskPredictor.cpp
    ai-core/MaskPredictor.hpp
    ai-core/InpaintEngine.cpp
    ai-core/InpaintEngine.hpp
    ai-core/UpscaleEngine.cpp
    ai-core/UpscaleEngine.hpp
    export-core/ExportService.cpp
    export-core/ExportService.hpp
    storage/ProjectStore.cpp
    storage/ProjectStore.hpp
)
target_include_directories(lumen_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(lumen_core
    PUBLIC Qt6::Core Qt6::Gui Qt6::Sql Qt6::Concurrent
)
# ONNX Runtime (optional — install via vcpkg or SDK)
find_package(onnxruntime QUIET
    HINTS "$ENV{ONNXRUNTIME_ROOT}/lib/cmake/onnxruntime")
if(onnxruntime_FOUND)
    message(STATUS "ONNX Runtime found — AI inference enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_ONNXRUNTIME)
    target_link_libraries(lumen_core PUBLIC onnxruntime::onnxruntime)
else()
    message(STATUS "ONNX Runtime NOT found — AI features disabled")
endif()
# LibRaw (optional — vcpkg: libraw)
find_package(LibRaw QUIET)
if(LibRaw_FOUND)
    message(STATUS "LibRaw found — RAW import enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_LIBRAW)
    target_link_libraries(lumen_core PUBLIC LibRaw::LibRaw)
else()
    message(STATUS "LibRaw NOT found — RAW import disabled")
endif()
# Little CMS 2 (optional — vcpkg: lcms2)
find_package(lcms2 QUIET)
if(lcms2_FOUND)
    message(STATUS "lcms2 found — color management enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_LCMS2)
    target_link_libraries(lumen_core PUBLIC lcms2)
else()
    message(STATUS "lcms2 NOT found — color profiles disabled")
endif()
# OpenCV (optional — vcpkg: opencv4[core,imgproc,photo])
find_package(OpenCV QUIET COMPONENTS core imgproc photo)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found — NR and sharpening enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_OPENCV)
    target_link_libraries(lumen_core PUBLIC ${OpenCV_LIBS})
else()
    message(STATUS "OpenCV NOT found — NR/sharpening disabled")
endif()
```

---

## TASK 28 — REPLACE `app/src/editor/DocumentController.hpp`

```cpp
#pragma once
#include "editor-core/DocumentModel.hpp"
#include "export-core/ExportService.hpp"
#include "image-core/RenderPipeline.hpp"
#include "mask-core/BrushEngine.hpp"
#include "ai-core/AiRuntime.hpp"
#include "ai-core/InpaintEngine.hpp"
#include "ai-core/UpscaleEngine.hpp"
#include "storage/ProjectStore.hpp"
#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <atomic>
#include <memory>
class DocumentController final : public QObject {
    Q_OBJECT
    // Core
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY documentChanged)
    Q_PROPERTY(QString imageUrl READ imageUrl NOTIFY previewChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(bool showOriginal READ showOriginal WRITE setShowOriginal NOTIFY previewChanged)
    // Adjustments
    Q_PROPERTY(double exposure    READ exposure    WRITE setExposure    NOTIFY adjustmentsChanged)
    Q_PROPERTY(double contrast    READ contrast    WRITE setContrast    NOTIFY adjustmentsChanged)
    Q_PROPERTY(double saturation  READ saturation  WRITE setSaturation  NOTIFY adjustmentsChanged)
    Q_PROPERTY(double highlights  READ highlights  WRITE setHighlights  NOTIFY adjustmentsChanged)
    Q_PROPERTY(double shadows     READ shadows     WRITE setShadows     NOTIFY adjustmentsChanged)
    Q_PROPERTY(double whites      READ whites      WRITE setWhites      NOTIFY adjustmentsChanged)
    Q_PROPERTY(double blacks      READ blacks      WRITE setBlacks      NOTIFY adjustmentsChanged)
    Q_PROPERTY(double vibrance    READ vibrance    WRITE setVibrance    NOTIFY adjustmentsChanged)
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY adjustmentsChanged)
    Q_PROPERTY(double tint        READ tint        WRITE setTint        NOTIFY adjustmentsChanged)
    Q_PROPERTY(double noiseReduction READ noiseReduction WRITE setNoiseReduction NOTIFY adjustmentsChanged)
    Q_PROPERTY(double sharpening  READ sharpening  WRITE setSharpening  NOTIFY adjustmentsChanged)
    // M5 — masking
    Q_PROPERTY(int    activeTool  READ activeTool  WRITE setActiveTool  NOTIFY activeToolChanged)
    Q_PROPERTY(bool   hasMask     READ hasMask     NOTIFY maskChanged)
    Q_PROPERTY(QString maskUrl    READ maskUrl     NOTIFY maskChanged)
    Q_PROPERTY(int sourceWidth    READ sourceWidth  NOTIFY documentChanged)
    Q_PROPERTY(int sourceHeight   READ sourceHeight NOTIFY documentChanged)
    // M6/M7 — AI
    Q_PROPERTY(bool   aiBusy     READ aiBusy      NOTIFY aiBusyChanged)
    Q_PROPERTY(QString aiStatus  READ aiStatus    NOTIFY aiStatusChanged)
    // M8 — layers
    Q_PROPERTY(QVariantList layerModel READ layerModel NOTIFY layersChanged)
    // M10 — recovery
    Q_PROPERTY(bool hasPendingRecovery READ hasPendingRecovery NOTIFY recoveryChanged)
public:
    explicit DocumentController(QObject* parent = nullptr);
    [[nodiscard]] bool    hasDocument()  const;
    [[nodiscard]] QString sourceName()   const;
    [[nodiscard]] QString imageUrl()     const;
    [[nodiscard]] bool    canUndo()      const;
    [[nodiscard]] bool    canRedo()      const;
    [[nodiscard]] bool    showOriginal() const;
    void setShowOriginal(bool v);
    [[nodiscard]] double exposure()      const;  void setExposure(double v);
    [[nodiscard]] double contrast()      const;  void setContrast(double v);
    [[nodiscard]] double saturation()    const;  void setSaturation(double v);
    [[nodiscard]] double highlights()    const;  void setHighlights(double v);
    [[nodiscard]] double shadows()       const;  void setShadows(double v);
    [[nodiscard]] double whites()        const;  void setWhites(double v);
    [[nodiscard]] double blacks()        const;  void setBlacks(double v);
    [[nodiscard]] double vibrance()      const;  void setVibrance(double v);
    [[nodiscard]] double temperature()   const;  void setTemperature(double v);
    [[nodiscard]] double tint()          const;  void setTint(double v);
    [[nodiscard]] double noiseReduction()const;  void setNoiseReduction(double v);
    [[nodiscard]] double sharpening()    const;  void setSharpening(double v);
    [[nodiscard]] int    activeTool()    const;  void setActiveTool(int tool);
    [[nodiscard]] bool   hasMask()       const;
    [[nodiscard]] QString maskUrl()      const;
    [[nodiscard]] int sourceWidth()      const;
    [[nodiscard]] int sourceHeight()     const;
    [[nodiscard]] bool   aiBusy()        const;
    [[nodiscard]] QString aiStatus()     const;
    [[nodiscard]] QVariantList layerModel() const;
    [[nodiscard]] bool hasPendingRecovery() const;
    Q_INVOKABLE bool openImage(const QUrl& url);
    Q_INVOKABLE bool saveProject(const QUrl& url);
    Q_INVOKABLE bool loadProject(const QUrl& url);
    Q_INVOKABLE bool exportImage(const QUrl& url);
    Q_INVOKABLE void resetAdjustments();
    Q_INVOKABLE void rotateClockwise();
    Q_INVOKABLE void rotateCounterClockwise();
    Q_INVOKABLE void flipHorizontal();
    Q_INVOKABLE void flipVertical();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    // M5
    Q_INVOKABLE void paintMaskStroke(double x, double y, double radius, bool erase);
    Q_INVOKABLE void clearMask();
    // M6
    Q_INVOKABLE void requestAiMask(double x, double y);
    // M7
    Q_INVOKABLE void applyInpaint();
    Q_INVOKABLE void applyUpscale();
    // M8
    Q_INVOKABLE void addImageLayer(const QUrl& url);
    Q_INVOKABLE void deleteLayer(const QString& id);
    Q_INVOKABLE void setLayerOpacity(const QString& id, double opacity);
    Q_INVOKABLE void setLayerVisible(const QString& id, bool visible);
    Q_INVOKABLE void exportBatch(const QUrl& directory, const QStringList& formats);
    // M10
    Q_INVOKABLE void recoverProject();
    Q_INVOKABLE void discardRecovery();
signals:
    void documentChanged();
    void previewChanged();
    void adjustmentsChanged();
    void historyChanged();
    void operationFailed(QString message);
    void activeToolChanged();
    void maskChanged();
    void aiBusyChanged();
    void aiStatusChanged();
    void layersChanged();
    void recoveryChanged();
private:
    void rebuildPreview();
    void setAdjustment(lumen::AdjustmentType type, double value);
    [[nodiscard]] QString localPath(const QUrl& url) const;
    void saveMaskToTemp();
    void setAiBusy(bool busy);
    void setAiStatus(const QString& status);
    void autoSave();
    void checkRecovery();
    QString autosavePath() const;
    lumen::DocumentModel   m_document;
    lumen::RenderPipeline  m_renderPipeline;
    lumen::ExportService   m_exportService;
    lumen::ProjectStore    m_projectStore;
    lumen::AiRuntime       m_aiRuntime;
    lumen::InpaintEngine   m_inpaintEngine;
    lumen::UpscaleEngine   m_upscaleEngine;
    QString  m_previewPath;
    int      m_previewVersion    = 0;
    bool     m_showOriginal      = false;
    QFutureWatcher<QImage>* m_previewWatcher = nullptr;
    bool     m_previewPending    = false;
    int      m_previewRequestId  = 0;
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
    // M5
    int      m_activeTool        = 0;
    std::unique_ptr<lumen::BrushEngine> m_brushEngine;
    QString  m_maskTempPath;
    int      m_maskVersion       = 0;
    // M6/M7
    bool     m_aiBusy            = false;
    QString  m_aiStatus;
    // M10
    QTimer*  m_autosaveTimer     = nullptr;
    bool     m_hasPendingRecovery = false;
};
```

---

## TASK 29 — REPLACE `app/src/editor/DocumentController.cpp`

```cpp
#include "editor/DocumentController.hpp"
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QVariantMap>
#include <QtConcurrent>
DocumentController::DocumentController(QObject* parent)
    : QObject(parent)
    , m_cancelFlag(std::make_shared<std::atomic<bool>>(false))
{
    connect(&m_document, &lumen::DocumentModel::changed, this, [this]() {
        rebuildPreview();
        emit documentChanged();
        emit adjustmentsChanged();
        emit layersChanged();
    });
    connect(&m_document, &lumen::DocumentModel::historyChanged,
            this, &DocumentController::historyChanged);
    connect(&m_aiRuntime, &lumen::AiRuntime::busyChanged,
            this, [this](bool busy){ setAiBusy(busy); });
    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setInterval(2 * 60 * 1000);
    connect(m_autosaveTimer, &QTimer::timeout, this, &DocumentController::autoSave);
    m_autosaveTimer->start();
    checkRecovery();
}
// ─── Core properties ───────────────────────────────────────────────────────
bool DocumentController::hasDocument() const { return m_document.hasDocument(); }
QString DocumentController::sourceName() const
{
    return m_document.hasDocument()
        ? QFileInfo(m_document.sourcePath()).fileName()
        : "No image loaded";
}
QString DocumentController::imageUrl() const
{
    if (m_showOriginal && m_document.hasDocument())
        return QUrl::fromLocalFile(m_document.sourcePath()).toString();
    return m_previewPath.isEmpty() ? QString()
        : QUrl::fromLocalFile(m_previewPath).toString();
}
bool DocumentController::canUndo()      const { return m_document.canUndo(); }
bool DocumentController::canRedo()      const { return m_document.canRedo(); }
bool DocumentController::showOriginal() const { return m_showOriginal; }
void DocumentController::setShowOriginal(bool v)
{ if (m_showOriginal == v) return; m_showOriginal = v; emit previewChanged(); }
// ─── Adjustments ──────────────────────────────────────────────────────────
#define ADJ_GET(Name, Type) \\
    double DocumentController::Name() const \\
    { return m_document.scalarAdjustment(lumen::AdjustmentType::Type); }
#define ADJ_SET(Name, Type) \\
    void DocumentController::set##Name(double v) \\
    { setAdjustment(lumen::AdjustmentType::Type, v); }
#define ADJ(Name, Type) ADJ_GET(Name, Type) ADJ_SET(Name, Type)
ADJ(exposure, Exposure)   ADJ(contrast, Contrast)
ADJ(saturation, Saturation) ADJ(highlights, Highlights)
ADJ(shadows, Shadows)     ADJ(whites, Whites)
ADJ(blacks, Blacks)       ADJ(vibrance, Vibrance)
ADJ(temperature, Temperature) ADJ(tint, Tint)
ADJ(noiseReduction, NoiseReduction) ADJ(sharpening, Sharpening)
// ─── M5 mask properties ───────────────────────────────────────────────────
int  DocumentController::activeTool()  const { return m_activeTool; }
bool DocumentController::hasMask()     const { return !m_document.activeMask().isNull(); }
QString DocumentController::maskUrl()  const { return m_maskTempPath.isEmpty() ? QString()
    : QUrl::fromLocalFile(m_maskTempPath).toString(); }
int DocumentController::sourceWidth()  const { return m_document.sourceSize().width(); }
int DocumentController::sourceHeight() const { return m_document.sourceSize().height(); }
void DocumentController::setActiveTool(int tool)
{ if (m_activeTool == tool) return; m_activeTool = tool; emit activeToolChanged(); }
// ─── M6/M7 AI properties ──────────────────────────────────────────────────
bool    DocumentController::aiBusy()    const { return m_aiBusy; }
QString DocumentController::aiStatus()  const { return m_aiStatus; }
void DocumentController::setAiBusy(bool busy)
{ if (m_aiBusy == busy) return; m_aiBusy = busy; emit aiBusyChanged(); }
void DocumentController::setAiStatus(const QString& s)
{ m_aiStatus = s; emit aiStatusChanged(); }
// ─── M8 layer model ───────────────────────────────────────────────────────
QVariantList DocumentController::layerModel() const
{
    QVariantList list;
    for (const lumen::Layer& l : m_document.layers()) {
        QVariantMap m;
        m["id"]      = l.id;
        m["name"]    = l.name;
        m["opacity"] = l.opacity;
        m["visible"] = l.visible;
        m["order"]   = l.order;
        list.prepend(m); // topmost layer first
    }
    return list;
}
// ─── M10 recovery ─────────────────────────────────────────────────────────
bool DocumentController::hasPendingRecovery() const { return m_hasPendingRecovery; }
QString DocumentController::autosavePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + "/lumenforge-autosave.lfproj";
}
void DocumentController::checkRecovery()
{
    m_hasPendingRecovery = QFileInfo::exists(autosavePath());
    if (m_hasPendingRecovery) emit recoveryChanged();
}
void DocumentController::autoSave()
{
    if (!m_document.hasDocument()) return;
    m_projectStore.saveProject(m_document, autosavePath());
}
void DocumentController::recoverProject()
{
    if (m_projectStore.loadProject(m_document, autosavePath()))
        QFile::remove(autosavePath());
    m_hasPendingRecovery = false;
    emit recoveryChanged();
}
void DocumentController::discardRecovery()
{
    QFile::remove(autosavePath());
    m_hasPendingRecovery = false;
    emit recoveryChanged();
}
// ─── Invokables ───────────────────────────────────────────────────────────
bool DocumentController::openImage(const QUrl& url)
{
    if (!m_document.openSourceImage(localPath(url))) {
        emit operationFailed("Could not open image."); return false;
    }
    // Reset brush engine to new image size
    m_brushEngine = std::make_unique<lumen::BrushEngine>(m_document.sourceSize());
    m_maskTempPath.clear();
    emit maskChanged();
    QFile::remove(autosavePath());
    m_hasPendingRecovery = false;
    emit recoveryChanged();
    return true;
}
bool DocumentController::saveProject(const QUrl& url)
{
    if (!m_projectStore.saveProject(m_document, localPath(url))) {
        emit operationFailed("Could not save project."); return false;
    }
    return true;
}
bool DocumentController::loadProject(const QUrl& url)
{
    if (!m_projectStore.loadProject(m_document, localPath(url))) {
        emit operationFailed("Could not load project."); return false;
    }
    m_brushEngine = std::make_unique<lumen::BrushEngine>(m_document.sourceSize());
    return true;
}
bool DocumentController::exportImage(const QUrl& url)
{
    if (!m_exportService.exportImage(m_document, localPath(url))) {
        emit operationFailed("Could not export image."); return false;
    }
    return true;
}
void DocumentController::resetAdjustments()
{
    for (auto type : {
        lumen::AdjustmentType::Exposure,    lumen::AdjustmentType::Contrast,
        lumen::AdjustmentType::Saturation,  lumen::AdjustmentType::Highlights,
        lumen::AdjustmentType::Shadows,     lumen::AdjustmentType::Whites,
        lumen::AdjustmentType::Blacks,      lumen::AdjustmentType::Vibrance,
        lumen::AdjustmentType::Temperature, lumen::AdjustmentType::Tint,
        lumen::AdjustmentType::NoiseReduction, lumen::AdjustmentType::Sharpening,
    }) setAdjustment(type, 0.0);
}
void DocumentController::rotateClockwise()       { m_document.rotateClockwise(); }
void DocumentController::rotateCounterClockwise(){ m_document.rotateCounterClockwise(); }
void DocumentController::flipHorizontal()        { m_document.flipHorizontal(); }
void DocumentController::flipVertical()          { m_document.flipVertical(); }
void DocumentController::undo()                  { m_document.undo(); }
void DocumentController::redo()                  { m_document.redo(); }
// M5
void DocumentController::paintMaskStroke(double x, double y, double radius, bool erase)
{
    if (!m_document.hasDocument() || !m_brushEngine) return;
    m_brushEngine->paintStroke(QPointF(x, y), radius, 0.85, erase);
    m_document.setActiveMask(m_brushEngine->mask());
    saveMaskToTemp();
    emit maskChanged();
}
void DocumentController::clearMask()
{
    if (m_brushEngine) m_brushEngine->clear();
    m_document.setActiveMask(QImage());
    m_maskTempPath.clear();
    emit maskChanged();
}
void DocumentController::saveMaskToTemp()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QString("/lumenforge-mask-%1.png").arg(++m_maskVersion);
    if (m_document.activeMask().save(path)) {
        if (!m_maskTempPath.isEmpty()) QFile::remove(m_maskTempPath);
        m_maskTempPath = path;
    }
}
// M6
void DocumentController::requestAiMask(double x, double y)
{
    if (!m_document.hasDocument() || m_aiBusy) return;
    setAiStatus("Loading model…");
    const QImage src = m_document.sourceImage();
    const QPointF pt(x, y);
    m_aiRuntime.predictMask(src, pt, [this](QImage result) {
        if (!result.isNull()) {
            if (!m_brushEngine)
                m_brushEngine = std::make_unique<lumen::BrushEngine>(m_document.sourceSize());
            m_brushEngine->mask() = result;
            m_document.setActiveMask(result);
            saveMaskToTemp();
            emit maskChanged();
        }
        setAiStatus("Done");
    });
}
// M7
void DocumentController::applyInpaint()
{
    if (!m_document.hasDocument() || m_document.activeMask().isNull() || m_aiBusy) return;
    setAiBusy(true);
    setAiStatus("Loading model…");
    const QImage src  = m_document.sourceImage();
    const QImage mask = m_document.activeMask();
    auto* w = new QFutureWatcher<QImage>(this);
    connect(w, &QFutureWatcher<QImage>::finished, this, [this, w]() {
        m_document.replaceSourceImage(w->result());
        if (m_brushEngine) m_brushEngine->resize(m_document.sourceSize());
        setAiStatus("Done");
        setAiBusy(false);
        rebuildPreview();
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([this, src, mask]() mutable -> QImage {
        QMetaObject::invokeMethod(this, [this]{ setAiStatus("Running inference…"); });
        return m_inpaintEngine.inpaint(src, mask);
    }));
}
void DocumentController::applyUpscale()
{
    if (!m_document.hasDocument() || m_aiBusy) return;
    setAiBusy(true);
    setAiStatus("Loading model…");
    const QImage src = m_document.sourceImage();
    auto* w = new QFutureWatcher<QImage>(this);
    connect(w, &QFutureWatcher<QImage>::finished, this, [this, w]() {
        m_document.replaceSourceImage(w->result());
        if (m_brushEngine) m_brushEngine->resize(m_document.sourceSize());
        setAiStatus("Done");
        setAiBusy(false);
        rebuildPreview();
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([this, src]() mutable -> QImage {
        QMetaObject::invokeMethod(this, [this]{ setAiStatus("Running inference…"); });
        return m_upscaleEngine.upscale(src);
    }));
}
// M8
void DocumentController::addImageLayer(const QUrl& url)
{ m_document.addImageLayer(localPath(url)); }
void DocumentController::deleteLayer(const QString& id)
{ m_document.deleteLayer(id); }
void DocumentController::setLayerOpacity(const QString& id, double opacity)
{ m_document.setLayerOpacity(id, opacity); }
void DocumentController::setLayerVisible(const QString& id, bool visible)
{ m_document.setLayerVisible(id, visible); }
void DocumentController::exportBatch(const QUrl& directory, const QStringList& formats)
{ m_exportService.exportBatch(m_document, directory.toLocalFile(), formats); }
// ─── Private ──────────────────────────────────────────────────────────────
void DocumentController::rebuildPreview()
{
    if (!m_document.hasDocument()) {
        ++m_previewRequestId;
        m_previewPath.clear();
        emit previewChanged();
        return;
    }
    if (m_previewWatcher && m_previewWatcher->isRunning()) {
        m_previewPending = true;
        *m_cancelFlag = true;
        return;
    }
    *m_cancelFlag = false;
    const QImage src  = m_document.sourceImage();
    const QImage mask = m_document.activeMask();
    const QVector<lumen::Adjustment> adjs = m_document.adjustments();
    const int requestId = ++m_previewRequestId;
    auto cancelled = m_cancelFlag;
    auto* watcher = new QFutureWatcher<QImage>(this);
    m_previewWatcher = watcher;
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, requestId]() {
        const QImage preview = watcher->result();
        if (!preview.isNull() && requestId == m_previewRequestId) {
            const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                + QString("/lumenforge-preview-%1.png").arg(++m_previewVersion);
            if (preview.save(path)) { m_previewPath = path; emit previewChanged(); }
        }
        watcher->deleteLater();
        m_previewWatcher = nullptr;
        if (m_previewPending) { m_previewPending = false; rebuildPreview(); }
    });
    watcher->setFuture(QtConcurrent::run(
        [pipeline = m_renderPipeline, src, adjs, mask, cancelled]() {
            return pipeline.renderPreviewFromData(src, adjs, QSize(1800, 1400), mask, cancelled);
        }));
}
void DocumentController::setAdjustment(lumen::AdjustmentType type, double value)
{
    if (qFuzzyCompare(m_document.scalarAdjustment(type), value)) return;
    m_document.setScalarAdjustment(type, value);
}
QString DocumentController::localPath(const QUrl& url) const
{ return url.isLocalFile() ? url.toLocalFile() : url.toString(); }
```

---

## TASK 30 — REPLACE `app/resources/qml/MaskCanvas.qml`

```qml
import QtQuick
import QtQuick.Controls
Canvas {
    id: maskCanvas
    property var   docCtrl:        null
    property double brushRadius:   50
    property bool   eraseMode:     false
    property bool   paintEnabled:  false
    implicitWidth:  200
    implicitHeight: 200
    Connections {
        target: docCtrl
        function onMaskChanged() { maskCanvas.requestPaint() }
    }
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        if (!docCtrl || !docCtrl.hasDocument || !docCtrl.hasMask) return;
        const url = docCtrl.maskUrl;
        if (!url || url.length === 0) return;
        ctx.globalAlpha = 0.45;
        ctx.drawImage(url, 0, 0, width, height);
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        property bool dragging: false
        onPressed:  (mouse) => { if (paintEnabled) { dragging = true;  stroke(mouse.x, mouse.y) } }
        onReleased:            { dragging = false }
        onExited:              { dragging = false }
        onPositionChanged: (mouse) => { if (dragging && paintEnabled) stroke(mouse.x, mouse.y) }
        function stroke(x, y) {
            const sw = docCtrl.sourceWidth  || maskCanvas.width
            const sh = docCtrl.sourceHeight || maskCanvas.height
            docCtrl.paintMaskStroke(
                x / maskCanvas.width  * sw,
                y / maskCanvas.height * sh,
                brushRadius, eraseMode)
        }
    }
}
```

---

## TASK 31 — REPLACE `app/resources/qml/Main.qml`

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
ApplicationWindow {
    id: root
    width: 1440
    height: 920
    visible: true
    title: "LumenForge"
    color: "#15171b"
    property real zoom: 1.0
    property real brushRadius: 50
    function fitZoom() {
        if (imagePreview.sourceSize.width <= 0) return 1.0
        return Math.min(
            canvasFlick.width  / imagePreview.sourceSize.width,
            canvasFlick.height / imagePreview.sourceSize.height) * 0.95
    }
    // ── Dialogs ────────────────────────────────────────────────────────────
    FileDialog {
        id: openImageDialog
        title: "Open image"
        nameFilters: [
            "Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp " +
            "*.cr2 *.cr3 *.nef *.arw *.dng *.raf *.orf *.rw2)",
            "All files (*)"
        ]
        onAccepted: documentController.openImage(selectedFile)
    }
    FileDialog {
        id: openProjectDialog; title: "Open project"
        nameFilters: ["LumenForge project (*.lfproj)"]
        onAccepted: documentController.loadProject(selectedFile)
    }
    FileDialog {
        id: saveProjectDialog; title: "Save project"
        fileMode: FileDialog.SaveFile; defaultSuffix: "lfproj"
        nameFilters: ["LumenForge project (*.lfproj)"]
        onAccepted: documentController.saveProject(selectedFile)
    }
    FileDialog {
        id: exportDialog; title: "Export image"
        fileMode: FileDialog.SaveFile; defaultSuffix: "png"
        nameFilters: ["PNG (*.png)", "JPEG (*.jpg)", "WebP (*.webp)"]
        onAccepted: documentController.exportImage(selectedFile)
    }
    FileDialog {
        id: addLayerDialog; title: "Add image layer"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp *.tif *.tiff *.bmp)"]
        onAccepted: documentController.addImageLayer(selectedFile)
    }
    // ── Recovery dialog ────────────────────────────────────────────────────
    Dialog {
        id: recoveryDialog
        title: "Recover unsaved work?"
        modal: true
        visible: documentController.hasPendingRecovery
        anchors.centerIn: parent
        Label { text: "An autosaved project was found. Recover it?" }
        footer: DialogButtonBox {
            Button { text: "Recover"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: { documentController.recoverProject(); recoveryDialog.close() } }
            Button { text: "Discard"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: { documentController.discardRecovery(); recoveryDialog.close() } }
        }
    }
    // ── Shortcuts ──────────────────────────────────────────────────────────
    Shortcut { sequence: StandardKey.Open;  onActivated: openImageDialog.open() }
    Shortcut { sequence: StandardKey.Save;  onActivated: saveProjectDialog.open() }
    Shortcut { sequence: "Ctrl+E";          onActivated: exportDialog.open() }
    Shortcut { sequence: "Ctrl+0";          onActivated: root.zoom = 1.0 }
    Shortcut { sequence: "Ctrl++";          onActivated: root.zoom = Math.min(4.0, root.zoom+0.1) }
    Shortcut { sequence: "Ctrl+-";          onActivated: root.zoom = Math.max(0.1, root.zoom-0.1) }
    Shortcut { sequence: StandardKey.Undo;  onActivated: documentController.undo() }
    Shortcut { sequence: StandardKey.Redo;  onActivated: documentController.redo() }
    Shortcut { sequence: "\\\\";             onActivated: documentController.showOriginal = !documentController.showOriginal }
    Shortcut { sequence: "Escape";
        onActivated: if (documentController.activeTool > 0) documentController.activeTool = 0 }
    // ── Header ─────────────────────────────────────────────────────────────
    header: ToolBar {
        height: 52
        background: Rectangle { color: "#1e2228" }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14; spacing: 10
            Label { text: "LumenForge"; color: "#f4f7fb"; font.pixelSize: 18; font.bold: true }
            Label { text: documentController.sourceName; color: "#98a2b3"
                elide: Text.ElideMiddle; Layout.fillWidth: true }
            Button { text: "Open";    onClicked: openImageDialog.open() }
            Button { text: "Project"; onClicked: openProjectDialog.open() }
            Button { text: "Save";    enabled: documentController.hasDocument
                onClicked: saveProjectDialog.open() }
            Button { text: "Export";  enabled: documentController.hasDocument
                onClicked: exportDialog.open() }
        }
    }
    // ── Footer — Layer panel ────────────────────────────────────────────────
    footer: Rectangle {
        height: 120
        color: "#1b1f25"
        border.color: "#2b313a"
        RowLayout {
            anchors.fill: parent; anchors.margins: 8; spacing: 8
            // Layer stack panel
            Rectangle {
                Layout.preferredWidth: 320; Layout.fillHeight: true
                color: "#20252c"; radius: 6; border.color: "#353c46"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 6; spacing: 4
                    RowLayout {
                        spacing: 6
                        Label { text: "Layers"; color: "#cbd5e1"; font.pixelSize: 12; font.bold: true
                            Layout.fillWidth: true }
                        Button { text: "+"; implicitWidth: 28; implicitHeight: 24
                            enabled: documentController.hasDocument
                            onClicked: addLayerDialog.open() }
                    }
                    ListView {
                        id: layerList
                        Layout.fillWidth: true; Layout.fillHeight: true
                        model: documentController.layerModel
                        clip: true
                        delegate: Rectangle {
                            width: layerList.width; height: 28
                            color: "transparent"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 4; spacing: 4
                                Button { text: modelData.visible ? "👁" : "○"
                                    implicitWidth: 24; implicitHeight: 22; flat: true
                                    onClicked: documentController.setLayerVisible(modelData.id, !modelData.visible) }
                                Label { text: modelData.name; color: "#cbd5e1"
                                    font.pixelSize: 11; Layout.fillWidth: true
                                    elide: Text.ElideRight }
                                Slider { from: 0; to: 1; value: modelData.opacity
                                    implicitWidth: 60; implicitHeight: 22
                                    onMoved: documentController.setLayerOpacity(modelData.id, value) }
                                Button { text: "✕"; implicitWidth: 24; implicitHeight: 22; flat: true
                                    onClicked: documentController.deleteLayer(modelData.id) }
                            }
                        }
                    }
                }
            }
            // History / Masks / Filmstrip placeholders
            Repeater {
                model: ["History", "Masks", "Filmstrip"]
                delegate: Rectangle {
                    Layout.preferredWidth: 160; Layout.fillHeight: true
                    radius: 6; color: "#20252c"; border.color: "#353c46"
                    Label { anchors.centerIn: parent; text: modelData
                        color: "#cbd5e1"; font.pixelSize: 12 }
                }
            }
            // AI status
            Label {
                text: documentController.aiStatus
                color: "#f59e0b"; font.pixelSize: 11
                visible: documentController.aiStatus.length > 0
                Layout.alignment: Qt.AlignVCenter
            }
            Item { Layout.fillWidth: true }
        }
    }
    // ── Main layout ────────────────────────────────────────────────────────
    RowLayout {
        anchors.fill: parent; spacing: 0
        // Tool rail
        Rectangle {
            Layout.preferredWidth: 72; Layout.fillHeight: true
            color: "#181b20"; border.color: "#272d35"
            ColumnLayout {
                anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 14; spacing: 6
                Repeater {
                    model: [
                        { icon: "M", tip: "Move",         tool: 0 },
                        { icon: "C", tip: "Crop",         tool: 0 },
                        { icon: "B", tip: "Brush mask",   tool: 1 },
                        { icon: "E", tip: "Erase mask",   tool: 2 },
                        { icon: "G", tip: "Gradient mask",tool: 0 },
                        { icon: "R", tip: "Radial mask",  tool: 0 }
                    ]
                    delegate: Button {
                        Layout.preferredWidth: 44; Layout.preferredHeight: 38
                        text: modelData.icon
                        checkable: modelData.tool > 0
                        checked: modelData.tool > 0 && documentController.activeTool === modelData.tool
                        ToolTip.visible: hovered; ToolTip.text: modelData.tip
                        onClicked: {
                            if (modelData.tool > 0)
                                documentController.activeTool =
                                    (documentController.activeTool === modelData.tool) ? 0 : modelData.tool
                        }
                    }
                }
                Rectangle { width: 44; height: 1; color: "#2b313a" }
                Label { text: "Size"; color: "#6b7280"; font.pixelSize: 9
                    Layout.alignment: Qt.AlignHCenter }
                Slider {
                    from: 5; to: 200; value: root.brushRadius
                    orientation: Qt.Vertical; implicitHeight: 80
                    Layout.alignment: Qt.AlignHCenter
                    onMoved: root.brushRadius = value
                }
            }
        }
        // Canvas area
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: "#101215"
            Flickable {
                id: canvasFlick; anchors.fill: parent
                contentWidth:  Math.max(width,  imagePreview.width)
                contentHeight: Math.max(height, imagePreview.height)
                clip: true
                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: (event) => {
                        const d = event.angleDelta.y > 0 ? 0.1 : -0.1
                        root.zoom = Math.min(4.0, Math.max(0.1, root.zoom + d))
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width:  Math.max(360, imagePreview.width  + 72)
                    height: Math.max(260, imagePreview.height + 72)
                    color: "#0c0e11"; border.color: "#2d333c"
                    Image {
                        id: imagePreview; anchors.centerIn: parent
                        source: documentController.imageUrl
                        cache: false; fillMode: Image.PreserveAspectFit
                        width:  sourceSize.width  > 0 ? sourceSize.width  * root.zoom : 0
                        height: sourceSize.height > 0 ? sourceSize.height * root.zoom : 0
                        asynchronous: true
                        onSourceSizeChanged: root.zoom = root.fitZoom()
                    }
                    MaskCanvas {
                        id: maskOverlay
                        anchors.centerIn: parent
                        width:  imagePreview.width
                        height: imagePreview.height
                        visible: documentController.activeTool > 0
                        docCtrl:      documentController
                        brushRadius:  root.brushRadius
                        eraseMode:    documentController.activeTool === 2
                        paintEnabled: documentController.activeTool > 0
                    }
                    BusyIndicator {
                        anchors.centerIn: parent
                        visible: documentController.aiBusy
                        running: documentController.aiBusy
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: !documentController.hasDocument
                        text: "Open an image to begin"
                        color: "#d0d5dd"; font.pixelSize: 22
                    }
                }
            }
            Row {
                anchors.left: parent.left; anchors.bottom: parent.bottom
                anchors.margins: 18; spacing: 8
                Button { text: "Fit";   enabled: documentController.hasDocument
                    onClicked: root.zoom = root.fitZoom() }
                Button { text: "100%";  enabled: documentController.hasDocument
                    onClicked: root.zoom = 1.0 }
                Button { text: "-";     enabled: documentController.hasDocument
                    onClicked: root.zoom = Math.max(0.1, root.zoom-0.1) }
                Button { text: "+";     enabled: documentController.hasDocument
                    onClicked: root.zoom = Math.min(4.0, root.zoom+0.1) }
                Button {
                    text: documentController.showOriginal ? "After" : "Before / After"
                    enabled: documentController.hasDocument
                    onClicked: documentController.showOriginal = !documentController.showOriginal
                }
                Button {
                    text: "Clear mask"
                    enabled: documentController.hasDocument && documentController.hasMask
                    onClicked: documentController.clearMask()
                }
            }
        }
        // Adjustments panel
        Rectangle {
            Layout.preferredWidth: 340; Layout.fillHeight: true
            color: "#1a1e24"; border.color: "#2c333d"
            ScrollView {
                anchors.fill: parent
                ColumnLayout {
                    width: parent.width; spacing: 14; anchors.margins: 18
                    Label { text: "Adjustments"; color: "#f2f4f7"
                        font.pixelSize: 18; font.bold: true
                        Layout.leftMargin: 18; Layout.topMargin: 18 }
                    Label { text: "Transform"; color: "#f2f4f7"
                        font.pixelSize: 14; font.bold: true; Layout.leftMargin: 18 }
                    GridLayout {
                        Layout.leftMargin: 18; Layout.rightMargin: 18
                        Layout.fillWidth: true; columns: 2; rowSpacing: 6; columnSpacing: 6
                        Button { text: "Rotate left";  enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.rotateCounterClockwise() }
                        Button { text: "Rotate right"; enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.rotateClockwise() }
                        Button { text: "Flip H"; enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.flipHorizontal() }
                        Button { text: "Flip V"; enabled: documentController.hasDocument
                            Layout.fillWidth: true; onClicked: documentController.flipVertical() }
                    }
                    RowLayout {
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true; spacing: 6
                        Button { text: "Undo"; enabled: documentController.canUndo
                            Layout.fillWidth: true; onClicked: documentController.undo() }
                        Button { text: "Redo"; enabled: documentController.canRedo
                            Layout.fillWidth: true; onClicked: documentController.redo() }
                    }
                    AdjustmentSlider { label: "Exposure";    from: -3;   to: 3
                        value: documentController.exposure;    onMoved: (v) => documentController.exposure = v }
                    AdjustmentSlider { label: "Contrast";    from: -100; to: 100
                        value: documentController.contrast;    onMoved: (v) => documentController.contrast = v }
                    AdjustmentSlider { label: "Saturation";  from: -100; to: 100
                        value: documentController.saturation;  onMoved: (v) => documentController.saturation = v }
                    AdjustmentSlider { label: "Highlights";  from: -100; to: 100
                        value: documentController.highlights;  onMoved: (v) => documentController.highlights = v }
                    AdjustmentSlider { label: "Shadows";     from: -100; to: 100
                        value: documentController.shadows;     onMoved: (v) => documentController.shadows = v }
                    AdjustmentSlider { label: "Whites";      from: -100; to: 100
                        value: documentController.whites;      onMoved: (v) => documentController.whites = v }
                    AdjustmentSlider { label: "Blacks";      from: -100; to: 100
                        value: documentController.blacks;      onMoved: (v) => documentController.blacks = v }
                    AdjustmentSlider { label: "Vibrance";    from: -100; to: 100
                        value: documentController.vibrance;    onMoved: (v) => documentController.vibrance = v }
                    AdjustmentSlider { label: "Temperature"; from: -100; to: 100
                        value: documentController.temperature; onMoved: (v) => documentController.temperature = v }
                    AdjustmentSlider { label: "Tint";        from: -100; to: 100
                        value: documentController.tint;        onMoved: (v) => documentController.tint = v }
                    Label { text: "Detail"; color: "#f2f4f7"
                        font.pixelSize: 14; font.bold: true; Layout.leftMargin: 18 }
                    AdjustmentSlider { label: "Noise Reduction"; from: 0; to: 100
                        value: documentController.noiseReduction
                        onMoved: (v) => documentController.noiseReduction = v }
                    AdjustmentSlider { label: "Sharpening"; from: 0; to: 100
                        value: documentController.sharpening
                        onMoved: (v) => documentController.sharpening = v }
                    Button {
                        text: "Reset all"
                        enabled: documentController.hasDocument
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.resetAdjustments()
                    }
                    Label { text: "AI tools"; color: "#f2f4f7"
                        font.pixelSize: 14; font.bold: true
                        Layout.leftMargin: 18; Layout.topMargin: 8 }
                    Button {
                        text: "Subject mask"
                        enabled: documentController.hasDocument && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.requestAiMask(
                            imagePreview.width / 2, imagePreview.height / 2)
                    }
                    Button {
                        text: "Background mask"
                        enabled: documentController.hasDocument && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: {
                            documentController.requestAiMask(
                                imagePreview.width / 2, imagePreview.height / 2)
                            // invert after — TODO: expose invertMask() invokable
                        }
                    }
                    Button {
                        text: "Object removal"
                        enabled: documentController.hasDocument && documentController.hasMask
                               && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.applyInpaint()
                    }
                    Button {
                        text: "Upscale ×4"
                        enabled: documentController.hasDocument && !documentController.aiBusy
                        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true
                        onClicked: documentController.applyUpscale()
                    }
                    Item { Layout.preferredHeight: 24 }
                }
            }
        }
    }
    // ── Inline component ──────────────────────────────────────────────────
    component AdjustmentSlider: ColumnLayout {
        id: sliderRoot
        property string label: ""
        property real   from:  0
        property real   to:    1
        property real   value: 0
        signal moved(real nextValue)
        Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.fillWidth: true; spacing: 4
        RowLayout {
            Layout.fillWidth: true
            Label { text: sliderRoot.label; color: "#d0d5dd"; Layout.fillWidth: true }
            Label { text: Number(slider.value).toFixed(sliderRoot.to <= 3 ? 2 : 0)
                color: "#98a2b3" }
        }
        Slider {
            id: slider; Layout.fillWidth: true
            from: sliderRoot.from; to: sliderRoot.to; value: sliderRoot.value
            enabled: documentController.hasDocument
            onMoved: sliderRoot.moved(value)
        }
    }
}
```

---

## TASK 32 — REPLACE `app/CMakeLists.txt`

```cmake
qt_add_executable(LumenForge
    src/main/main.cpp
    src/editor/DocumentController.cpp
    src/editor/DocumentController.hpp
)
qt_add_qml_module(LumenForge
    URI LumenForge
    VERSION 1.0
    QML_FILES
        resources/qml/Main.qml
        resources/qml/MaskCanvas.qml
)
target_link_libraries(LumenForge
    PRIVATE
        lumen_core
        Qt6::Core
        Qt6::Gui
        Qt6::Concurrent
        Qt6::Quick
        Qt6::QuickControls2
)
target_include_directories(LumenForge
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(LumenForge PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
install(TARGETS LumenForge
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)
# windeployqt — copies Qt DLLs next to the exe on Windows
find_program(WINDEPLOYQT windeployqt
    HINTS "${Qt6_DIR}/../../../bin"
          "$ENV{QTDIR}/bin")
if(WINDEPLOYQT AND WIN32)
    add_custom_command(TARGET LumenForge POST_BUILD
        COMMAND ${WINDEPLOYQT}
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/resources/qml"
            $<TARGET_FILE:LumenForge>
        COMMENT "Running windeployqt…"
        VERBATIM)
endif()
```

---

## TASK 33 — REPLACE `app/src/main/main.cpp`

```cpp
#include "editor/DocumentController.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("LumenForge");
    QGuiApplication::setOrganizationName("LumenForge");
    QGuiApplication::setApplicationVersion("0.1.0");
    QQuickStyle::setStyle("Fusion");
    DocumentController documentController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("documentController", &documentController);
    engine.loadFromModule("LumenForge", "Main");
    if (engine.rootObjects().isEmpty()) return -1;
    return QGuiApplication::exec();
}
```

---

## TASK 34 — REPLACE `CMakeLists.txt` (root)

```cmake
cmake_minimum_required(VERSION 3.24)
project(LumenForge
    VERSION 0.1.0
    DESCRIPTION "Offline-first native desktop photo editor"
    LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Qt6 6.5 REQUIRED
    COMPONENTS Core Gui Quick QuickControls2 Sql Concurrent)
qt_standard_project_setup(REQUIRES 6.5)
add_subdirectory(core)
add_subdirectory(app)
# ── CPack / WiX installer (Windows) ────────────────────────────────────────
set(CPACK_PACKAGE_NAME        "LumenForge")
set(CPACK_PACKAGE_VENDOR      "LumenForge")
set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION     "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
if(WIN32)
    set(CPACK_GENERATOR "WIX")
    set(CPACK_WIX_UPGRADE_GUID "A1B2C3D4-E5F6-7890-ABCD-EF1234567890")
    if(EXISTS "${CMAKE_SOURCE_DIR}/app/resources/icon.ico")
        set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/app/resources/icon.ico")
    endif()
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
endif()
include(CPack)
```

---

## TASK 35 — CREATE `LICENSE.txt`

```
MIT License
Copyright (c) 2024 LumenForge
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## TASK 36 — CREATE `models/.gitkeep`

```
```

(Empty file — ensures the `models/` directory is tracked by git even though `*.onnx` files are ignored.)

---

That is the complete task list — **35 files** to create or replace covering all six milestones. Execute them top to bottom. A few notes before you hand this to your agent:

**Build order for external dependencies** — before the first cmake configure after Task 27, run `vcpkg install onnxruntime libraw lcms2 opencv4` and pass `-DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake`. Each library is optional at compile time (guarded by `HAVE_*` defines), so the app builds and runs without them — AI features just won't function until the libs and model weights are present.

**Model weights** — after Task 36, manually download `mobile_sam.onnx`, `big-lama.onnx`, and `realesrgan-x4plus.onnx` into `models/`. Nothing in the build system fetches them.

**`app/resources/icon.ico`** — Task 34 references it for the installer. Create a placeholder `.ico` file or skip the CPack step until you have one; the app itself builds without it.

