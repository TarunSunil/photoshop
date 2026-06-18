#include "AiRuntime.hpp"
#include <QThread>
#include <QtConcurrent/QtConcurrent>
#include <QDebug>

AiRuntime::AiRuntime(QObject *parent) : QObject(parent), m_samSession(nullptr), 
                                          m_maskPredictor(nullptr), m_inpaintEngine(nullptr),
                                          m_upscaleEngine(nullptr) {
    // Load models in background thread to avoid blocking UI
}

void AiRuntime::loadMobileSAMModel() {
    if (!m_samSession) {
        emit busyChanged(true);
        
        QThread *thread = new QThread();
        this->moveToThread(thread);
        connect(this, &AiRuntime::finished, thread, &QThread::quit);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
    } else {
        emit modelLoaded();
    }
}

void AiRuntime::predictMask(const QImage &image, const QPolygonF &points, bool labelType) {
    if (m_maskPredictor && !m_isBusy) {
        m_isBusy = true;
        emit busyChanged(true);
        
        // Process in background thread for non-blocking operation
        auto result = QtConcurrent::run([this, image, points, labelType]() -> QImage {
            if (m_maskPredictor && !points.isEmpty()) {
                return m_maskPredictor->predict(image, points, labelType);
            }
            return {};
        });
        
        // Wait for result and emit signal with mask
    } else {
        qDebug() << "SAM model not loaded or currently busy";
    }
}

void AiRuntime::removeObject(const QImage &image, int pointIndex) {
    if (m_inpaintEngine && !m_isBusy) {
        m_isBusy = true;
        
        auto result = QtConcurrent::run([this, image, pointIndex]() -> QImage {
            return m_inpaintEngine->inpaint(image, pointIndex);
        });
        
        // Handle result...
    } else if (!m_inpaintEngine) {
        qDebug() << "Inpainting model not loaded";
    }
}

void AiRuntime::upscaleImage(QImage image, qreal scale) {
    if (m_upscaleEngine && !m_isBusy) {
        m_isBusy = true;
        
        auto result = QtConcurrent::run([this, image, scale]() -> QImage {
            return m_upscaleEngine->upscale(image, scale);
        });
        
        // Handle upscaled result...
    } else if (!m_upscaleEngine) {
        qDebug() << "Upscaling model not loaded";
    }
}
