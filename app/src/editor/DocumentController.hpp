#pragma once

#include "editor-core/DocumentModel.hpp"
#include "export-core/ExportService.hpp"
#include "image-core/RenderPipeline.hpp"
#include "storage/ProjectStore.hpp"

#include <QObject>
#include <QUrl>

class DocumentController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY documentChanged)
    Q_PROPERTY(QString imageUrl READ imageUrl NOTIFY previewChanged)
    Q_PROPERTY(double exposure READ exposure WRITE setExposure NOTIFY adjustmentsChanged)
    Q_PROPERTY(double contrast READ contrast WRITE setContrast NOTIFY adjustmentsChanged)
    Q_PROPERTY(double saturation READ saturation WRITE setSaturation NOTIFY adjustmentsChanged)
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY adjustmentsChanged)
    Q_PROPERTY(double tint READ tint WRITE setTint NOTIFY adjustmentsChanged)

public:
    explicit DocumentController(QObject* parent = nullptr);

    [[nodiscard]] bool hasDocument() const;
    [[nodiscard]] QString sourceName() const;
    [[nodiscard]] QString imageUrl() const;

    [[nodiscard]] double exposure() const;
    void setExposure(double value);
    [[nodiscard]] double contrast() const;
    void setContrast(double value);
    [[nodiscard]] double saturation() const;
    void setSaturation(double value);
    [[nodiscard]] double temperature() const;
    void setTemperature(double value);
    [[nodiscard]] double tint() const;
    void setTint(double value);

    Q_INVOKABLE bool openImage(const QUrl& url);
    Q_INVOKABLE bool saveProject(const QUrl& url);
    Q_INVOKABLE bool loadProject(const QUrl& url);
    Q_INVOKABLE bool exportImage(const QUrl& url);
    Q_INVOKABLE void resetAdjustments();

signals:
    void documentChanged();
    void previewChanged();
    void adjustmentsChanged();
    void operationFailed(QString message);

private:
    void rebuildPreview();
    void setAdjustment(lumen::AdjustmentType type, double value);
    [[nodiscard]] QString localPath(const QUrl& url) const;

    lumen::DocumentModel m_document;
    lumen::RenderPipeline m_renderPipeline;
    lumen::ExportService m_exportService;
    lumen::ProjectStore m_projectStore;
    QString m_previewPath;
    int m_previewVersion = 0;
};
