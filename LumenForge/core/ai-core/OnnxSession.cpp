#include "OnnxSession.hpp"
#include <QFile>
#include <QDebug>

OnnxSession::OnnxSession(const QString &modelPath, QObject *parent) 
    : QObject(parent), m_model(nullptr) {
    
}

bool OnnxSession::initializeModel() {
    // Load ONNX model lazily on first call
    if (!m_session) {
        QFile file(modelPath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            
            Ort::Env env(0, "LumenForge");
            auto session_options = std::make_unique<Ort::SessionOptions>();
            
            m_session = std::make_shared<Ort::Session>(env, data.data(), 
                                                        static_cast<int32_t>(data.size()), *session_options);
        } else {
            emit loadFailed("Could not open model file: " + modelPath);
        }
    }
    
    if (m_session) {
        emit modelLoaded();
    }
}
