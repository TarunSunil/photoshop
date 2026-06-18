# PART 2 of 4 — AI core — ONNX runtime, mask predictor, inpaint, upscale
# Tasks 11–20 (10 files)

## Checklist (mark [x] as each file is completed)
- [ ] Task 11: CREATE `core/ai-core/OnnxSession.hpp`
- [ ] Task 12: CREATE `core/ai-core/OnnxSession.cpp`
- [ ] Task 13: CREATE `core/ai-core/MaskPredictor.hpp`
- [ ] Task 14: CREATE `core/ai-core/MaskPredictor.cpp`
- [ ] Task 15: CREATE `core/ai-core/InpaintEngine.hpp`
- [ ] Task 16: CREATE `core/ai-core/InpaintEngine.cpp`
- [ ] Task 17: CREATE `core/ai-core/UpscaleEngine.hpp`
- [ ] Task 18: CREATE `core/ai-core/UpscaleEngine.cpp`
- [ ] Task 19: REPLACE `core/ai-core/AiRuntime.hpp`
- [ ] Task 20: REPLACE `core/ai-core/AiRuntime.cpp`

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
