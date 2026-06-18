#include "DocumentModel.hpp"
#include <QList>
#include <QDebug>

DocumentModel::DocumentModel(QObject *parent) : QObject(parent), m_layers(new QList<Layer*>()) {
}

void DocumentModel::addLayer(const QString &type, const QImage &image) {
    Layer* layer = new Layer(type, image);
    m_layers->append(layer);
    
    emit layerAdded(layer);
    emit documentChanged();
}

void DocumentModel::applyAdjustment(int adjustmentId) {
    // Apply tone curve, noise reduction, HSL adjustments to layers
    if (adjustmentId >= 0 && adjustmentId < static_cast<int>(m_layers->count())) {
        Layer* layer = m_layers[adjustmentId];
        // ... apply specific adjustment logic here
    }
    
    emit documentChanged();
}
