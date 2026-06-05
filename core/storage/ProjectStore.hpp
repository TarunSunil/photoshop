#pragma once

#include "editor-core/DocumentModel.hpp"

#include <QObject>
#include <QString>

namespace lumen {

class ProjectStore final : public QObject {
    Q_OBJECT

public:
    explicit ProjectStore(QObject* parent = nullptr);

    [[nodiscard]] bool saveProject(const DocumentModel& document, const QString& path) const;
    [[nodiscard]] bool loadProject(DocumentModel& document, const QString& path) const;
};

} // namespace lumen
