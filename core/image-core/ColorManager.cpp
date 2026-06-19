#include "image-core/ColorManager.hpp"
#ifdef HAVE_LCMS2
#  include <lcms2.h>
#endif
namespace lumen {
ColorManager::ColorManager()  = default;
ColorManager::~ColorManager()
{
#ifdef HAVE_LCMS2
    if (m_transform)     cmsDeleteTransform(static_cast<cmsHTRANSFORM>(m_transform));
    if (m_inputProfile)  cmsCloseProfile(static_cast<cmsHPROFILE>(m_inputProfile));
    if (m_outputProfile) cmsCloseProfile(static_cast<cmsHPROFILE>(m_outputProfile));
#endif
}
bool ColorManager::loadProfiles(const QString& inputProfile, const QString& outputProfile)
{
#ifdef HAVE_LCMS2
    if (m_transform)     cmsDeleteTransform(static_cast<cmsHTRANSFORM>(m_transform));
    if (m_inputProfile)  cmsCloseProfile(static_cast<cmsHPROFILE>(m_inputProfile));
    if (m_outputProfile) cmsCloseProfile(static_cast<cmsHPROFILE>(m_outputProfile));
    m_transform = m_inputProfile = m_outputProfile = nullptr;
    cmsHPROFILE in  = cmsOpenProfileFromFile(inputProfile.toLocal8Bit().constData(),  "r");
    cmsHPROFILE out = cmsOpenProfileFromFile(outputProfile.toLocal8Bit().constData(), "r");
    if (!in || !out) {
        if (in)  cmsCloseProfile(in);
        if (out) cmsCloseProfile(out);
        return false;
    }
    cmsHTRANSFORM xf = cmsCreateTransform(
        in,  TYPE_RGBA_16,
        out, TYPE_RGBA_16,
        INTENT_PERCEPTUAL, 0);
    if (!xf) {
        cmsCloseProfile(in);
        cmsCloseProfile(out);
        return false;
    }
    m_inputProfile  = in;
    m_outputProfile = out;
    m_transform     = xf;
    return true;
#else
    Q_UNUSED(inputProfile) Q_UNUSED(outputProfile)
    return false;
#endif
}
bool ColorManager::hasTransform() const { return m_transform != nullptr; }
void ColorManager::applyTransform(QImage& image) const
{
#ifdef HAVE_LCMS2
    if (!m_transform) return;
    image = image.convertToFormat(QImage::Format_RGBA64);
    for (int y = 0; y < image.height(); ++y) {
        void* line = image.scanLine(y);
        cmsDoTransformLineStride(
            static_cast<cmsHTRANSFORM>(m_transform),
            line, line, image.width(), 1,
            image.bytesPerLine(), image.bytesPerLine(), 0, 0);
    }
#else
    Q_UNUSED(image)
#endif
}
} // namespace lumen