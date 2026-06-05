#include "image-core/RenderPipeline.hpp"

#include <QtMath>

namespace lumen {

namespace {

quint16 clamp16(double value)
{
    return static_cast<quint16>(qBound(0.0, value, 65535.0));
}

} // namespace

QImage RenderPipeline::renderPreview(const DocumentModel& document, QSize maximumSize) const
{
    if (!document.hasDocument()) {
        return {};
    }

    QImage source = document.sourceImage();
    if (maximumSize.isValid()
        && (source.width() > maximumSize.width() || source.height() > maximumSize.height())) {
        source = source.scaled(maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return applyAdjustments(source, document);
}

QImage RenderPipeline::renderFullResolution(const DocumentModel& document) const
{
    if (!document.hasDocument()) {
        return {};
    }
    return applyAdjustments(document.sourceImage(), document);
}

QImage RenderPipeline::applyAdjustments(QImage image, const DocumentModel& document) const
{
    image = image.convertToFormat(QImage::Format_RGBA64);

    const double exposureStops = document.scalarAdjustment(AdjustmentType::Exposure);
    const double contrast = document.scalarAdjustment(AdjustmentType::Contrast);
    const double saturation = document.scalarAdjustment(AdjustmentType::Saturation);
    const double temperature = document.scalarAdjustment(AdjustmentType::Temperature);
    const double tint = document.scalarAdjustment(AdjustmentType::Tint);

    const double exposureGain = qPow(2.0, exposureStops);
    const double contrastGain = 1.0 + (contrast / 100.0);
    const double saturationGain = 1.0 + (saturation / 100.0);
    const double redBalance = 1.0 + (temperature / 400.0);
    const double blueBalance = 1.0 - (temperature / 400.0);
    const double greenBalance = 1.0 + (tint / 400.0);

    for (int y = 0; y < image.height(); ++y) {
        auto* scanline = reinterpret_cast<QRgba64*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            QRgba64 pixel = scanline[x];
            double r = pixel.red() * exposureGain * redBalance;
            double g = pixel.green() * exposureGain * greenBalance;
            double b = pixel.blue() * exposureGain * blueBalance;

            r = ((r / 65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            g = ((g / 65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            b = ((b / 65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;

            const double luma = (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
            r = luma + ((r - luma) * saturationGain);
            g = luma + ((g - luma) * saturationGain);
            b = luma + ((b - luma) * saturationGain);

            scanline[x] = QRgba64::fromRgba64(clamp16(r), clamp16(g), clamp16(b), pixel.alpha());
        }
    }

    return image;
}
