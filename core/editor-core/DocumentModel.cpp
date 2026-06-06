#include "editor-core/DocumentModel.hpp"

#include <QFileInfo>
#include <QUuid>

namespace lumen {

namespace {

QString makeId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace

QString adjustmentTypeToString(AdjustmentType type)
{
    switch (type) {
    case AdjustmentType::Exposure: return "exposure";
    case AdjustmentType::Contrast: return "contrast";
    case AdjustmentType::Highlights: return "highlights";
    case AdjustmentType::Shadows: return "shadows";
    case AdjustmentType::Whites: return "whites";
    case AdjustmentType::Blacks: return "blacks";
    case AdjustmentType::Saturation: return "saturation";
    case AdjustmentType::Vibrance: return "vibrance";
    case AdjustmentType::Temperature: return "temperature";
    case AdjustmentType::Tint: return "tint";
    case AdjustmentType::RotationDegrees: return "rotationDegrees";
    case AdjustmentType::FlipHorizontal: return "flipHorizontal";
    case AdjustmentType::FlipVertical: return "flipVertical";
    }
    return "exposure";
}

AdjustmentType adjustmentTypeFromString(const QString& value)
{
    if (value == "contrast") return AdjustmentType::Contrast;
    if (value == "highlights") return AdjustmentType::Highlights;
    if (value == "shadows") return AdjustmentType::Shadows;
    if (value == "whites") return AdjustmentType::Whites;
    if (value == "blacks") return AdjustmentType::Blacks;
    if (value == "saturation") return AdjustmentType::Saturation;
    if (value == "vibrance") return AdjustmentType::Vibrance;
    if (value == "temperature") return AdjustmentType::Temperature;
    if (value == "tint") return AdjustmentType::Tint;
    if (value == "rotationDegrees") return AdjustmentType::RotationDegrees;
    if (value == "flipHorizontal") return AdjustmentType::FlipHorizontal;
    if (value == "flipVertical") return AdjustmentType::FlipVertical;
    return AdjustmentType::Exposure;
}

DocumentModel::DocumentModel(QObject* parent)
    : QObject(parent)
{
}

bool DocumentModel::openSourceImage(const QString& path)
{
    QImage image;
    if (!image.load(path)) {
        return false;
    }

    clear();
    m_projectId = makeId();
    m_sourcePath = QFileInfo(path).absoluteFilePath();
    m_sourceImage = image.convertToFormat(QImage::Format_RGBA64);

    Layer baseLayer;
    baseLayer.id = makeId();
    baseLayer.name = QFileInfo(path).completeBaseName();
    baseLayer.kind = LayerKind::Image;
    baseLayer.sourceAssetId = m_projectId;
    m_layers.push_back(baseLayer);

    emit changed();
    return true;
}

void DocumentModel::clear()
{
    m_projectId.clear();
    m_sourcePath.clear();
    m_sourceImage = {};
    m_layers.clear();
    m_masks.clear();
    m_adjustments.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    emit changed();
    emit historyChanged();
}

bool DocumentModel::hasDocument() const
{
    return !m_sourceImage.isNull();
}

QString DocumentModel::sourcePath() const
{
    return m_sourcePath;
}

QSize DocumentModel::sourceSize() const
{
    return m_sourceImage.size();
}

const QImage& DocumentModel::sourceImage() const
{
    return m_sourceImage;
}

QVector<Adjustment> DocumentModel::adjustments() const
{
    return m_adjustments;
}

QVector<Layer> DocumentModel::layers() const
{
    return m_layers;
}

QVector<Mask> DocumentModel::masks() const
{
    return m_masks;
}

const QImage& DocumentModel::activeMask() const
{
    // For now, return the first mask if any, or a null image
    if (!m_masks.isEmpty()) {
        return m_masks.first().mask;
    }
    static const QImage nullMask;
    return nullMask;
}

void DocumentModel::setActiveMask(const QImage& mask)
{
    if (m_masks.isEmpty()) {
        Mask newMask;
        newMask.id = makeId();
        newMask.name = "Active Mask";
        newMask.mask = mask;
        m_masks.push_back(newMask);
    } else {
        m_masks.first().mask = mask;
    }
    emit changed();
}

bool DocumentModel::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool DocumentModel::canRedo() const
{
    return !m_redoStack.isEmpty();
}

void DocumentModel::setScalarAdjustment(AdjustmentType type, double value)
{
    if (qFuzzyCompare(scalarAdjustment(type) + 1.0, value + 1.0)) {
        return;
    }

    pushHistorySnapshot();

    Adjustment* adjustment = findAdjustment(type);
    if (!adjustment) {
        Adjustment next;
        next.id = makeId();
        next.type = type;
        next.order = m_adjustments.size();
        m_adjustments.push_back(next);
        adjustment = &m_adjustments.last();
    }

    adjustment->parameters["value"] = value;
    m_redoStack.clear();
    emit changed();
    emit historyChanged();
}

double DocumentModel::scalarAdjustment(AdjustmentType type) const
{
    const Adjustment* adjustment = findAdjustment(type);
    if (!adjustment) {
        return 0.0;
    }
    return adjustment->parameters.value("value").toDouble(0.0);
}

void DocumentModel::rotateClockwise()
{
    const int rotation = static_cast<int>(scalarAdjustment(AdjustmentType::RotationDegrees));
    setScalarAdjustment(AdjustmentType::RotationDegrees, (rotation + 90) % 360);
}

void DocumentModel::rotateCounterClockwise()
{
    const int rotation = static_cast<int>(scalarAdjustment(AdjustmentType::RotationDegrees));
    setScalarAdjustment(AdjustmentType::RotationDegrees, (rotation + 270) % 360);
}

void DocumentModel::flipHorizontal()
{
    setScalarAdjustment(AdjustmentType::FlipHorizontal, scalarAdjustment(AdjustmentType::FlipHorizontal) > 0.5 ? 0.0 : 1.0);
}

void DocumentModel::flipVertical()
{
    setScalarAdjustment(AdjustmentType::FlipVertical, scalarAdjustment(AdjustmentType::FlipVertical) > 0.5 ? 0.0 : 1.0);
}

void DocumentModel::undo()
{
    if (!canUndo()) {
        return;
    }

    m_redoStack.push_back(m_adjustments);
    restoreAdjustments(m_undoStack.takeLast());
}

void DocumentModel::redo()
{
    if (!canRedo()) {
        return;
    }

    m_undoStack.push_back(m_adjustments);
    restoreAdjustments(m_redoStack.takeLast());
}

Adjustment* DocumentModel::findAdjustment(AdjustmentType type)
{
    for (Adjustment& adjustment : m_adjustments) {
        if (adjustment.type == type) {
            return &adjustment;
        }
    }
    return nullptr;
}

const Adjustment* DocumentModel::findAdjustment(AdjustmentType type) const
{
    for (const Adjustment& adjustment : m_adjustments) {
        if (adjustment.type == type) {
            return &adjustment;
        }
    }
    return nullptr;
}

void DocumentModel::pushHistorySnapshot()
{
    m_undoStack.push_back(m_adjustments);
    if (m_undoStack.size() > 100) {
        m_undoStack.removeFirst();
    }
}

void DocumentModel::restoreAdjustments(const QVector<Adjustment>& adjustments)
{
    m_adjustments = adjustments;
    emit changed();
    emit historyChanged();
}

} // namespace lumen
