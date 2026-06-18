#ifndef RAW_IMPORTER_HPP
#define RAW_IMPORTER_HPP

#include <QImage>
#include <QObject>

class RawImporter : public QObject {
    Q_OBJECT
    
public:
    explicit RawImporter(QObject *parent = nullptr);
    
signals:
    void importComplete(const QImage &image, const QString &error);

private slots:
    QImage importRawFile(const QString &filePath);
};

#endif // RAW_IMPORTER_HPP
