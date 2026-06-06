#include "image-core/RenderPipeline.hpp"

#include <QTransform>
#include <QtMath>

namespace lumen {

namespace {

quint16 clamp16(double value)
{
    return static_cast<quint16>(qBound(0.0, value, 65535.0));
}

double scalarAdjustment(const QVector<Adjustment>& adjustments, AdjustmentType type)
{
    for (const Adjustment& adjustment : adjustments) {
        if (adjustment.type == type && adjustment.enabled) {
            return adjustment.parameters.value("value").toDouble(0.0);
        }
    }
    return 0.0;
}

} // namespace

QImage RenderPipeline::renderPreview(const DocumentModel& document, QSize maximumSize) const
{
    if (!document.hasDocument()) {
        return {};
    }

    return renderPreviewFromData(document.sourceImage(), document.adjustments(), maximumSize);
}

QImage RenderPipeline::renderPreviewFromData(const QImage& sourceImage,
                                             const QVector<Adjustment>& adjustments,
                                             QSize maximumSize) const
{
    if (sourceImage.isNull()) {
        return {};
    }

    QImage source = sourceImage;
    if (maximumSize.isValid()
        && (source.width() > maximumSize.width() || source.height() > maximumSize.height())) {
        source = source.scaled(maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return applyAdjustments(source, adjustments);
}

QImage RenderPipeline::renderFullResolution(const DocumentModel& document) const
{
    if (!document.hasDocument()) {
        return {};
    }
    return applyAdjustments(document.sourceImage(), document.adjustments());
}

QImage RenderPipeline::applyAdjustments(QImage image, const QVector<Adjustment>& adjustments) const
{
    image = image.convertToFormat(QImage::Format_RGBA64);

    const int rotation = static_cast<int>(scalarAdjustment(adjustments, AdjustmentType::RotationDegrees)) % 360;
    const bool flipHorizontal = scalarAdjustment(adjustments, AdjustmentType::FlipHorizontal) > 0.5;
    const bool flipVertical = scalarAdjustment(adjustments, AdjustmentType::FlipVertical) > 0.5;

    if (flipHorizontal || flipVertical) {
        image = image.mirrored(flipHorizontal, flipVertical);
    }

    if (rotation != 0) {
        QTransform transform;
        transform.rotate(rotation);
        image = image.transformed(transform, Qt::SmoothTransformation);
    }

    const double exposureStops = scalarAdjustment(adjustments, AdjustmentType::Exposure);
    const double contrast = scalarAdjustment(adjustments, AdjustmentType::Contrast);
    const double highlights = scalarAdjustment(adjustments, AdjustmentType::Highlights) / 100.0;
    const double shadows = scalarAdjustment(adjustments, AdjustmentType::Shadows) / 100.0;
    const double whites = scalarAdjustment(adjustments, AdjustmentType::Whites) / 100.0;
    const double blacks = scalarAdjustment(adjustments, AdjustmentType::Blacks) / 100.0;
    const double saturation = scalarAdjustment(adjustments, AdjustmentType::Saturation);
    const double vibrance = scalarAdjustment(adjustments, AdjustmentType::Vibrance) / 100.0;
    const double temperature = scalarAdjustment(adjustments, AdjustmentType::Temperature);
    const double tint = scalarAdjustment(adjustments, AdjustmentType::Tint);

    const double exposureGain = qPow(2.0, exposureStops);
    const double whitesGain = 1.0 + (whites * 0.5);
    const double blacksOffset = blacks * 0.15 * 65535.0;
    const double contrastGain = 1.0 + (contrast / 100.0);
    const double saturationGain = 1.0 + (saturation / 100.0);
    const double redBalance = 1.0 + (temperature / 400.0);
    const double blueBalance = 1.0 - (temperature / 400.0);
    const double greenBalance = 1.0 + (tint / 400.0);

    for (int y = 0; y < image.height(); ++y) {
        auto* scanline = reinterpret_cast<QRgba64*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            QRgba64 pixel = scanline[x];
            double r = pixel.red() * exposureGain;
            double g = pixel.green() * exposureGain;
            double b = pixel.blue() * exposureGain;

            r = (r * whitesGain) + blacksOffset;
            g = (g * whitesGain) + blacksOffset;
            b = (b * whitesGain) + blacksOffset;

            r = ((r / 65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            g = ((g / 65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            b = ((b / 65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;

            double luma = ((0.2126 * r) + (0.7152 * g) + (0.0722 * b)) / 65535.0;
            if (luma > 0.5) {
                const double blend = (luma - 0.5) * 2.0;
                const double gain = 1.0 + (highlights * blend);
                r *= gain;
                g *= gain;
                b *= gain;
            }

            luma = ((0.2126 * r) + (0.7152 * g) + (0.0722 * b)) / 65535.0;
            if (luma < 0.5) {
                const double blend = (0.5 - luma) * 2.0;
                const double gain = 1.0 + (shadows * blend);
                r *= gain;
                g *= gain;
                b *= gain;
            }

            const double luma16 = (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
            r = luma16 + ((r - luma16) * saturationGain);
            g = luma16 + ((g - luma16) * saturationGain);
            b = luma16 + ((b - luma16) * saturationGain);

            if (m_mask.isValid()) {
                const double maskLuma = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 65535.0;
                r = (r * (1.0 - maskLuma) + (r * maskLuma)) ; // Simplified logic for now, just applying the mask influence
            }

            const double satMax = qMax(r, qMax(g, b));
            const double satMin = qMin(r, qMin(g, b));
            const double colorfulness = (satMax - satMin) / (satMax + 1.0e-6);
            const double vibranceGain = 1.0 + (vibrance * (1.0 - colorfulness));
            r = luma16 + ((r - luma16) * vibranceGain);
            g = luma16 + ((g - luma16) * vibranceGain);
            b = luma16 + ((b - luma16) * vibranceGain);

            r *= redBalance;
            g *= greenBalance;
            b *= blueBalance;

            scanline[x] = QRgba64::fromRgba64(clamp16(r), clamp16(g), clamp16(b), pixel.alpha());
        }
    }

    return image;
}
