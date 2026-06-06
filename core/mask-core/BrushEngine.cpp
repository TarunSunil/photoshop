#include "mask-core/BrushEngine.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QBlurEffect>

namespace lumen {

BrushEngine::BrushEngine(QSize size)
    : m_mask(size, QImage::Format_ARGB32)
{
    m_mask.fill(Qt::transparent);
}

void BrushEngine::paintStroke(QPointF center, double radius, double opacity, bool erase)
{
    if (m_mask.isNull()) {
        return;
    }

    QPainter painter(&m_mask);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Create radial gradient brush
    QRadialGradient gradient(center, radius);
    gradient.setColorAt(0, erase ? Qt::black : Qt::white);
    gradient.setColorAt(1, erase ? Qt::black : Qt::white);
    gradient.setColorAt(0.5, erase ? Qt::black : Qt::white);
    gradient.setOpacity(opacity);

    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);

    // Draw the stroke as a circle
    QPainterPath path;
    path.addEllipse(center, radius, radius);
    painter.drawPath(path);
}

void BrushEngine::feather(double radius)
{
    if (m_mask.isNull()) {
        return;
    }

    // Apply Gaussian blur using QImage convolution
    // Create a simple Gaussian kernel
    const int kernelSize = static_cast<int>(radius * 6) | 1; // Ensure odd
    const double sigma = radius;

    // Generate Gaussian kernel
    QVector<double> kernel(kernelSize);
    const double sumFactor = 1.0 / (2.0 * M_PI * sigma * sigma);
    const int halfSize = kernelSize / 2;

    double total = 0.0;
    for (int i = 0; i < kernelSize; ++i) {
        const double x = i - halfSize;
        kernel[i] = std::exp(-(x * x) / (2.0 * sigma * sigma));
        total += kernel[i];
    }

    // Normalize kernel
    for (int i = 0; i < kernelSize; ++i) {
        kernel[i] /= total;
    }

    // Apply horizontal blur
    QImage tempImage(m_mask.size(), QImage::Format_ARGB32);
    for (int y = 0; y < m_mask.height(); ++y) {
        const QRgb* srcRow = reinterpret_cast<const QRgb*>(m_mask.constScanLine(y));
        QRgb* dstRow = reinterpret_cast<QRgb*>(tempImage.scanLine(y));

        for (int x = 0; x < m_mask.width(); ++x) {
            double r = 0, g = 0, b = 0, a = 0;

            for (int k = 0; k < kernelSize; ++k) {
                const int srcX = qBound(0, x + k - halfSize, m_mask.width() - 1);
                QRgb pixel = srcRow[srcX];

                r += qRed(pixel) * kernel[k];
                g += qGreen(pixel) * kernel[k];
                b += qBlue(pixel) * kernel[k];
                a += qAlpha(pixel) * kernel[k];
            }

            dstRow[x] = qRgba(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), static_cast<int>(a));
        }
    }

    // Apply vertical blur
    for (int x = 0; x < m_mask.width(); ++x) {
        for (int y = 0; y < m_mask.height(); ++y) {
            double r = 0, g = 0, b = 0, a = 0;

            for (int k = 0; k < kernelSize; ++k) {
                const int srcY = qBound(0, y + k - halfSize, m_mask.height() - 1);
                QRgb pixel = *reinterpret_cast<const QRgb*>(tempImage.constScanLine(srcY) + x);

                r += qRed(pixel) * kernel[k];
                g += qGreen(pixel) * kernel[k];
                b += qBlue(pixel) * kernel[k];
                a += qAlpha(pixel) * kernel[k];
            }

            QRgb* dstRow = reinterpret_cast<QRgb*>(m_mask.scanLine(y));
            dstRow[x] = qRgba(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), static_cast<int>(a));
        }
    }
}

const QImage& BrushEngine::mask() const
{
    return m_mask;
}

QImage& BrushEngine::mask()
{
    return m_mask;
}

} // namespace lumen
