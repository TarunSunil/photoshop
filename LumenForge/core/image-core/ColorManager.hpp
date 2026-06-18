#ifndef COLOR_MANAGER_HPP
#define COLOR_MANAGER_HPP

#include <QImage>
#include <QObject>

class ColorManager : public QObject {
    Q_OBJECT
    
public:
    explicit ColorManager(QObject *parent = nullptr);
    
signals:
    void colorTransformComplete(const QImage &result, const QString &error);

private slots:
    QImage applyColorProfile(QImage image, const QString &profilePath);
};

#endif // COLOR_MANAGER_HPP
