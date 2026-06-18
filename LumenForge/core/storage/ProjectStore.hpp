#ifndef PROJECT_STORE_HPP
#define PROJECT_STORE_HPP

#include <QObject>
#include "Layer.hpp"
#include "Mask.hpp"

class ProjectStore : public QObject {
    Q_OBJECT
    
public:
    explicit ProjectStore(QObject *parent = nullptr);
    
signals:
    void saveComplete();
    void loadComplete(const QString &error);

private slots:
    bool saveProject(const QString &filePath, const QList<Layer*> layers);
    bool loadProject(const QString &filePath);
};

#endif // PROJECT_STORE_HPP
