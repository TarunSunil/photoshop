#pragma once

#include "editor-core/DocumentModel.hpp"

#include <QImage>
#include <QSize>

namespace lumen {

class RenderPipeline {
public:
    [[nodiscard]] QImage renderPreview(const DocumentModel& document, QSize maximumSize) const;
    [[nodiscard]] QImage renderFullResolution(const DocumentModel& document) const;

private:
    [[nodiscard]] QImage applyAdjustments(QImage image, const DocumentModel& document) const;
};

} // namespace lumen
