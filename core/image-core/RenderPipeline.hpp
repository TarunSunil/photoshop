#pragma once

#include "editor-core/DocumentModel.hpp"

#include <QImage>
#include <QSize>
#include <QVector>

namespace lumen {

class RenderPipeline {
public:
    [[nodiscard]] QImage renderPreview(const DocumentModel& document, QSize maximumSize) const;
    [[nodiscard]] QImage renderPreviewFromData(const QImage& source,
                                               const QVector<Adjustment>& adjustments,
                                               QSize maximumSize) const;
    [[nodiscard]] QImage renderFullResolution(const DocumentModel& document) const;

private:
    [[nodiscard]] QImage applyAdjustments(QImage image, const QVector<Adjustment>& adjustments) const;
};

} // namespace lumen
