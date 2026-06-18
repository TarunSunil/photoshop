#ifndef MASK_PREDICTOR_HPP
#define MASK_PREDICTOR_HPP

#include <QImage>
#include <QObject>
#include "OnnxSession.hpp"

class MaskPredictor : public QObject {
    Q_OBJECT
    
public:
    explicit MaskPredictor(Ort::Env &env, Ort::Session &session, 
                          const std::vector<const char*> &inputNames,
                          const std::vector<std::pair<const char*, int>>& inputShapes,
                          QObject *parent = nullptr);
    
signals:
    void predictionReady(const QImage &mask);

private slots:
    QImage predict(const QImage &image, const QPolygonF &points, bool labelType);
};

#endif // MASK_PREDICTOR_HPP
