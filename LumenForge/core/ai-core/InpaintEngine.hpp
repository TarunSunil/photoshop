#ifndef INPAINT_ENGINE_HPP
#define INPAINT_ENGINE_HPP

#include <QImage>
#include <QObject>
#include "OnnxSession.hpp"

class InpaintEngine : public QObject {
    Q_OBJECT
    
public:
    explicit InpaintEngine(Ort::Env &env, Ort::Session &session, 
                          const std::vector<const char*> &inputNames,
                          const std::vector<std::pair<const char*, int>>& inputShapes,
                          QObject *parent = nullptr);
    
signals:
    void inpaintReady(const QImage &result);

private slots:
    QImage inpaint(const QImage &image, int pointIndex);
};

#endif // INPAINT_ENGINE_HPP
