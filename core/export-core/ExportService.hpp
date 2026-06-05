#pragma once

#include "editor-core/DocumentModel.hpp"
#include "image-core/RenderPipeline.hpp"

#include <QObject>
#include <QString>

namespace lumen {

class ExportService final : public QObject {
    Q_OBJECT

public:
    explicit ExportService(QObject* parent = nullptr);

    [[nodiscard]] bool exportImage(const DocumentModel& document, const QString& path, int quality = 92) const;

private:
    RenderPipeline m_renderPipeline;
};

} // namespace lumen
