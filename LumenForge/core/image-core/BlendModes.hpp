#ifndef BLEND_MODES_HPP
#define BLEND_MODES_HPP

#include <QImage>
#include <QObject>

class BlendModes : public QObject {
    Q_OBJECT
    
public:
    enum class Mode { Normal, Multiply, Screen, Overlay, SoftLight, HardLight, Difference };
    
signals:
    void blendComplete(const QImage &result);

private slots:
    QImage applyBlendMode(QImage image, const QImage& layer, Mode mode = Mode::Normal);
};

#endif // BLEND_MODES_HPP
