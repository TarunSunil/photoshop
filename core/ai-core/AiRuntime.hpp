#pragma once

#include <QObject>
#include <QString>

namespace lumen {

enum class AiBackend {
    Cpu,
    DirectMl,
    Cuda,
    CoreMl
};

class AiRuntime final : public QObject {
    Q_OBJECT

public:
    explicit AiRuntime(QObject* parent = nullptr);

    [[nodiscard]] bool isModelAvailable(const QString& modelId) const;
    [[nodiscard]] AiBackend activeBackend() const;

signals:
    void progressChanged(QString jobId, double progress);

private:
    AiBackend m_backend = AiBackend::Cpu;
};

} // namespace lumen
