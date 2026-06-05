#include "export-core/ExportService.hpp"

namespace lumen {

ExportService::ExportService(QObject* parent)
    : QObject(parent)
{
}

bool ExportService::exportImage(const DocumentModel& document, const QString& path, int quality) const
{
    const QImage rendered = m_renderPipeline.renderFullResolution(document);
    if (rendered.isNull()) {
        return false;
    }
    return rendered.save(path, nullptr, quality);
}

} // namespace lumen
