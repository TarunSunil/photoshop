#ifndef AI_RUNTIME_HPP
#define AI_RUNTIME_HPP

#include <QObject>
#include <QFutureWatcher>
#include "OnnxSession.hpp"
#include "MaskPredictor.hpp"
#include "InpaintEngine.hpp"
#include "UpscaleEngine.hpp"

class AiRuntime : public QObject {
    Q_OBJECT
    
public:
    explicit AiRuntime(QObject *parent = nullptr);
    
signals:
    void modelLoaded();
    void maskReady(const QImage &mask);
    void busyChanged(bool isBusy);
    
private slots:
    void loadMobileSAMModel();
    void predictMask(const QImage &image, const QPolygonF &points, bool labelType = 1);
    void removeObject(const QImage &image, int pointIndex = -1);
    void upscaleImage(QImage image, qreal scale = 4.0f);

private:
    OnnxSession *m_samSession;      // MobileSAM for segmentation
    MaskPredictor *m_maskPredictor; // SAM prediction wrapper
    InpaintEngine *m_inpaintEngine;  // LaMa inpainting
    UpscaleEngine *m_upscaleEngine;  // Real-ESRGAN upscaling
    
    bool m_isBusy = false;
};

#endif // AI_RUNTIME_HPP
