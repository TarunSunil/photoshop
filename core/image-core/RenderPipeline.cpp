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
std::vector<double> RenderPipeline::buildCurveLut(const QJsonArray& points)
{
    // Heap-allocated on purpose: this used to be std::array<double,65536>
    // (512KB) returned by value and stored as a local in applyAdjustments().
    // Four of those declared unconditionally on the stack (2MB total) blew
    // past the default 1MB thread stack on QtConcurrent worker threads,
    // which was the actual cause of the 0xc00000fd crash.
    std::vector<double> lut(65536);
    if (points.isEmpty()) {
        for (int i = 0; i < 65536; ++i) lut[i] = i;
        return lut;
    }
    QVector<QPair<double,double>> pts;
    pts.append({0.0, 0.0});
    for (const QJsonValue& v : points) {
        const QJsonArray p = v.toArray();
        if (p.size() >= 2)
            pts.append({p[0].toDouble(), p[1].toDouble()});
    }
    pts.append({1.0, 1.0});
    std::sort(pts.begin(), pts.end(), [](auto& a, auto& b){ return a.first < b.first; });
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
    // Pass the active mask into each layer's adjustment pass, same as
    // renderPreviewFromData does. The old code ignored the mask here and
    // instead zeroed out alpha on the whole composited canvas afterward,
    // which produced a transparent/black-holed export that didn't match
    // what the preview showed at all.
    const QImage activeMask = doc.activeMask();
    for (const Layer& layer : layers) {
        if (!layer.visible) continue;
        QImage layerImg = doc.layerImage(layer.id);
        if (layerImg.isNull()) continue;
        QVector<Adjustment> adjs = globalAdjs;
        adjs.append(doc.adjustmentsForLayer(layer.id));
        layerImg = applyAdjustments(
            layerImg.convertToFormat(QImage::Format_RGBA64), adjs, activeMask);
        blendOnto(canvas, layerImg, layer.blendMode, layer.opacity);
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
    const int  rotation      = int(scalarAdj(adjustments, AdjustmentType::RotationDegrees)) % 360;
    const bool flipH         = scalarAdj(adjustments, AdjustmentType::FlipHorizontal) > 0.5;
    const bool flipV         = scalarAdj(adjustments, AdjustmentType::FlipVertical)   > 0.5;
    if (flipH || flipV) image = image.mirrored(flipH, flipV);
    if (rotation != 0) {
        QTransform t; t.rotate(rotation);
        image = image.transformed(t, Qt::SmoothTransformation);
    }
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
    // Heap-backed vectors, default-empty. They only allocate (and only
    // allocate the one curve actually present) when a tone-curve
    // adjustment exists. Previously these were four unconditional
    // std::array<double,65536> stack locals (2MB) on every call — see
    // buildCurveLut() for the full explanation.
    bool hasLumaLut = false, hasRLut = false, hasGLut = false, hasBLut = false;
    std::vector<double> lumaLut, rLut, gLut, bLut;
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveLuma)) {
        lumaLut = buildCurveLut(a->parameters.value("points").toArray()); hasLumaLut = true; }
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveR)) {
        rLut = buildCurveLut(a->parameters.value("points").toArray()); hasRLut = true; }
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveG)) {
        gLut = buildCurveLut(a->parameters.value("points").toArray()); hasGLut = true; }
    if (const Adjustment* a = findAdj(adjustments, AdjustmentType::ToneCurveB)) {
        bLut = buildCurveLut(a->parameters.value("points").toArray()); hasBLut = true; }
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
            if (hasRLut) r = rLut[clamp16(r)];
            if (hasGLut) g = gLut[clamp16(g)];
            if (hasBLut) b = bLut[clamp16(b)];
            if (hasLumaLut) {
                const double l2  = 0.2126*r + 0.7152*g + 0.0722*b;
                const double l2m = lumaLut[clamp16(l2)];
                const double scale = l2 > 0 ? l2m / l2 : 1.0;
                r *= scale; g *= scale; b *= scale;
            }
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
#ifdef HAVE_OPENCV
    const double nr = scalarAdj(adjustments, AdjustmentType::NoiseReduction);
    const double sh = scalarAdj(adjustments, AdjustmentType::Sharpening);
    if (nr > 0.0 || sh > 0.0) {
        QImage img8 = image.convertToFormat(QImage::Format_RGB888);
        cv::Mat mat(img8.height(), img8.width(), CV_8UC3,
                    img8.bits(), img8.bytesPerLine());
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);

        if (nr > 0.0) {
            // Bilateral filter: edge-preserving denoising — does NOT blur across
            // edges unlike fastNlMeansDenoising. Much faster and produces clean
            // results that match professional photo editors (Lightroom uses a
            // similar luminance/color bilateral approach).
            // sigmaColor: how much color difference is tolerated (higher = more smoothing)
            // sigmaSpace: spatial extent — keep moderate to avoid oil-painting look
            cv::Mat filtered;
            const double sigmaColor = 8.0 + nr * 0.40;   // 8–48 range
            const double sigmaSpace = 4.0 + nr * 0.12;   // 4–16 range
            // Use small d=-1 so sigma drives radius (avoids O(d^2) cost)
            cv::bilateralFilter(mat, filtered, -1, sigmaColor, sigmaSpace);
            mat = filtered;
        }

        if (sh > 0.0) {
            // Proper Unsharp Masking (USM): the industry standard.
            // sharp = original + amount * (original - blurred)
            // Equivalent to: sharp = (1+amount)*original - amount*blurred
            // Threshold clamping prevents amplifying already-noisy regions.
            const double sigma  = 0.6 + sh * 0.025;     // 0.6–3.1px blur radius
            const double amount = sh * 0.018;            // 0–1.8 gain
            const int    thresh = static_cast<int>(sh * 0.3); // noise gate 0–30

            cv::Mat blurred;
            cv::GaussianBlur(mat, blurred, cv::Size(0, 0), sigma);

            // Apply USM only where the edge signal exceeds the threshold
            // (prevents halo artifacts on flat areas and noise amplification)
            for (int y = 0; y < mat.rows; ++y) {
                const uchar* src = mat.ptr<uchar>(y);
                const uchar* blu = blurred.ptr<uchar>(y);
                uchar*       dst = mat.ptr<uchar>(y);
                for (int x = 0; x < mat.cols * 3; ++x) {
                    const int edge = static_cast<int>(src[x]) - static_cast<int>(blu[x]);
                    if (std::abs(edge) > thresh)
                        dst[x] = static_cast<uchar>(
                            std::clamp(static_cast<int>(src[x]) +
                                static_cast<int>(std::round(amount * edge)), 0, 255));
                }
            }
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