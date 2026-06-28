#pragma once
#include "ai-core/OnnxSession.hpp"
#include <QImage>
#include <QPointF>

namespace lumen {

// Wraps a MobileSAM ONNX model exported by the `samexporter` tool.
// That export combines the ViT-tiny encoder and SAM decoder into one graph
// so a single OnnxSession::run() call handles the full encode → decode pipeline.
//
// Expected model file:  models/mobile_sam.onnx
// How to build it:
//   pip install samexporter
//   python -m samexporter.export_sam \
//       --model-type vit_t \
//       --checkpoint mobile_sam.pt \
//       --output models/mobile_sam.onnx --overwrite
// MobileSAM weights: https://github.com/ChaoningZhang/MobileSAM (mobile_sam.pt)
class MaskPredictor {
public:
    MaskPredictor();

    // Returns an ARGB32 mask the same size as `source`.
    // White pixels (alpha=255) are inside the predicted object;
    // transparent pixels (alpha=0) are outside.
    // Returns a null QImage on failure; check lastError() for details.
    [[nodiscard]] QImage predict(const QImage& source, QPointF promptPoint);

    [[nodiscard]] QString lastError() const { return m_session.lastError(); }

private:
    OnnxSession m_session;
    // SAM / MobileSAM uses a fixed 1024×1024 input resolution.
    static constexpr int MODEL_SIZE = 1024;
};

} // namespace lumen
