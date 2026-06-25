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

    const int  rotation = int(scalarAdj(adjustments, AdjustmentType::RotationDegrees)) % 360;
    const bool flipH    = scalarAdj(adjustments, AdjustmentType::FlipHorizontal) > 0.5;
    const bool flipV    = scalarAdj(adjustments, AdjustmentType::FlipVertical)   > 0.5;
    if (flipH || flipV) image = image.mirrored(flipH, flipV);
    if (rotation != 0) {
        QTransform t; t.rotate(rotation);
        image = image.transformed(t, Qt::SmoothTransformation);
    }

    // ── Scalar adjustment parameters ─────────────────────────────────────────
    // Brightness: ±100 → ±32767 additive offset on the 0-65535 channel range.
    // Applied after Exposure so both controls interact naturally (Lightroom legacy model).
    const double brightnessOffset = scalarAdj(adjustments, AdjustmentType::Brightness)
                                    / 100.0 * 32767.0;
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

            // 1. Exposure (multiplicative EV shift)
            double r = origPixel.red()   * exposureGain;
            double g = origPixel.green() * exposureGain;
            double b = origPixel.blue()  * exposureGain;

            // 2. Brightness (additive midtone lift / pull)
            r += brightnessOffset;
            g += brightnessOffset;
            b += brightnessOffset;

            // 3. Whites / Blacks
            r = (r * whitesGain) + blacksOffset;
            g = (g * whitesGain) + blacksOffset;
            b = (b * whitesGain) + blacksOffset;

            // 4. Contrast
            r = ((r/65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            g = ((g/65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;
            b = ((b/65535.0 - 0.5) * contrastGain + 0.5) * 65535.0;

            // 5. Highlights
            double luma = (0.2126*r + 0.7152*g + 0.0722*b) / 65535.0;
            if (luma > 0.5) {
                const double blend = (luma - 0.5) * 2.0;
                const double gain  = 1.0 + highlights * blend;
                r *= gain; g *= gain; b *= gain;
            }

            // 6. Shadows
            luma = (0.2126*r + 0.7152*g + 0.0722*b) / 65535.0;
            if (luma < 0.5) {
                const double blend = (0.5 - luma) * 2.0;
                const double gain  = 1.0 + shadows * blend;
                r *= gain; g *= gain; b *= gain;
            }

            // 7. Saturation
            const double luma16 = 0.2126*r + 0.7152*g + 0.0722*b;
            r = luma16 + (r - luma16) * satGain;
            g = luma16 + (g - luma16) * satGain;
            b = luma16 + (b - luma16) * satGain;

            // 8. Vibrance
            const double satMax = qMax(r, qMax(g, b));
            const double satMin = qMin(r, qMin(g, b));
            const double colorfulness = (satMax - satMin) / (satMax + 1e-6);
            const double vibGain = 1.0 + vibrance * (1.0 - colorfulness);
            r = luma16 + (r - luma16) * vibGain;
            g = luma16 + (g - luma16) * vibGain;
            b = luma16 + (b - luma16) * vibGain;

            // 9. Temperature / Tint
            r *= redBalance; g *= greenBalance; b *= blueBalance;

            // 10. Per-channel tone curves
            if (hasRLut) r = rLut[clamp16(r)];
            if (hasGLut) g = gLut[clamp16(g)];
            if (hasBLut) b = bLut[clamp16(b)];
            if (hasLumaLut) {
                const double l2  = 0.2126*r + 0.7152*g + 0.0722*b;
                const double l2m = lumaLut[clamp16(l2)];
                const double scale = l2 > 0 ? l2m / l2 : 1.0;
                r *= scale; g *= scale; b *= scale;
            }

            // 11. Mask blend
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
        // ── Convert RGBA64 → fresh continuous BGR mat ─────────────────────
        // Root cause of the red-dot export artefact:
        //   cvtColor(mat, mat, RGB→BGR) on a mat that wraps Qt image data
        //   corrupts the channel layout on certain OpenCV builds when src==dst.
        //   Likewise, the USM loop writing dst[x] while reading src[x] from
        //   the same buffer causes race-like pixel corruption at full resolution.
        // Fix: always use a separate destination mat for every operation, and
        //   clone the work buffer before USM so read and write never alias.
        QImage img8 = image.convertToFormat(QImage::Format_RGB888);
        cv::Mat bgrMat;
        {
            cv::Mat tmp(img8.height(), img8.width(), CV_8UC3,
                        const_cast<uchar*>(img8.constBits()),
                        static_cast<size_t>(img8.bytesPerLine()));
            cv::cvtColor(tmp, bgrMat, cv::COLOR_RGB2BGR); // separate dst — no in-place
        }
        // bgrMat is now a fully continuous, independently-owned BGR mat.
        // img8 is no longer referenced.

        cv::Mat work = bgrMat;

        if (nr > 0.0) {
            const double sigmaColor = 8.0 + nr * 0.40;  // 8–48
            const double sigmaSpace = 4.0 + nr * 0.12;  // 4–16
            cv::Mat filtered;
            cv::bilateralFilter(bgrMat, filtered, -1, sigmaColor, sigmaSpace);
            work = std::move(filtered);
        }

        if (sh > 0.0) {
            const double sigma  = 0.6 + sh * 0.025;           // 0.6–3.1 px
            const double amount = sh * 0.018;                  // 0–1.8 gain
            const int    thresh = static_cast<int>(sh * 0.3); // 0–30

            cv::Mat blurred;
            cv::GaussianBlur(work, blurred, cv::Size(0, 0), sigma);

            // Clone work so read (srcClean) and write (work) never alias.
            // Without this, pixels written early in a row affect the edge
            // computation for subsequent pixels — the actual source of the
            // red-dot artefact on full-resolution exports.
            cv::Mat srcClean = work.clone();
            for (int y = 0; y < work.rows; ++y) {
                const uchar* src = srcClean.ptr<uchar>(y);
                const uchar* blu = blurred.ptr<uchar>(y);
                uchar*       dst = work.ptr<uchar>(y);
                for (int x = 0; x < work.cols * 3; ++x) {
                    const int edge = static_cast<int>(src[x]) - static_cast<int>(blu[x]);
                    if (std::abs(edge) > thresh) {
                        dst[x] = static_cast<uchar>(
                            std::clamp(static_cast<int>(src[x]) +
                                static_cast<int>(std::round(amount * edge)), 0, 255));
                    } else {
                        dst[x] = src[x]; // copy unchanged from the clean src
                    }
                }
            }
        }

        // BGR → RGB with a separate destination mat (no in-place)
        cv::Mat rgbOut;
        cv::cvtColor(work, rgbOut, cv::COLOR_BGR2RGB);
        QImage result(rgbOut.data, rgbOut.cols, rgbOut.rows,
                      static_cast<int>(rgbOut.step), QImage::Format_RGB888);
        image = result.copy().convertToFormat(QImage::Format_RGBA64);
    }
#endif
    return image;
}
} // namespace lumen