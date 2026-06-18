#pragma once
#include <QImage>
#include <QString>
namespace lumen {
class ColorManager {
public:
    ColorManager();
    \~ColorManager();
    bool loadProfiles(const QString& inputProfile, const QString& outputProfile);
    [[nodiscard]] bool hasTransform() const;
    void applyTransform(QImage& image) const;
private:
    void* m_inputProfile  = nullptr;
    void* m_outputProfile = nullptr;
    void* m_transform     = nullptr;
};
} // namespace lumen