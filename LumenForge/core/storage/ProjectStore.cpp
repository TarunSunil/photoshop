#include "ProjectStore.hpp"
#include <QFile>
#include <QDebug>

ProjectStore::ProjectStore(QObject *parent) : QObject(parent), m_layers(new QList<Layer*>()) {
}

bool ProjectStore::saveProject(const QString &filePath, const QList<Layer*> layers) {
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit loadComplete("Could not open file for writing");
        return false;
    }
    
    QTextStream out(&file);
    // ... serialize project data to JSON or XML here
    
    delete m_layers;
}

bool ProjectStore::loadProject(const QString &filePath) {
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit loadComplete("Could not open file for reading");
        return false;
    }
    
    QTextStream in(&file);
    // ... deserialize project data from JSON or XML here
    
}
