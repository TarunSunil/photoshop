#include "mask-core/BrushEngine.hpp"
#include <QPainter>
#include <QRadialGradient>
#include <cmath>
namespace lumen {
BrushEngine::BrushEngine(QSize size)
    : m_mask(size, QImage::Format_ARGB32)
{
    m_mask.fill(Qt::transparent);
}
void BrushEngine::paintStroke(QPointF center, double radius, double opacity, bool erase)
{
    if (m_mask.isNull()) return;
    QPainter painter(&m_mask);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(
        erase ? QPainter::CompositionMode_Clear
              : QPainter::CompositionMode_SourceOver);
    painter.setOpacity(opacity);
    QRadialGradient gradient(center, radius);
    gradient.setColorAt(0.0, Qt::white);
    gradient.setColorAt(0.75, Qt::white);
    gradient.setColorAt(1.0, Qt::transparent);
    painter.setBrush(QBrush(gradient));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(center, radius, radius);
}
void BrushEngine::feather(double radius)
{
    if (m_mask.isNull() || radius < 1.0) return;
    const int kernelSize = qMax(3, (static_cast<int>(radius * 6)) | 1);
    const double sigma   = radius;
    const int halfSize   = kernelSize / 2;
    QVector<double> kernel(kernelSize);
    double total = 0.0;
    for (int i = 0; i < kernelSize; ++i) {
        const double x = i - halfSize;
        kernel[i] = std::exp(-(x * x) / (2.0 * sigma * sigma));
        total += kernel[i];
    }
    for (int i = 0; i < kernelSize; ++i) kernel[i] /= total;
    QImage tmp(m_mask.size(), QImage::Format_ARGB32);
    tmp.fill(Qt::transparent);
    for (int y = 0; y < m_mask.height(); ++y) {
        const auto* src = reinterpret_cast<const QRgb*>(m_mask.constScanLine(y));
        auto*       dst = reinterpret_cast<QRgb*>(tmp.scanLine(y));
        for (int x = 0; x < m_mask.width(); ++x) {
            double r = 0, g = 0, b = 0, a = 0;
            for (int k = 0; k < kernelSize; ++k) {
                const int sx = qBound(0, x + k - halfSize, m_mask.width() - 1);
                const QRgb p = src[sx];
                r += qRed(p)   * kernel[k];
                g += qGreen(p) * kernel[k];
                b += qBlue(p)  * kernel[k];
                a += qAlpha(p) * kernel[k];
            }
            dst[x] = qRgba(int(r), int(g), int(b), int(a));
        }
    }
    for (int x = 0; x < m_mask.width(); ++x) {
        for (int y = 0; y < m_mask.height(); ++y) {
            double r = 0, g = 0, b = 0, a = 0;
            for (int k = 0; k < kernelSize; ++k) {
                const int sy = qBound(0, y + k - halfSize, m_mask.height() - 1);
                const QRgb p = reinterpret_cast<const QRgb*>(tmp.constScanLine(sy))[x];
                r += qRed(p)   * kernel[k];
                g += qGreen(p) * kernel[k];
                b += qBlue(p)  * kernel[k];
                a += qAlpha(p) * kernel[k];
            }
            reinterpret_cast<QRgb*>(m_mask.scanLine(y))[x] = qRgba(int(r), int(g), int(b), int(a));
        }
    }
}
void BrushEngine::clear()
{
    m_mask.fill(Qt::transparent);
}
void BrushEngine::resize(QSize size)
{
    if (m_mask.size() == size) return;
    QImage next(size, QImage::Format_ARGB32);
    next.fill(Qt::transparent);
    QPainter p(&next);
    p.drawImage(QRect(QPoint(0,0), size),
                m_mask.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    m_mask = next;
}
const QImage& BrushEngine::mask() const { return m_mask; }
QImage&       BrushEngine::mask()       { return m_mask; }
} // namespace lumen