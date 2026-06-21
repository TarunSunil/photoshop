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
    // Callback now also receives an error string (empty on success).
    // Previously only the QImage came back, so a failed prediction
    // (missing model, ONNX Runtime not compiled in, inference
    // exception) looked identical to a successful one that happened to
    // produce nothing -- the caller had no way to tell the user why.
    void predictMask(const QImage& source, QPointF point,
                     std::function<void(QImage, QString)> callback);
signals:
    void progressChanged(QString jobId, double progress);
    void busyChanged(bool busy);
private:
    AiBackend m_backend = AiBackend::Cpu;
};
} // namespace lumen
