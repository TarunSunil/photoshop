#ifndef RENDER_PIPELINE_HPP
#define RENDER_PIPELINE_HPP

#include <QObject>
#include <QImage>

class RenderPipeline : public QObject {
    Q_OBJECT
    
public:
    explicit RenderPipeline(QObject *parent = nullptr);
    
signals:
    void renderComplete(const QImage &result);
    void progress(int value, int max);

private slots:
    QImage render(QImage image, const QString &operation);
};

#endif // RENDER_PIPELINE_HPP
