#pragma once

#include <QImage>
#include <QPoint>
#include <QSize>

namespace lumen {

class BrushEngine {
public:
    explicit BrushEngine(QSize size);

    void paintStroke(QPointF center, double radius, double opacity, bool erase);
    void feather(double radius);

    [[nodiscard]] const QImage& mask() const;
    [[nodiscard]] QImage& mask();

private:
    QImage m_mask;
};

} // namespace lumen
