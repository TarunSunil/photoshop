#include "editor/DocumentController.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtConcurrent>

DocumentController::DocumentController(QObject* parent)
    : QObject(parent)
{
    connect(&m_document, &lumen::DocumentModel::changed, this, [this]() {
        rebuildPreview();
        emit documentChanged();
        emit adjustmentsChanged();
    });
    connect(&m_document, &lumen::DocumentModel::historyChanged, this, &DocumentController::historyChanged);
}

bool DocumentController::hasDocument() const
{
    return m_document.hasDocument();
}

QString DocumentController::sourceName() const
{
    if (!m_document.hasDocument()) {
        return "No image loaded";
    }
    return QFileInfo(m_document.sourcePath()).fileName();
}

QString DocumentController::imageUrl() const
{
    if (m_showOriginal && m_document.hasDocument()) {
        return QUrl::fromLocalFile(m_document.sourcePath()).toString();
    }

    if (m_previewPath.isEmpty()) {
        return {};
    }
    return QUrl::fromLocalFile(m_previewPath).toString();
}

double DocumentController::exposure() const
{
    return m_document.scalarAdjustment(lumen::AdjustmentType::Exposure);
}

void DocumentController::setExposure(double value)
{
    setAdjustment(lumen::AdjustmentType::Exposure, value);
}

double DocumentController::contrast() const
{
    return m_document.scalarAdjustment(lumen::AdjustmentType::Contrast);
}

void DocumentController::setContrast(double value)
{
    setAdjustment(lumen::AdjustmentType::Contrast, value);
}

double DocumentController::saturation() const
{
    return m_document.scalarAdjustment(lumen::AdjustmentType::Saturation);
}

void DocumentController::setSaturation(double value)
{
    setAdjustment(lumen::AdjustmentType::Saturation, value);
}

double DocumentController::highlights() const
{
    return m_document.scalarAdjustment(lumen::AdjustmentType::Highlights);
}

void DocumentController::setHighlights(double value)
{
    setAdjustment(lumen::AdjustmentType::Highlights, value);
}

double DocumentController::shadows() const
{
    return m_document.scalarAdjustment(lumen::AdjustmentType::Shadows);
}

void DocumentController::setShadows(double value)
{
    setAdjustment(lumen::AdjustmentType::Shadows, value);
}

double DocumentController::whites() const
{
    return m_document.scalarAdjustment(lumen::AdjustmentType::Whites);
}

void DocumentController::setWhites(double value)
{
    setAdjustment(lumen::AdjustmentType::Whites, value);
}

double DocumentController::blacks() const
{
    return m_document.scalarAdjustment(lumen::AdjustmentType::Blacks);
}

void DocumentController::setBlacks(double value)
{
    setAdjustment(lumen::AdjustmentType::Blacks, value);
}

double DocumentController::vibrance() const
{
    return m_document.scalarAdjustment(lumen::AdjustmentType::Vibrance);
}

void DocumentController::setVibrance(double value)
{
    setAdjustment(lumen::AdjustmentType::Vibrance, value);
}

double DocumentController::temperature() const
{
    return m_document.scalarAdjustment(lumen::AdjustmentType::Temperature);
}

void DocumentController::setTemperature(double value)
{
    setAdjustment(lumen::AdjustmentType::Temperature, value);
}

double DocumentController::tint() const
{
    return m_document.scalarAdjustment(lumen::AdjustmentType::Tint);
}

void DocumentController::setTint(double value)
{
    setAdjustment(lumen::AdjustmentType::Tint, value);
}

bool DocumentController::canUndo() const
{
    return m_document.canUndo();
}

bool DocumentController::canRedo() const
{
    return m_document.canRedo();
}

bool DocumentController::showOriginal() const
{
    return m_showOriginal;
}

void DocumentController::setShowOriginal(bool value)
{
    if (m_showOriginal == value) {
        return;
    }

    m_showOriginal = value;
    emit previewChanged();
}

bool DocumentController::openImage(const QUrl& url)
{
    const QString path = localPath(url);
    if (!m_document.openSourceImage(path)) {
        emit operationFailed("Could not open image.");
        return false;
    }
    return true;
}

bool DocumentController::saveProject(const QUrl& url)
{
    if (!m_projectStore.saveProject(m_document, localPath(url))) {
        emit operationFailed("Could not save project.");
        return false;
    }
    return true;
}

bool DocumentController::loadProject(const QUrl& url)
{
    if (!m_projectStore.loadProject(m_document, localPath(url))) {
        emit operationFailed("Could not load project.");
        return false;
    }
    return true;
}

bool DocumentController::exportImage(const QUrl& url)
{
    if (!m_exportService.exportImage(m_document, localPath(url))) {
        emit operationFailed("Could not export image.");
        return false;
    }
    return true;
}

void DocumentController::resetAdjustments()
{
    setExposure(0.0);
    setContrast(0.0);
    setSaturation(0.0);
    setHighlights(0.0);
    setShadows(0.0);
    setWhites(0.0);
    setBlacks(0.0);
    setVibrance(0.0);
    setTemperature(0.0);
    setTint(0.0);
}

void DocumentController::rotateClockwise()
{
    m_document.rotateClockwise();
}

void DocumentController::rotateCounterClockwise()
{
    m_document.rotateCounterClockwise();
}

void DocumentController::flipHorizontal()
{
    m_document.flipHorizontal();
}

void DocumentController::flipVertical()
{
    m_document.flipVertical();
}

void DocumentController::undo()
{
    m_document.undo();
}

void DocumentController::redo()
{
    m_document.redo();
}

void DocumentController::rebuildPreview()
{
    if (!m_document.hasDocument()) {
        ++m_previewRequestId;
        m_previewPath.clear();
        emit previewChanged();
        return;
    }

    if (m_previewWatcher && m_previewWatcher->isRunning()) {
        m_previewPending = true;
        return;
    }

    const QImage sourceImage = m_document.sourceImage();
    const QVector<lumen::Adjustment> adjustments = m_document.adjustments();
    const int requestId = ++m_previewRequestId;
    auto* watcher = new QFutureWatcher<QImage>(this);
    m_previewWatcher = watcher;

    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, requestId]() {
        const QImage preview = watcher->result();
        if (!preview.isNull() && requestId == m_previewRequestId) {
            QDir previewDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
            const QString filename = QString("lumenforge-preview-%1.png").arg(++m_previewVersion);
            const QString path = previewDir.absoluteFilePath(filename);

            if (preview.save(path)) {
                m_previewPath = path;
                emit previewChanged();
            }
        }

        watcher->deleteLater();
        m_previewWatcher = nullptr;

        if (m_previewPending) {
            m_previewPending = false;
            rebuildPreview();
        }
    });

    const QFuture<QImage> future = QtConcurrent::run([pipeline = m_renderPipeline, sourceImage, adjustments]() {
        return pipeline.renderPreviewFromData(sourceImage, adjustments, QSize(1800, 1400));
    });
    watcher->setFuture(future);
}

void DocumentController::setAdjustment(lumen::AdjustmentType type, double value)
{
    if (qFuzzyCompare(m_document.scalarAdjustment(type), value)) {
        return;
    }
    m_document.setScalarAdjustment(type, value);
}

QString DocumentController::localPath(const QUrl& url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}
