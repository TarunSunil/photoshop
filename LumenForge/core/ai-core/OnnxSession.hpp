#ifndef ONNX_SESSION_HPP
#define ONNX_SESSION_HPP

#include <onnxruntime_cxx_api.h>
#include <QImage>
#include <QObject>

class OnnxSession : public QObject {
    Q_OBJECT
    
public:
    explicit OnnxSession(const QString &modelPath, QObject *parent = nullptr);
    
signals:
    void modelLoaded();
    void loadFailed(QString error);
    
private slots:
    bool initializeModel();
};

#endif // ONNX_SESSION_HPP
