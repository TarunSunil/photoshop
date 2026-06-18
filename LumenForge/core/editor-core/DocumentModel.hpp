#ifndef DOCUMENT_MODEL_HPP
#define DOCUMENT_MODEL_HPP

#include <QObject>
#include "Mask.hpp"
#include "Layer.hpp"

class DocumentModel : public QObject {
    Q_OBJECT
    
public:
    explicit DocumentModel(QObject *parent = nullptr);
    
signals:
    void documentChanged();
    void layerAdded(Layer* layer);
    void maskUpdated(Mask* mask);

private slots:
    void addLayer(const QString &type, const QImage &image);
    void applyAdjustment(int adjustmentId);
};

#endif // DOCUMENT_MODEL_HPP
