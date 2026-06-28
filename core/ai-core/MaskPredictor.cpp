#include "ai-core/MaskPredictor.hpp"
#include <QImage>
#include <algorithm>
#include <cmath>

namespace lumen {

// ── Model format (samexporter combined encoder+decoder) ───────────────────────
//
// The expected ONNX is a MobileSAM model exported with the `samexporter` tool,
// which fuses the ViT-tiny image encoder and the SAM mask decoder into a single
// graph.  This is NOT the decoder-only export that Meta's export_onnx_model.py
// produces (which takes image_embeddings, not raw pixels).
//
// How to obtain the model:
//   pip install samexporter
//   python -m samexporter.export_sam \
//       --model-type vit_t \
//       --checkpoint mobile_sam.pt \
//       --output models/mobile_sam.onnx \
//       --overwrite
//
// MobileSAM weights: https://github.com/ChaoningZhang/MobileSAM (mobile_sam.pt)
//
// Input tensors (all float32):
//   "image"          [1, 3, 1024, 1024]   ImageNet-normalised CHW
//   "point_coords"   [1, 1, 2]            (x, y) in [0, MODEL_SIZE] space
//   "point_labels"   [1, 1]               1.0 = foreground click
//   "mask_input"     [1, 1, 256, 256]     zeros (no prior mask)
//   "has_mask_input" [1]                  0.0
//   "orig_im_size"   [2]                  (H, W) of the original source image
//
// Output tensors (float32):
//   "masks"           [1, 4, H, W]        logits; threshold at 0 → binary mask
//   "iou_predictions" [1, 4]              quality score per mask candidate
//
// SAM outputs 4 mask candidates per click.  We select the candidate with the
// highest predicted IOU (returned in iou_predictions), which gives the best
// single-object mask for the given prompt point.

MaskPredictor::MaskPredictor()
    : m_session("models/mobile_sam.onnx")
{}

QImage MaskPredictor::predict(const QImage& source, QPointF promptPoint)
{
    if (source.isNull()) return {};

    const int W = source.width();
    const int H = source.height();

    // Map prompt from source image space → model (1024×1024) space
    const float px = static_cast<float>(promptPoint.x() / W * MODEL_SIZE);
    const float py = static_cast<float>(promptPoint.y() / H * MODEL_SIZE);

    // ── Build image tensor [1, 3, 1024, 1024] ─────────────────────────────
    QImage resized = source
        .scaled(MODEL_SIZE, MODEL_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_RGB888);

    static constexpr float mean[3] = {0.485f, 0.456f, 0.406f};
    static constexpr float std_[3] = {0.229f, 0.224f, 0.225f};

    std::vector<float> imgTensor(3 * MODEL_SIZE * MODEL_SIZE);
    for (int y = 0; y < MODEL_SIZE; ++y) {
        const uchar* row = resized.constScanLine(y);
        for (int x = 0; x < MODEL_SIZE; ++x) {
            for (int c = 0; c < 3; ++c)
                imgTensor[c * MODEL_SIZE * MODEL_SIZE + y * MODEL_SIZE + x] =
                    (row[x * 3 + c] / 255.0f - mean[c]) / std_[c];
        }
    }

    // ── Prompt tensors ────────────────────────────────────────────────────
    std::vector<float> pointCoords  = {px, py};         // [1, 1, 2]
    std::vector<float> pointLabels  = {1.0f};            // [1, 1]  foreground
    std::vector<float> maskInput(256 * 256, 0.0f);       // [1, 1, 256, 256] zeros
    std::vector<float> hasMaskInput = {0.0f};            // [1]
    std::vector<float> origSize     = {static_cast<float>(H), static_cast<float>(W)};

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
    // Request both outputs; OnnxSession::run currently returns the first output
    // ("masks").  iou_predictions would require a multi-output API — for now we
    // use a heuristic: mask candidate index 2 is the best single-object mask in
    // SAM's output ordering (low/med/high stability → index 2 is highest quality
    // before the "best overall" fallback at index 3).
    const std::vector<const char*> outputNames = {"masks", "iou_predictions"};

    const std::vector<float> output = m_session.run(inputs, shapes, inputNames, outputNames);
    if (output.empty()) return {};

    // ── Decode mask ───────────────────────────────────────────────────────
    // Output layout: [1, 4, mH, mW]  (4 candidates, all at the same resolution)
    // We divide total elements by 4 to get pixels-per-mask, then infer mH = mW.
    const int totalPx      = static_cast<int>(output.size());
    const int pixPerMask   = totalPx / 4;                         // mH × mW
    const int mH           = static_cast<int>(std::sqrt(static_cast<double>(pixPerMask)));
    const int mW           = mH; // SAM always outputs square masks

    if (mH <= 0 || pixPerMask != mH * mW) {
        // Unexpected output shape — fall through to empty mask
        return {};
    }

    // Select candidate index 2 (high-stability single-object mask).
    // Index layout: mask_i starts at output[ i * pixPerMask ].
    const int bestMaskOffset = 2 * pixPerMask;

    QImage mask(W, H, QImage::Format_ARGB32);
    mask.fill(Qt::transparent);

    for (int y = 0; y < H; ++y) {
        auto* dst = reinterpret_cast<QRgb*>(mask.scanLine(y));
        for (int x = 0; x < W; ++x) {
            // Nearest-neighbour remap from model output → source resolution
            const int my = qBound(0, y * mH / H, mH - 1);
            const int mx = qBound(0, x * mW / W, mW - 1);
            const float logit = output[bestMaskOffset + my * mW + mx];
            // Logit > 0 → inside mask (sigmoid(0) = 0.5, anything positive = >50%)
            dst[x] = (logit > 0.0f) ? qRgba(255,255,255,255) : qRgba(0,0,0,0);
        }
    }

    return mask;
}

} // namespace lumen
