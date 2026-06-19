#pragma once
#include <QImage>
#include <QString>
#include <QStringList>
namespace lumen {
class RawImporter {
public:
    [[nodiscard]] static QStringList supportedExtensions();
    [[nodiscard]] QImage load(const QString& path);
};
} // namespace lumen