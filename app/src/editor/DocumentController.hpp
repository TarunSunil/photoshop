#pragma once

#include "editor-core/DocumentModel.hpp"
#include "export-core/ExportService.hpp"
#include "image-core/RenderPipeline.hpp"
#include "storage/ProjectStore.hpp"

#include <QFutureWatcher>
#include <QImage>
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
    Q_PROPERTY(double highlights READ highlights WRITE setHighlights NOTIFY adjustmentsChanged)
    Q_PROPERTY(double shadows READ shadows WRITE setShadows NOTIFY adjustmentsChanged)
    Q_PROPERTY(double whites READ whites WRITE setWhites NOTIFY adjustmentsChanged)
    Q_PROPERTY(double blacks READ blacks WRITE setBlacks NOTIFY adjustmentsChanged)
    Q_PROPERTY(double vibrance READ vibrance WRITE setVibrance NOTIFY adjustmentsChanged)
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY adjustmentsChanged)
    Q_PROPERTY(double tint READ tint WRITE setTint NOTIFY adjustmentsChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(bool showOriginal READ showOriginal WRITE setShowOriginal NOTIFY previewChanged)

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
    [[nodiscard]] double highlights() const;
    void setHighlights(double value);
    [[nodiscard]] double shadows() const;
    void setShadows(double value);
    [[nodiscard]] double whites() const;
    void setWhites(double value);
    [[nodiscard]] double blacks() const;
    void setBlacks(double value);
    [[nodiscard]] double vibrance() const;
    void setVibrance(double value);
    [[nodiscard]] double temperature() const;
    void setTemperature(double value);
    [[nodiscard]] double tint() const;
    void setTint(double value);
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] bool showOriginal() const;
    void setShowOriginal(bool value);

    Q_INVOKABLE bool openImage(const QUrl& url);
    Q_INVOKABLE bool saveProject(const QUrl& url);
    Q_INVOKABLE bool loadProject(const QUrl& url);
    Q_INVOKABLE bool exportImage(const QUrl& url);
    Q_INVOKABLE void resetAdjustments();
    Q_INVOKABLE void rotateClockwise();
    Q_INVOKABLE void rotateCounterClockwise();
    Q_INVOKABLE void flipHorizontal();
    Q_INVOKABLE void flipVertical();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void documentChanged();
    void previewChanged();
    void adjustmentsChanged();
    void historyChanged();
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
    bool m_showOriginal = false;
    QFutureWatcher<QImage>* m_previewWatcher = nullptr;
    bool m_previewPending = false;
    int m_previewRequestId = 0;
};
