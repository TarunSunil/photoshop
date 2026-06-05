#include "editor/DocumentController.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

DocumentController::DocumentController(QObject* parent)
    : QObject(parent)
{
    connect(&m_document, &lumen::DocumentModel::changed, this, [this]() {
        rebuildPreview();
        emit documentChanged();
        emit adjustmentsChanged();
    });
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
    setTemperature(0.0);
    setTint(0.0);
}

void DocumentController::rebuildPreview()
{
    if (!m_document.hasDocument()) {
        m_previewPath.clear();
        emit previewChanged();
        return;
    }

    QDir previewDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    const QString filename = QString("lumenforge-preview-%1.png").arg(++m_previewVersion);
    const QString path = previewDir.absoluteFilePath(filename);

    const QImage preview = m_renderPipeline.renderPreview(m_document, QSize(1800, 1400));
    if (preview.save(path)) {
        m_previewPath = path;
        emit previewChanged();
    }
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
