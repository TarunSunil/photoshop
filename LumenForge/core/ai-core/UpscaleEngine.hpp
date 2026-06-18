#ifndef UPSCALE_ENGINE_HPP
#define UPSCALE_ENGINE_HPP

#include <QImage>
#include <QObject>
#include "OnnxSession.hpp"

class UpscaleEngine : public QObject {
    Q_OBJECT
    
public:
    explicit UpscaleEngine(Ort::Env &env, Ort::Session &session, 
                          const std::vector<const char*> &inputNames,
                          const std::vector<std::pair<const char*, int>>& inputShapes,
                          qreal scale = 4.0f, QObject *parent = nullptr);
    
signals:
    void upscaleReady(const QImage &result);

private slots:
    QImage upscale(QImage image, qreal scale = 4.0f);
};

#endif // UPSCALE_ENGINE_HPP
