# PART 3 of 4 — Editor core — document model, render pipeline, export, core CMake
# Tasks 21–27 (7 files)

## Checklist (mark [x] as each file is completed)
- [ ] Task 21: REPLACE `core/editor-core/DocumentModel.hpp`
- [ ] Task 22: REPLACE `core/editor-core/DocumentModel.cpp`
- [ ] Task 23: REPLACE `core/image-core/RenderPipeline.hpp`
- [ ] Task 24: REPLACE `core/image-core/RenderPipeline.cpp`
- [ ] Task 25: REPLACE `core/export-core/ExportService.hpp`
- [ ] Task 26: REPLACE `core/export-core/ExportService.cpp`
- [ ] Task 27: REPLACE `core/CMakeLists.txt`

---

## TASK 21 — REPLACE `core/editor-core/DocumentModel.hpp`

```cpp
#pragma once
#include "shared-types/Adjustment.hpp"
#include "shared-types/Layer.hpp"
#include "shared-types/Mask.hpp"
#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>
namespace lumen {
class DocumentModel final : public QObject {
    Q_OBJECT
public:
    explicit DocumentModel(QObject* parent = nullptr);
    bool openSourceImage(const QString& path);
    void replaceSourceImage(const QImage& newImage);
    void clear();
    [[nodiscard]] bool    hasDocument()  const;
    [[nodiscard]] QString sourcePath()   const;
    [[nodiscard]] QSize   sourceSize()   const;
    [[nodiscard]] const QImage& sourceImage() const;
    [[nodiscard]] QVector<Adjustment> adjustments()             const;
    [[nodiscard]] QVector<Adjustment> adjustmentsForLayer(const QString& layerId) const;
    [[nodiscard]] QVector<Layer>      layers()                  const;
    [[nodiscard]] QVector<Mask>       masks()                   const;
    [[nodiscard]] QImage              layerImage(const QString& layerId) const;
    [[nodiscard]] bool    canUndo()      const;
    [[nodiscard]] bool    canRedo()      const;
    [[nodiscard]] bool    isDownsampled() const;
    void setActiveMask(const QImage& mask);
    [[nodiscard]] const QImage& activeMask() const;
    void setScalarAdjustment(AdjustmentType type, double value);
    [[nodiscard]] double scalarAdjustment(AdjustmentType type) const;
    void rotateClockwise();
    void rotateCounterClockwise();
    void flipHorizontal();
    void flipVertical();
    void undo();
    void redo();
    // Layer management (M8)
    void addImageLayer(const QString& path);
    void moveLayer(int fromIndex, int toIndex);
    void setLayerOpacity(const QString& id, double opacity);
    void setLayerVisible(const QString& id, bool visible);
    void setLayerBlendMode(const QString& id, BlendMode mode);
    void deleteLayer(const QString& id);
signals:
    void changed();
    void historyChanged();
private:
    Adjustment*       findAdjustment(AdjustmentType type);
    const Adjustment* findAdjustment(AdjustmentType type) const;
    Layer*            findLayer(const QString& id);
    void pushHistorySnapshot();
    void restoreAdjustments(const QVector<Adjustment>& adjustments);
    QString   m_projectId;
    QString   m_sourcePath;
    QImage    m_sourceImage;
    bool      m_isDownsampled = false;
    QVector<Layer>      m_layers;
    QVector<Mask>       m_masks;
    QVector<Adjustment> m_adjustments;
    QHash<QString, QImage> m_layerImages;
    QVector<QVector<Adjustment>>  m_undoStack;
    QVector<QVector<Adjustment>>  m_redoStack;
    QVector<QImage>               m_sourceImageHistory; // for replaceSourceImage undo
};
} // namespace lumen
```


---

## TASK 22 — REPLACE `core/editor-core/DocumentModel.cpp`

```cpp
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
    case AdjustmentType::Exposure:       return "exposure";
    case AdjustmentType::Contrast:       return "contrast";
    case AdjustmentType::Highlights:     return "highlights";
    case AdjustmentType::Shadows:        return "shadows";
    case AdjustmentType::Whites:         return "whites";
    case AdjustmentType::Blacks:         return "blacks";
    case AdjustmentType::Saturation:     return "saturation";
    case AdjustmentType::Vibrance:       return "vibrance";
    case AdjustmentType::Temperature:    return "temperature";
    case AdjustmentType::Tint:           return "tint";
    case AdjustmentType::RotationDegrees:return "rotationDegrees";
    case AdjustmentType::FlipHorizontal: return "flipHorizontal";
    case AdjustmentType::FlipVertical:   return "flipVertical";
    case AdjustmentType::ToneCurveLuma:  return "toneCurveLuma";
    case AdjustmentType::ToneCurveR:     return "toneCurveR";
    case AdjustmentType::ToneCurveG:     return "toneCurveG";
    case AdjustmentType::ToneCurveB:     return "toneCurveB";
    case AdjustmentType::NoiseReduction: return "noiseReduction";
    case AdjustmentType::Sharpening:     return "sharpening";
    default:                             return "exposure";
    }
}
AdjustmentType adjustmentTypeFromString(const QString& v)
{
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
    if (image.isNull()) {
        image.load(path);
    }
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
    base.id           = makeId();
    base.name         = QFileInfo(path).completeBaseName();
    base.kind         = LayerKind::Image;
    base.sourceAssetId= m_projectId;
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
QVector<Adjustment> DocumentModel::adjustments() const { return m_adjustments; }
QVector<Layer>      DocumentModel::layers()       const { return m_layers; }
QVector<Mask>       DocumentModel::masks()        const { return m_masks; }
QVector<Adjustment> DocumentModel::adjustmentsForLayer(const QString& layerId) const
{
    QVector<Adjustment> result;
    for (const Adjustment& a : m_adjustments) {
        if (a.targetLayerId == layerId || (layerId.isEmpty() && a.targetLayerId.isEmpty()))
            result.push_back(a);
    }
    return result;
}
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
void DocumentModel::setScalarAdjustment(AdjustmentType type, double value)
{
    if (qFuzzyCompare(scalarAdjustment(type) + 1.0, value + 1.0)) return;
    pushHistorySnapshot();
    Adjustment* adj = findAdjustment(type);
    if (!adj) {
        Adjustment next; next.id = makeId(); next.type = type;
        next.order = m_adjustments.size();
        m_adjustments.push_back(next);
        adj = &m_adjustments.last();
    }
    adj->parameters["value"] = value;
    m_redoStack.clear();
    emit changed(); emit historyChanged();
}
double DocumentModel::scalarAdjustment(AdjustmentType type) const
{
    const Adjustment* a = findAdjustment(type);
    return a ? a->parameters.value("value").toDouble(0.0) : 0.0;
}
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
// Layer management
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
    m_layers.push_back(layer);
    m_layerImages[layer.id] = img.scaled(m_sourceImage.size(),
        Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_RGBA64);
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
{
    if (Layer* l = findLayer(id)) { l->opacity = opacity; emit changed(); }
}
void DocumentModel::setLayerVisible(const QString& id, bool visible)
{
    if (Layer* l = findLayer(id)) { l->visible = visible; emit changed(); }
}
void DocumentModel::setLayerBlendMode(const QString& id, BlendMode mode)
{
    if (Layer* l = findLayer(id)) { l->blendMode = mode; emit changed(); }
}
void DocumentModel::deleteLayer(const QString& id)
{
    if (m_layers.size() <= 1) return;
    pushHistorySnapshot();
    m_layers.removeIf([&](const Layer& l){ return l.id == id; });
    m_layerImages.remove(id);
    emit changed();
}
Adjustment* DocumentModel::findAdjustment(AdjustmentType type)
{
    for (Adjustment& a : m_adjustments) if (a.type == type) return &a;
    return nullptr;
}
const Adjustment* DocumentModel::findAdjustment(AdjustmentType type) const
{
    for (const Adjustment& a : m_adjustments) if (a.type == type) return &a;
    return nullptr;
}
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
```


---

## TASK 23 — REPLACE `core/image-core/RenderPipeline.hpp`

```cpp
#pragma once
#include "editor-core/DocumentModel.hpp"
#include "shared-types/Layer.hpp"
#include <QImage>
#include <QSize>
#include <QVector>
#include <atomic>
#include <memory>
namespace lumen {
class RenderPipeline {
public:
    [[nodiscard]] QImage renderPreview(
        const DocumentModel& document, QSize maximumSize,
        std::shared_ptr<std::atomic<bool>> cancelled = nullptr) const;
    [[nodiscard]] QImage renderPreviewFromData(
        const QImage& source,
        const QVector<Adjustment>& adjustments,
        QSize maximumSize,
        const QImage& mask = {},
        std::shared_ptr<std::atomic<bool>> cancelled = nullptr) const;
    [[nodiscard]] QImage renderFullResolution(const DocumentModel& document) const;
private:
    [[nodiscard]] QImage applyAdjustments(
        QImage image,
        const QVector<Adjustment>& adjustments,
        const QImage& mask = {},
        std::shared_ptr<std::atomic<bool>> cancelled = nullptr) const;
    void blendOnto(QImage& canvas, const QImage& layer,
                   BlendMode mode, double opacity) const;
    static std::array<double, 65536> buildCurveLut(const QJsonArray& points);
};
} // namespace lumen
```


---

## TASK 24 — REPLACE `core/image-core/RenderPipeline.cpp`

```cpp
#include "image-core/RenderPipeline.hpp"
#include "image-core/BlendModes.hpp"
#include "image-core/ColorManager.hpp"
#include <QJsonArray>
#include <QJsonValue>
#include <QTransform>
#include <QtMath>
#include <algorithm>
#ifdef HAVE_OPENCV
#  include <opencv2/core.hpp>
#  include <opencv2/imgproc.hpp>
#  include <opencv2/photo.hpp>
#endif
namespace lumen {
namespace {
quint16 clamp16(double v) { return static_cast<quint16>(qBound(0.0, v, 65535.0)); }
double scalarAdj(const QVector<Adjustment>& adjs, AdjustmentType type)
{
    for (const Adjustment& a : adjs)
        if (a.type == type && a.enabled)
            return a.parameters.value("value").toDouble(0.0);
    return 0.0;
}
const Adjustment* findAdj(const QVector<Adjustment>& adjs, AdjustmentType type)
{
    for (const Adjustment& a : adjs)
        if (a.type == type && a.enabled) return &a;
    return nullptr;
}
} // namespace
std::array<double, 65536> RenderPipeline::buildCurveLut(const QJsonArray& points)
{
    std::array<double, 65536> lut;
    if (points.isEmpty()) {
        for (int i = 0; i < 65536; ++i) lut[i] = i;
        return lut;
    }
    // Parse control points
    QVector<QPair<double,double>> pts;
    pts.append({0.0, 0.0});
    for (const QJsonValue& v : points) {
        const QJsonArray p = v.toArray();
        if (p.size() >= 2)
            pts.append({p[0].toDouble(), p[1].toDouble()});
    }
    pts.append({1.0, 1.0});
    std::sort(pts.begin(), pts.end(), [](auto& a, auto& b){ return a.first < b.first; });
    // Piecewise linear interpolation
    for (int i = 0; i < 65536; ++i) {
        const double x = i / 65535.0;
        double y = x;
        for (int k = 1; k < pts.size(); ++k) {
            if (x <= pts[k].first) {
                const double t = (x - pts[k-1].first) / (pts[k].first - pts[k-1].first + 1e-9);
                y = pts[k-1].second + t * (pts[k].second - pts[k-1].second);
                break;
            }
        }
        lut[i] = qBound(0.0, y * 65535.0, 65535.0);
    }
    return lut;
}
QImage RenderPipeline::renderPreview(const DocumentModel& doc, QSize maxSize,
                                      std::shared_ptr<std::atomic<bool>> cancelled) const
{
    if (!doc.hasDocument()) return {};
    return renderPreviewFromData(doc.sourceImage(), doc.adjustments(),
                                  maxSize, doc.activeMask(), cancelled);
}
QImage RenderPipeline::renderPreviewFromData(
    const QImage& sourceImage,
    const QVector<Adjustment>& adjustments,
    QSize maximumSize,
    const QImage& mask,
    std::shared_ptr<std::atomic<bool>> cancelled) const
{
    if (sourceImage.isNull()) return {};
    QImage source = sourceImage;
    if (maximumSize.isValid() &&
        (source.width() > maximumSize.width() || source.height() > maximumSize.height()))
        source = source.scaled(maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage scaledMask = mask.isNull() ? QImage() :
        mask.scaled(source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return applyAdjustments(source, adjustments, scaledMask, cancelled);
}
QImage RenderPipeline::renderFullResolution(const DocumentModel& doc) const
{
    if (!doc.hasDocument()) return {};
    auto layers = doc.layers();
    if (layers.isEmpty())
        return applyAdjustments(doc.sourceImage(), doc.adjustmentsForLayer(QString()),
                                doc.activeMask());
    std::sort(layers.begin(), layers.end(),
              [](const Layer& a, const Layer& b){ return a.order < b.order; });
    const QSize sz = doc.sourceImage().size();
    QImage canvas(sz, QImage::Format_RGBA64);
    canvas.fill(Qt::transparent);
    const QVector<Adjustment> globalAdjs = doc.adjustmentsForLayer(QString());
    for (const Layer& layer : layers) {
        if (!layer.visible) continue;
        QImage layerImg = doc.layerImage(layer.id);
        if (layerImg.isNull()) continue;
        QVector<Adjustment> adjs = globalAdjs;
        adjs.append(doc.adjustmentsForLayer(layer.id));
        layerImg = applyAdjustments(
            layerImg.convertToFormat(QImage::Format_RGBA64), adjs);
        blendOnto(canvas, layerImg, layer.blendMode, layer.opacity);
    }
    // Apply mask to final composite
    if (!doc.activeMask().isNull()) {
        const QImage scaledMask = doc.activeMask().scaled(
            sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < canvas.height(); ++y) {
            auto* dst = reinterpret_cast<QRgba64*>(canvas.scanLine(y));
            const auto* msk = reinterpret_cast<const QRgb*>(scaledMask.constScanLine(y));
            for (int x = 0; x < canvas.width(); ++x) {
                const double a = qAlpha(msk[x]) / 255.0;
                dst[x] = QRgba64::fromRgba64(
                    dst[x].red(), dst[x].green(), dst[x].blue(),
                    static_cast<quint16>(dst[x].alpha() * a));
            }
        }
    }
    return canvas;
}
void RenderPipeline::blendOnto(QImage& canvas, const QImage& layer,
                                BlendMode mode, double opacity) const
{
    const int W = qMin(canvas.width(),  layer.width());
    const int H = qMin(canvas.height(), layer.height());
    for (int y = 0; y < H; ++y) {
        auto*       dst = reinterpret_cast<QRgba64*>(canvas.scanLine(y));
        const auto* src = reinterpret_cast<const QRgba64*>(layer.constScanLine(y));
        for (int x = 0; x < W; ++x) {
            const double bR = dst[x].red()   / 65535.0, bG = dst[x].green() / 65535.0;
            const double bB = dst[x].blue()  / 65535.0, bA = dst[x].alpha() / 65535.0;
            const double lR = src[x].red()   / 65535.0, lG = src[x].green() / 65535.0;
            const double lB = src[x].blue()  / 65535.0, lA = src[x].alpha() / 65535.0;
            const double eff = opacity * lA;
            const double oR  = blend::compose(bR, lR, eff, mode);
            const double oG  = blend::compose(bG, lG, eff, mode);
            const double oB  = blend::compose(bB, lB, eff, mode);
            const double oA  = bA + lA * opacity * (1.0 - bA);
            dst[x] = QRgba64::fromRgba64(
                clamp16(oR*65535), clamp16(oG*65535),
                clamp16(oB*65535), clamp16(oA*65535));
        }
    }
}
QImage RenderPipeline::applyAdjustments(
    QImage image,
    const QVector<Adjustment>& adjustments,
    const QImage& mask,
    std::shared_ptr<std::atomic<bool>> cancelled) const
{
    image = image.convertToFormat(QImage::Format_RGBA64);
    // Transform ops
    const int  rotation      = int(scalarAdj(adjustments, AdjustmentType::RotationDegrees)) % 360;
    const bool flipH         = scalarAdj(adjustments, AdjustmentType::FlipHorizontal) > 0.5;
    const bool flipV         = scalarAdj(adjustments, AdjustmentType::FlipVertical)   > 0.5;
    if (flipH || flipV) image = image.mirrored(flipH, flipV);
    if (rotation != 0) {
        QTransform t; t.rotate(rotation);
        image = image.transformed(t, Qt::SmoothTransformation);
    }
    // Tone scalars
    const double exposureGain  = qPow(2.0, scalarAdj(adjustments, AdjustmentType::Exposure));
    const double contrastGain  = 1.0 + scalarAdj(adjustments, AdjustmentType::Contrast) / 100.0;
    const double highlights    = scalarAdj(adjustments, AdjustmentType::Highlights) / 100.0;
    const double shadows       = scalarAdj(adjustments, AdjustmentType::Shadows)    / 100.0;
    const double whites        = scalarAdj(adjustments, AdjustmentType::Whites)     / 100.0;
    const double blacks        = scalarAdj(adjustments, AdjustmentType::Blacks)     / 100.0;
    const double satGain       = 1.0 + scalarAdj(adjustments, AdjustmentType::Saturation) / 100.0;
    const double vibrance      = scalarAdj(adjustments, AdjustmentType::Vibrance)   / 100.0;
    const double temperature   = scalarAdj(adjustments, AdjustmentType::Temperature);
    const double tint          = scalarAdj(adjustments, AdjustmentType::Tint);
    const double whitesGain    = 1.0 + whites * 0.5;
    const double blacksOffset  = blacks * 0.15 * 65535.0;
    const double redBalance    = 1.0 + temperature / 400.0;
    const double blueBalance   = 1.0 - temperature / 400.0;
    const double greenBalance  = 1.0 + tint        / 400.0;
    // Tone curve LUTs
    bool hasLumaLut = false, hasRLut = false, hasGLut = false, hasBLut = false;
    std::array<double,65536> lumaLut{}, rLut{}, gLut{}, bLut{};
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveLuma)) {
        lumaLut = buildCurveLut(a->parameters.value("points").toArray()); hasLumaLut = true; }
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveR)) {
        rLut = buildCurveLut(a->parameters.value("points").toArray()); hasRLut = true; }
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveG)) {
        gLut = buildCurveLut(a->parameters.value("points").toArray()); hasGLut = true; }
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveB)) {
        bLut = buildCurveLut(a->parameters.value("points").toArray()); hasBLut = true; }
    // Scaled mask for compositing
    const bool hasMask = !mask.isNull();
    QImage scaledMask = hasMask ?
        mask.scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_ARGB32) : QImage();
    for (int y = 0; y < image.height(); ++y) {
        if (cancelled && *cancelled) return {};
        auto* scanline = reinterpret_cast<QRgba64*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgba64 origPixel = scanline[x];
            double r = origPixel.red()   * exposureGain;
            double g = origPixel.green() * exposureGain;
            double b = origPixel.blue()  * exposureGain;
            r = (r * whitesGain) + blacksOffset;
            g = (g * whitesGain) + blacksOffset;
            b = (b * whitesGain) + blacksOffset;
            r = ((r/65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            g = ((g/65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            b = ((b/65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            double luma = (0.2126*r + 0.7152*g + 0.0722*b) / 65535.0;
            if (luma > 0.5) {
                const double blend = (luma - 0.5) * 2.0;
                const double gain  = 1.0 + highlights * blend;
                r *= gain; g *= gain; b *= gain;
            }
            luma = (0.2126*r + 0.7152*g + 0.0722*b) / 65535.0;
            if (luma < 0.5) {
                const double blend = (0.5 - luma) * 2.0;
                const double gain  = 1.0 + shadows * blend;
                r *= gain; g *= gain; b *= gain;
            }
            const double luma16 = 0.2126*r + 0.7152*g + 0.0722*b;
            r = luma16 + (r - luma16) * satGain;
            g = luma16 + (g - luma16) * satGain;
            b = luma16 + (b - luma16) * satGain;
            const double satMax = qMax(r, qMax(g, b));
            const double satMin = qMin(r, qMin(g, b));
            const double colorfulness = (satMax - satMin) / (satMax + 1e-6);
            const double vibGain = 1.0 + vibrance * (1.0 - colorfulness);
            r = luma16 + (r - luma16) * vibGain;
            g = luma16 + (g - luma16) * vibGain;
            b = luma16 + (b - luma16) * vibGain;
            r *= redBalance; g *= greenBalance; b *= blueBalance;
            // Tone curves
            if (hasRLut) r = rLut[clamp16(r)];
            if (hasGLut) g = gLut[clamp16(g)];
            if (hasBLut) b = bLut[clamp16(b)];
            if (hasLumaLut) {
                const double l2  = 0.2126*r + 0.7152*g + 0.0722*b;
                const double l2m = lumaLut[clamp16(l2)];
                const double scale = l2 > 0 ? l2m / l2 : 1.0;
                r *= scale; g *= scale; b *= scale;
            }
            // Mask compositing
            if (hasMask) {
                const QRgb mPx = reinterpret_cast<const QRgb*>(
                    scaledMask.constScanLine(y))[x];
                const double alpha = qAlpha(mPx) / 255.0;
                r = origPixel.red()   * (1.0-alpha) + r * alpha;
                g = origPixel.green() * (1.0-alpha) + g * alpha;
                b = origPixel.blue()  * (1.0-alpha) + b * alpha;
            }
            scanline[x] = QRgba64::fromRgba64(
                clamp16(r), clamp16(g), clamp16(b), origPixel.alpha());
        }
    }
    // OpenCV NR and Sharpening (applied post pixel-loop)
#ifdef HAVE_OPENCV
    const double nr = scalarAdj(adjustments, AdjustmentType::NoiseReduction);
    const double sh = scalarAdj(adjustments, AdjustmentType::Sharpening);
    if (nr > 0.0 || sh > 0.0) {
        QImage img8 = image.convertToFormat(QImage::Format_RGB888);
        cv::Mat mat(img8.height(), img8.width(), CV_8UC3,
                    img8.bits(), img8.bytesPerLine());
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
        if (nr > 0.0) {
            float h = static_cast<float>(nr * 0.5);
            cv::fastNlMeansDenoisingColored(mat, mat, h, h, 7, 21);
        }
        if (sh > 0.0) {
            cv::Mat blurred;
            cv::GaussianBlur(mat, blurred, cv::Size(0,0), 3);
            cv::addWeighted(mat, 1.0 + sh/100.0,
                            blurred, -(sh/100.0), 0, mat);
        }
        cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
        QImage result(mat.data, mat.cols, mat.rows,
                      static_cast<int>(mat.step), QImage::Format_RGB888);
        image = result.copy().convertToFormat(QImage::Format_RGBA64);
    }
#endif
    return image;
}
} // namespace lumen
```


---

## TASK 25 — REPLACE `core/export-core/ExportService.hpp`

```cpp
#pragma once
#include "editor-core/DocumentModel.hpp"
#include "image-core/RenderPipeline.hpp"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
namespace lumen {
class ExportService final : public QObject {
    Q_OBJECT
public:
    explicit ExportService(QObject* parent = nullptr);
    [[nodiscard]] bool exportImage(const DocumentModel& document,
                                   const QString& path, int quality = 92) const;
    void exportBatch(const DocumentModel& document,
                     const QString& directory,
                     const QStringList& formats);
signals:
    void batchComplete();
    void batchFailed(QString reason);
private:
    RenderPipeline m_renderPipeline;
};
} // namespace lumen
```


---

## TASK 26 — REPLACE `core/export-core/ExportService.cpp`

```cpp
#include "export-core/ExportService.hpp"
#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>
namespace lumen {
ExportService::ExportService(QObject* parent) : QObject(parent) {}
bool ExportService::exportImage(const DocumentModel& document,
                                 const QString& path, int quality) const
{
    const QImage rendered = m_renderPipeline.renderFullResolution(document);
    if (rendered.isNull()) return false;
    return rendered.save(path, nullptr, quality);
}
void ExportService::exportBatch(const DocumentModel& document,
                                 const QString& directory,
                                 const QStringList& formats)
{
    const QString baseName = QFileInfo(document.sourcePath()).baseName();
    const QImage rendered  = m_renderPipeline.renderFullResolution(document);
    if (rendered.isNull()) { emit batchFailed("Render failed"); return; }
    QtConcurrent::run([this, rendered, directory, baseName, formats]() {
        bool allOk = true;
        for (const QString& fmt : formats) {
            const QString path = directory + "/" + baseName + "." + fmt.toLower();
            const int quality  = fmt.toLower() == "jpg" ? 92 : -1;
            if (!rendered.save(path, nullptr, quality)) allOk = false;
        }
        QMetaObject::invokeMethod(this,
            allOk ? &ExportService::batchComplete
                  : [this]{ emit batchFailed("One or more formats failed"); },
            Qt::QueuedConnection);
    });
}
} // namespace lumen
```


---

## TASK 27 — REPLACE `core/CMakeLists.txt`

```cmake
add_library(lumen_core STATIC
    shared-types/Adjustment.hpp
    shared-types/Layer.hpp
    shared-types/Mask.hpp
    editor-core/DocumentModel.cpp
    editor-core/DocumentModel.hpp
    image-core/RenderPipeline.cpp
    image-core/RenderPipeline.hpp
    image-core/BlendModes.hpp
    image-core/RawImporter.cpp
    image-core/RawImporter.hpp
    image-core/ColorManager.cpp
    image-core/ColorManager.hpp
    mask-core/BrushEngine.cpp
    mask-core/BrushEngine.hpp
    mask-core/MaskDocument.cpp
    mask-core/MaskDocument.hpp
    ai-core/AiRuntime.cpp
    ai-core/AiRuntime.hpp
    ai-core/OnnxSession.cpp
    ai-core/OnnxSession.hpp
    ai-core/MaskPredictor.cpp
    ai-core/MaskPredictor.hpp
    ai-core/InpaintEngine.cpp
    ai-core/InpaintEngine.hpp
    ai-core/UpscaleEngine.cpp
    ai-core/UpscaleEngine.hpp
    export-core/ExportService.cpp
    export-core/ExportService.hpp
    storage/ProjectStore.cpp
    storage/ProjectStore.hpp
)
target_include_directories(lumen_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(lumen_core
    PUBLIC Qt6::Core Qt6::Gui Qt6::Sql Qt6::Concurrent
)
# ONNX Runtime (optional — install via vcpkg or SDK)
find_package(onnxruntime QUIET
    HINTS "$ENV{ONNXRUNTIME_ROOT}/lib/cmake/onnxruntime")
if(onnxruntime_FOUND)
    message(STATUS "ONNX Runtime found — AI inference enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_ONNXRUNTIME)
    target_link_libraries(lumen_core PUBLIC onnxruntime::onnxruntime)
else()
    message(STATUS "ONNX Runtime NOT found — AI features disabled")
endif()
# LibRaw (optional — vcpkg: libraw)
find_package(LibRaw QUIET)
if(LibRaw_FOUND)
    message(STATUS "LibRaw found — RAW import enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_LIBRAW)
    target_link_libraries(lumen_core PUBLIC LibRaw::LibRaw)
else()
    message(STATUS "LibRaw NOT found — RAW import disabled")
endif()
# Little CMS 2 (optional — vcpkg: lcms2)
find_package(lcms2 QUIET)
if(lcms2_FOUND)
    message(STATUS "lcms2 found — color management enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_LCMS2)
    target_link_libraries(lumen_core PUBLIC lcms2)
else()
    message(STATUS "lcms2 NOT found — color profiles disabled")
endif()
# OpenCV (optional — vcpkg: opencv4[core,imgproc,photo])
find_package(OpenCV QUIET COMPONENTS core imgproc photo)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found — NR and sharpening enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_OPENCV)
    target_link_libraries(lumen_core PUBLIC ${OpenCV_LIBS})
else()
    message(STATUS "OpenCV NOT found — NR/sharpening disabled")
endif()
```
