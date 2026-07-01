#include "editor-core/DocumentModel.hpp"
#include "image-core/RawImporter.hpp"
#include <QFileInfo>
#include <QUuid>
namespace lumen {
namespace {
QString makeId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
} // namespace

QString adjustmentTypeToString(AdjustmentType type)
{
    switch (type) {
    case AdjustmentType::Brightness:      return "brightness";
    case AdjustmentType::Exposure:        return "exposure";
    case AdjustmentType::Contrast:        return "contrast";
    case AdjustmentType::Highlights:      return "highlights";
    case AdjustmentType::Shadows:         return "shadows";
    case AdjustmentType::Whites:          return "whites";
    case AdjustmentType::Blacks:          return "blacks";
    case AdjustmentType::Saturation:      return "saturation";
    case AdjustmentType::Vibrance:        return "vibrance";
    case AdjustmentType::Temperature:     return "temperature";
    case AdjustmentType::Tint:            return "tint";
    case AdjustmentType::RotationDegrees: return "rotationDegrees";
    case AdjustmentType::FlipHorizontal:  return "flipHorizontal";
    case AdjustmentType::FlipVertical:    return "flipVertical";
    case AdjustmentType::ToneCurveLuma:   return "toneCurveLuma";
    case AdjustmentType::ToneCurveR:      return "toneCurveR";
    case AdjustmentType::ToneCurveG:      return "toneCurveG";
    case AdjustmentType::ToneCurveB:      return "toneCurveB";
    case AdjustmentType::NoiseReduction:  return "noiseReduction";
    case AdjustmentType::Sharpening:      return "sharpening";
    default:                              return "exposure";
    }
}

AdjustmentType adjustmentTypeFromString(const QString& v)
{
    if (v == "brightness")      return AdjustmentType::Brightness;
    if (v == "contrast")        return AdjustmentType::Contrast;
    if (v == "highlights")      return AdjustmentType::Highlights;
    if (v == "shadows")         return AdjustmentType::Shadows;
    if (v == "whites")          return AdjustmentType::Whites;
    if (v == "blacks")          return AdjustmentType::Blacks;
    if (v == "saturation")      return AdjustmentType::Saturation;
    if (v == "vibrance")        return AdjustmentType::Vibrance;
    if (v == "temperature")     return AdjustmentType::Temperature;
    if (v == "tint")            return AdjustmentType::Tint;
    if (v == "rotationDegrees") return AdjustmentType::RotationDegrees;
    if (v == "flipHorizontal")  return AdjustmentType::FlipHorizontal;
    if (v == "flipVertical")    return AdjustmentType::FlipVertical;
    if (v == "toneCurveLuma")   return AdjustmentType::ToneCurveLuma;
    if (v == "toneCurveR")      return AdjustmentType::ToneCurveR;
    if (v == "toneCurveG")      return AdjustmentType::ToneCurveG;
    if (v == "toneCurveB")      return AdjustmentType::ToneCurveB;
    if (v == "noiseReduction")  return AdjustmentType::NoiseReduction;
    if (v == "sharpening")      return AdjustmentType::Sharpening;
    return AdjustmentType::Exposure;
}

DocumentModel::DocumentModel(QObject* parent) : QObject(parent) {}

bool DocumentModel::openSourceImage(const QString& path)
{
    QImage image;
    const QString ext = QFileInfo(path).suffix().toLower();
    if (RawImporter::supportedExtensions().contains(ext)) {
        RawImporter importer;
        image = importer.load(path);
    }
    if (image.isNull()) image.load(path);
    if (image.isNull()) return false;

    clear();
    m_projectId  = makeId();
    m_sourcePath = QFileInfo(path).absoluteFilePath();

    constexpr int MAX_MP = 100'000'000;
    if (image.width() * image.height() > MAX_MP) {
        m_isDownsampled = true;
        const double scale = std::sqrt(static_cast<double>(MAX_MP) /
                                       (image.width() * image.height()));
        m_sourceImage = image
            .scaled(image.size() * scale, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_RGBA64);
    } else {
        m_isDownsampled = false;
        m_sourceImage   = image.convertToFormat(QImage::Format_RGBA64);
    }

    Layer base;
    base.id            = makeId();
    base.name          = QFileInfo(path).completeBaseName();
    base.kind          = LayerKind::Image;
    base.sourceAssetId = m_projectId;
    m_layers.push_back(base);
    m_layerImages[base.id] = m_sourceImage;
    emit changed();
    return true;
}

void DocumentModel::replaceSourceImage(const QImage& newImage)
{
    if (m_sourceImageHistory.size() >= 5)
        m_sourceImageHistory.removeFirst();
    m_sourceImageHistory.push_back(m_sourceImage);
    m_sourceImage = newImage.convertToFormat(QImage::Format_RGBA64);
    if (!m_layers.isEmpty())
        m_layerImages[m_layers.first().id] = m_sourceImage;
    emit changed();
}

void DocumentModel::clear()
{
    m_projectId.clear(); m_sourcePath.clear();
    m_sourceImage = {}; m_isDownsampled = false;
    m_layers.clear(); m_masks.clear(); m_adjustments.clear();
    m_layerImages.clear(); m_sourceImageHistory.clear();
    m_undoStack.clear(); m_redoStack.clear();
    emit changed(); emit historyChanged();
}

bool    DocumentModel::hasDocument()   const { return !m_sourceImage.isNull(); }
QString DocumentModel::sourcePath()    const { return m_sourcePath; }
QSize   DocumentModel::sourceSize()    const { return m_sourceImage.size(); }
bool    DocumentModel::isDownsampled() const { return m_isDownsampled; }
const QImage& DocumentModel::sourceImage() const { return m_sourceImage; }

// ── Adjustment queries ────────────────────────────────────────────────────────

QVector<Adjustment> DocumentModel::adjustments() const { return m_adjustments; }

QVector<Adjustment> DocumentModel::adjustmentsForLayer(const QString& layerId) const
{
    QVector<Adjustment> result;
    for (const Adjustment& a : m_adjustments) {
        if (a.targetLayerId == layerId || (layerId.isEmpty() && a.targetLayerId.isEmpty()))
            result.push_back(a);
    }
    return result;
}

// Issue 5: filter by targetMaskId
QVector<Adjustment> DocumentModel::adjustmentsForTarget(const QString& targetMaskId) const
{
    QVector<Adjustment> result;
    for (const Adjustment& a : m_adjustments)
        if (a.targetMaskId == targetMaskId && a.enabled)
            result.push_back(a);
    return result;
}

// ── Private finders ───────────────────────────────────────────────────────────

Adjustment* DocumentModel::findAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId)
{
    for (Adjustment& a : m_adjustments)
        if (a.type == type && a.targetMaskId == targetMaskId) return &a;
    return nullptr;
}

const Adjustment* DocumentModel::findAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId) const
{
    for (const Adjustment& a : m_adjustments)
        if (a.type == type && a.targetMaskId == targetMaskId) return &a;
    return nullptr;
}

// Legacy finders: only consider global (targetMaskId=="") adjustments.
// This prevents mask-targeted adjustments from leaking into global reads.
Adjustment* DocumentModel::findAdjustment(AdjustmentType type)
{
    for (Adjustment& a : m_adjustments)
        if (a.type == type && a.targetMaskId.isEmpty()) return &a;
    return nullptr;
}

const Adjustment* DocumentModel::findAdjustment(AdjustmentType type) const
{
    for (const Adjustment& a : m_adjustments)
        if (a.type == type && a.targetMaskId.isEmpty()) return &a;
    return nullptr;
}

// ── Target-aware scalar adjustment (issue 5) ──────────────────────────────────

void DocumentModel::setScalarAdjustmentForTarget(AdjustmentType type, double value, const QString& targetMaskId)
{
    if (qFuzzyCompare(scalarAdjustmentForTarget(type, targetMaskId) + 1.0, value + 1.0)) return;
    pushHistorySnapshot();
    Adjustment* adj = findAdjustmentForTarget(type, targetMaskId);
    if (!adj) {
        Adjustment next;
        next.id           = makeId();
        next.type         = type;
        next.targetMaskId = targetMaskId;
        next.order        = m_adjustments.size();
        m_adjustments.push_back(next);
        adj = &m_adjustments.last();
    }
    adj->parameters["value"] = value;
    m_redoStack.clear();
    emit changed(); emit historyChanged();
}

double DocumentModel::scalarAdjustmentForTarget(AdjustmentType type, const QString& targetMaskId) const
{
    const Adjustment* a = findAdjustmentForTarget(type, targetMaskId);
    return a ? a->parameters.value("value").toDouble(0.0) : 0.0;
}

// ── Legacy scalar adjustment (delegates to targetMaskId="") ──────────────────

void DocumentModel::setScalarAdjustment(AdjustmentType type, double value)
{
    setScalarAdjustmentForTarget(type, value, QString());
}

double DocumentModel::scalarAdjustment(AdjustmentType type) const
{
    return scalarAdjustmentForTarget(type, QString());
}

// ── Layer/mask queries ────────────────────────────────────────────────────────

QVector<Layer> DocumentModel::layers()  const { return m_layers; }
QVector<Mask>  DocumentModel::masks()   const { return m_masks; }

QImage DocumentModel::layerImage(const QString& layerId) const
{
    if (m_layerImages.contains(layerId))
        return m_layerImages[layerId];
    return m_sourceImage;
}

const QImage& DocumentModel::activeMask() const
{
    if (!m_masks.isEmpty()) return m_masks.first().mask;
    static const QImage nullMask;
    return nullMask;
}

void DocumentModel::setActiveMask(const QImage& mask)
{
    if (m_masks.isEmpty()) {
        Mask m; m.id = makeId(); m.name = "Active Mask"; m.mask = mask;
        m_masks.push_back(m);
    } else {
        m_masks.first().mask = mask;
    }
    emit changed();
}

bool DocumentModel::canUndo() const { return !m_undoStack.isEmpty(); }
bool DocumentModel::canRedo() const { return !m_redoStack.isEmpty(); }

// ── Transform adjustments ─────────────────────────────────────────────────────

void DocumentModel::rotateClockwise()
{ setScalarAdjustment(AdjustmentType::RotationDegrees,
    (int(scalarAdjustment(AdjustmentType::RotationDegrees)) + 90) % 360); }
void DocumentModel::rotateCounterClockwise()
{ setScalarAdjustment(AdjustmentType::RotationDegrees,
    (int(scalarAdjustment(AdjustmentType::RotationDegrees)) + 270) % 360); }
void DocumentModel::flipHorizontal()
{ setScalarAdjustment(AdjustmentType::FlipHorizontal,
    scalarAdjustment(AdjustmentType::FlipHorizontal) > 0.5 ? 0.0 : 1.0); }
void DocumentModel::flipVertical()
{ setScalarAdjustment(AdjustmentType::FlipVertical,
    scalarAdjustment(AdjustmentType::FlipVertical) > 0.5 ? 0.0 : 1.0); }

// ── Undo / redo ───────────────────────────────────────────────────────────────

void DocumentModel::undo()
{
    if (!canUndo()) return;
    m_redoStack.push_back(m_adjustments);
    restoreAdjustments(m_undoStack.takeLast());
}
void DocumentModel::redo()
{
    if (!canRedo()) return;
    m_undoStack.push_back(m_adjustments);
    restoreAdjustments(m_redoStack.takeLast());
}

// ── Layer management ──────────────────────────────────────────────────────────

void DocumentModel::addImageLayer(const QString& path)
{
    QImage img; img.load(path);
    if (img.isNull()) return;
    pushHistorySnapshot();
    Layer layer;
    layer.id    = makeId();
    layer.name  = QFileInfo(path).completeBaseName();
    layer.kind  = LayerKind::Image;
    layer.order = m_layers.size();
    // Issue 6: keep layer at its own native resolution; transforms handle placement.
    // Do NOT scale to source size — that would stretch every added image to fill
    // the entire canvas, which defeats the "sticker on base" use case.
    m_layerImages[layer.id] = img.convertToFormat(QImage::Format_RGBA64);
    m_layers.push_back(layer);
    emit changed();
}

void DocumentModel::moveLayer(int from, int to)
{
    if (from < 0 || from >= m_layers.size() || to < 0 || to >= m_layers.size()) return;
    pushHistorySnapshot();
    m_layers.move(from, to);
    for (int i = 0; i < m_layers.size(); ++i) m_layers[i].order = i;
    emit changed();
}

Layer* DocumentModel::findLayer(const QString& id)
{
    for (Layer& l : m_layers) if (l.id == id) return &l;
    return nullptr;
}

void DocumentModel::setLayerOpacity(const QString& id, double opacity)
{ if (Layer* l = findLayer(id)) { l->opacity = opacity; emit changed(); } }

void DocumentModel::setLayerVisible(const QString& id, bool visible)
{ if (Layer* l = findLayer(id)) { l->visible = visible; emit changed(); } }

void DocumentModel::setLayerBlendMode(const QString& id, BlendMode mode)
{ if (Layer* l = findLayer(id)) { l->blendMode = mode; emit changed(); } }

void DocumentModel::deleteLayer(const QString& id)
{
    if (m_layers.size() <= 1) return;
    pushHistorySnapshot();
    m_layers.removeIf([&](const Layer& l){ return l.id == id; });
    m_layerImages.remove(id);
    emit changed();
}

// Issue 6: per-layer transform
void DocumentModel::setLayerTransform(const QString& id,
                                       double posX, double posY,
                                       double scaleX, double scaleY,
                                       double rotation)
{
    if (Layer* l = findLayer(id)) {
        l->posX     = posX;
        l->posY     = posY;
        l->scaleX   = scaleX;
        l->scaleY   = scaleY;
        l->rotation = rotation;
        emit changed();
    }
}

// ── History ───────────────────────────────────────────────────────────────────

void DocumentModel::pushHistorySnapshot()
{
    m_undoStack.push_back(m_adjustments);
    if (m_undoStack.size() > 100) m_undoStack.removeFirst();
}

void DocumentModel::restoreAdjustments(const QVector<Adjustment>& adjs)
{
    m_adjustments = adjs;
    emit changed(); emit historyChanged();
}

} // namespace lumen