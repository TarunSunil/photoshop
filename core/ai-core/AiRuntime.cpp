#include "ai-core/AiRuntime.hpp"

#include <QFileInfo>

namespace lumen {

AiRuntime::AiRuntime(QObject* parent)
    : QObject(parent)
{
}

bool AiRuntime::isModelAvailable(const QString& modelId) const
{
    return QFileInfo::exists(modelId);
}

AiBackend AiRuntime::activeBackend() const
{
    return m_backend;
}

} // namespace lumen
