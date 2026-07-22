#include "image-core/RenderPipeline.hpp"
#include "image-core/BlendModes.hpp"
#include "image-core/ColorManager.hpp"
#include <QJsonArray>
#include <QJsonValue>
#include <QPainter>
#include <QTransform>
#include <QElapsedTimer>
#include <QDebug>
#include <QtMath>
#include <algorithm>
#ifdef HAVE_OPENCV
#  include <opencv2/core.hpp>
#  include <opencv2/imgproc.hpp>
#  include <opencv2/photo.hpp>
#endif
namespace lumen {
namespace {
quint16 clamp16(double v) { return static_cast<quint16>(qBound(0.0,v,65535.0)); }
double scalarAdj(const QVector<Adjustment>& adjs, AdjustmentType t) {
    for (const auto& a : adjs) if (a.type==t && a.enabled) return a.parameters.value("value").toDouble(0.0);
    return 0.0;
}
const Adjustment* findAdj(const QVector<Adjustment>& adjs, AdjustmentType t) {
    for (const auto& a : adjs) if (a.type==t && a.enabled) return &a;
    return nullptr;
}
} // namespace

// ── Curve LUT ─────────────────────────────────────────────────────────────────

std::vector<double> RenderPipeline::buildCurveLut(const QJsonArray& points) {
    std::vector<double> lut(65536);
    if (points.isEmpty()) { for (int i=0;i<65536;++i) lut[i]=i; return lut; }
    QVector<QPair<double,double>> pts; pts.append({0.0,0.0});
    for (const QJsonValue& v : points) { const QJsonArray p=v.toArray(); if (p.size()>=2) pts.append({p[0].toDouble(),p[1].toDouble()}); }
    pts.append({1.0,1.0});
    std::sort(pts.begin(),pts.end(),[](auto& a,auto& b){return a.first<b.first;});
    for (int i=0;i<65536;++i) {
        const double x=i/65535.0; double y=x;
        for (int k=1;k<pts.size();++k) {
            if (x<=pts[k].first) { const double t=(x-pts[k-1].first)/(pts[k].first-pts[k-1].first+1e-9);
                y=pts[k-1].second+t*(pts[k].second-pts[k-1].second); break; }
        }
        lut[i]=qBound(0.0,y*65535.0,65535.0);
    }
    return lut;
}

// ── Legacy preview paths ───────────────────────────────────────────────────────

QImage RenderPipeline::renderPreview(const DocumentModel& doc, QSize sz, std::shared_ptr<std::atomic<bool>> c) const {
    if (!doc.hasDocument()) return {};
    return renderPreviewFromData(doc.sourceImage(),doc.adjustments(),sz,doc.activeMask(),c);
}

QImage RenderPipeline::renderPreviewFromData(const QImage& src, const QVector<Adjustment>& adjs,
    QSize maxSz, const QImage& mask, std::shared_ptr<std::atomic<bool>> cancelled) const {
    if (src.isNull()) return {};
    QImage s=src;
    if (maxSz.isValid()&&(s.width()>maxSz.width()||s.height()>maxSz.height()))
        s=s.scaled(maxSz,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    QImage sm=mask.isNull()?QImage():mask.scaled(s.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    return applyAdjustments(s,adjs,sm,cancelled);
}

// ── Full pipeline render (issues 5 + 6) ──────────────────────────────────────

QImage RenderPipeline::renderWithLayers(
    const QImage& baseSource,
    const QVector<Adjustment>& globalAdjustments,
    const std::vector<MaskAdjLayer>& maskAdjLayers,
    const QVector<Layer>& overlayLayers,
    const QHash<QString, QImage>& layerImages,
    QSize maximumSize,
    std::shared_ptr<std::atomic<bool>> cancelled) const
{
    const bool profile = qEnvironmentVariableIsSet("LUMEN_PROFILE");
    QElapsedTimer profileTimer;
    if (profile) profileTimer.start();
    if (baseSource.isNull()) return {};

    // -- Step 1: scale source to preview size ----------------------------------
    double previewScale = 1.0;
    QImage scaledSrc = baseSource;
    if (maximumSize.isValid() &&
        (baseSource.width() > maximumSize.width() || baseSource.height() > maximumSize.height())) {
        previewScale = std::min(
            static_cast<double>(maximumSize.width())  / baseSource.width(),
            static_cast<double>(maximumSize.height()) / baseSource.height());
        scaledSrc = baseSource.scaled(maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (cancelled && *cancelled) return {};

    // -- Step 2: apply global adjustments (no mask) ---------------------------
    QImage result = applyAdjustments(scaledSrc, globalAdjustments, {}, cancelled);
    if (result.isNull() || (cancelled && *cancelled)) return {};

    // -- Step 3: apply per-mask local adjustments, BASE-IMAGE-scoped masks
    //    only (issue 5). Masks scoped to a specific overlay layer (see
    //    Mask::targetLayerId / MaskAdjLayer::targetLayerId) must never
    //    affect the base composite -- those are applied to that layer's
    //    own pixels inside compositeOverlayLayers() below instead. -------
    for (const auto& ml : maskAdjLayers) {
        if (!ml.targetLayerId.isEmpty()) continue;
        if (cancelled && *cancelled) return {};
        if (ml.adjustments.isEmpty()) continue;
        QImage scaledMask = ml.mask.isNull() ? QImage()
            : ml.mask.scaled(result.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        result = applyAdjustments(result, ml.adjustments, scaledMask, cancelled);
        if (result.isNull()) return {};
    }

    // -- Step 4: composite overlay layers (issue 6), each applying its own
    //    layer-scoped masked adjustments to its own pixels first --------
    if (overlayLayers.size() > 1) {
        compositeOverlayLayers(result, overlayLayers, layerImages, previewScale, maskAdjLayers);
    }

    if (profile)
        qInfo().noquote() << "PROFILE renderWithLayers ms="
                          << profileTimer.nsecsElapsed()/1000000.0
                          << "size=" << result.size()
                          << "layers=" << overlayLayers.size();
    return result;
}

// ── Overlay layer compositing (issue 6) ───────────────────────────────────────

QTransform RenderPipeline::canvasToLayerLocalTransform(
    const Layer& layer, QSize baseImageSize, QSize nativeImgSize, bool* ok)
{
    if (ok) *ok = false;
    if (nativeImgSize.width() <= 0 || nativeImgSize.height() <= 0)
        return QTransform();

    const double cxNative = baseImageSize.width()  * 0.5 + layer.posX;
    const double cyNative = baseImageSize.height() * 0.5 + layer.posY;
    const double dwNative = nativeImgSize.width()  * layer.scaleX;
    const double dhNative = nativeImgSize.height() * layer.scaleY;
    if (dwNative < 1.0 || dhNative < 1.0) return QTransform();

    QTransform localToBase;
    localToBase.translate(cxNative, cyNative);
    if (!qFuzzyIsNull(layer.rotation)) localToBase.rotate(layer.rotation);
    localToBase.translate(-dwNative * 0.5, -dhNative * 0.5);
    localToBase.scale(dwNative / nativeImgSize.width(), dhNative / nativeImgSize.height());

    if (ok) *ok = true;
    return localToBase.inverted();
}

void RenderPipeline::compositeOverlayLayers(
    QImage& canvas,
    const QVector<Layer>& layers,
    const QHash<QString, QImage>& layerImages,
    double scale,
    const std::vector<MaskAdjLayer>& maskAdjLayers) const
{
    const bool profile = qEnvironmentVariableIsSet("LUMEN_PROFILE");
    QElapsedTimer profileTimer;
    if (profile) profileTimer.start();
    // Sort by order so base (order==0) is bottom, overlays are on top.
    QVector<Layer> sorted = layers;
    std::sort(sorted.begin(), sorted.end(),
              [](const Layer& a, const Layer& b){ return a.order < b.order; });

    // QPainter needs a premultiplied surface for correct alpha compositing.
    QImage comp = canvas.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&comp);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    for (const Layer& layer : sorted) {
        if (layer.isBaseLayer() || !layer.visible) continue;  // skip base layer
        QImage img = layerImages.value(layer.id);
        if (img.isNull()) continue;

        // Apply this layer's OWN masked adjustments to its OWN pixels
        // before compositing. Layer-scoped masks are already baked into
        // THIS layer's own native pixel space at paint-commit time (see
        // DocumentController::bakeMaskForTarget()) -- they are applied
        // directly here, with no further warping, and are therefore
        // naturally carried along by the SAME placement transform used
        // to draw `img` onto the canvas below. This is the fix for
        // "overlay masks don't follow the overlay": the previous design
        // re-derived the mask's placement from the layer's CURRENT
        // transform on every render (via what's now
        // canvasToLayerLocalTransform()), which meant moving the layer
        // sampled "whatever's now under the new position" instead of the
        // mask staying attached to the layer's own surface.
        for (const auto& ml : maskAdjLayers) {
            if (ml.targetLayerId != layer.id || ml.adjustments.isEmpty() || ml.mask.isNull()) continue;
            const QImage localMask = (ml.mask.size() == img.size())
                ? ml.mask
                : ml.mask.scaled(img.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            img = applyAdjustments(img, ml.adjustments, localMask);
        }

        painter.save();
        painter.setOpacity(layer.opacity);

        // Centre of placement in canvas coords.
        // posX/posY are in BASE-IMAGE pixels; scale converts to canvas pixels.
        const double cx = comp.width()  * 0.5 + layer.posX * scale;
        const double cy = comp.height() * 0.5 + layer.posY * scale;

        // Destination size at current preview scale.
        const int dw = qMax(1, qRound(img.width()  * layer.scaleX * scale));
        const int dh = qMax(1, qRound(img.height() * layer.scaleY * scale));

        painter.translate(cx, cy);
        if (!qFuzzyIsNull(layer.rotation))
            painter.rotate(layer.rotation);
        painter.drawImage(QRectF(-dw * 0.5, -dh * 0.5, dw, dh), img);
        painter.restore();
    }
    painter.end();

    // Convert back to RGBA64 to stay consistent with the rest of the pipeline.
    canvas = comp.convertToFormat(QImage::Format_RGBA64);
    if (profile)
        qInfo().noquote() << "PROFILE compositeOverlayLayers ms="
                          << profileTimer.nsecsElapsed()/1000000.0
                          << "layers=" << layers.size();
}

// ── Full-resolution export ────────────────────────────────────────────────────

QImage RenderPipeline::renderFullResolution(const DocumentModel& doc) const {
    if (!doc.hasDocument()) return {};

    // Global adjustments are those with targetMaskId=="" — adjustmentsForLayer()
    // filters on targetLayerId instead, which every slider-set adjustment leaves
    // empty, so it was returning EVERY adjustment (global AND mask-scoped) and
    // applying it to the whole image. That double-applied mask-scoped edits
    // (once here, once more in the old per-mask loop below) and made exported
    // files not match the on-screen preview, which already uses
    // adjustmentsForTarget("") correctly. Use the same call here.
    const QVector<Adjustment> globalAdjustments = doc.adjustmentsForTarget(QString());

    std::vector<MaskAdjLayer> maskAdjLayers;
    for (const Mask& mask : doc.masks()) {
        if (mask.mask.isNull()) continue;
        const QVector<Adjustment> maskAdjs = doc.adjustmentsForTarget(mask.id);
        if (maskAdjs.isEmpty()) continue;
        maskAdjLayers.push_back({mask.mask, maskAdjs, mask.targetLayerId});
    }

    const QVector<Layer> layers = doc.layers();
    QHash<QString, QImage> layerImages;
    for (const Layer& layer : layers)
        layerImages.insert(layer.id, doc.layerImage(layer.id));

    // QSize() is deliberately invalid (not QSize(0,0)) so renderWithLayers'
    // "only downscale if maximumSize.isValid()" check is skipped entirely,
    // producing a native-resolution render through the exact same
    // adjustment/mask/overlay pipeline DocumentController::rebuildPreview()
    // and buildHqPreview() already use for the on-screen preview -- including
    // compositeOverlayLayers(), which this export path never called before.
    return renderWithLayers(doc.sourceImage(), globalAdjustments, maskAdjLayers,
                             layers, layerImages, QSize());
}

// ── Blend onto canvas ─────────────────────────────────────────────────────────

void RenderPipeline::blendOnto(QImage& canvas,const QImage& layer,BlendMode mode,double opacity) const {
    const int W=qMin(canvas.width(),layer.width()),H=qMin(canvas.height(),layer.height());
    for (int y=0;y<H;++y) {
        auto* dst=reinterpret_cast<QRgba64*>(canvas.scanLine(y));
        const auto* src=reinterpret_cast<const QRgba64*>(layer.constScanLine(y));
        for (int x=0;x<W;++x) {
            const double bR=dst[x].red()/65535.0,bG=dst[x].green()/65535.0,bB=dst[x].blue()/65535.0,bA=dst[x].alpha()/65535.0;
            const double lR=src[x].red()/65535.0,lG=src[x].green()/65535.0,lB=src[x].blue()/65535.0,lA=src[x].alpha()/65535.0;
            const double eff=opacity*lA;
            dst[x]=QRgba64::fromRgba64(clamp16(blend::compose(bR,lR,eff,mode)*65535),clamp16(blend::compose(bG,lG,eff,mode)*65535),
                clamp16(blend::compose(bB,lB,eff,mode)*65535),clamp16((bA+lA*opacity*(1.0-bA))*65535));
        }
    }
}

// ── Core pixel pipeline ───────────────────────────────────────────────────────

QImage RenderPipeline::applyAdjustments(QImage image,const QVector<Adjustment>& adjustments,
    const QImage& mask,std::shared_ptr<std::atomic<bool>> cancelled) const {
    image=image.convertToFormat(QImage::Format_RGBA64);
    const int rot=int(scalarAdj(adjustments,AdjustmentType::RotationDegrees))%360;
    const bool fh=scalarAdj(adjustments,AdjustmentType::FlipHorizontal)>0.5;
    const bool fv=scalarAdj(adjustments,AdjustmentType::FlipVertical)>0.5;
    if (fh||fv) image=image.mirrored(fh,fv);
    if (rot!=0) { QTransform t; t.rotate(rot); image=image.transformed(t,Qt::SmoothTransformation); }

    const double combinedGain = qPow(2.0, scalarAdj(adjustments,AdjustmentType::Exposure))
                               * qPow(2.0, scalarAdj(adjustments,AdjustmentType::Brightness)/100.0);
    const double cg  = 1.0+scalarAdj(adjustments,AdjustmentType::Contrast)/100.0;
    const double hl  = scalarAdj(adjustments,AdjustmentType::Highlights)/100.0;
    const double sh  = scalarAdj(adjustments,AdjustmentType::Shadows)/100.0;
    const double wg  = 1.0+scalarAdj(adjustments,AdjustmentType::Whites)*0.005;
    const double bof = (scalarAdj(adjustments,AdjustmentType::Blacks)/100.0)*0.15*65535.0;
    const double sg  = 1.0+scalarAdj(adjustments,AdjustmentType::Saturation)/100.0;
    const double vib = scalarAdj(adjustments,AdjustmentType::Vibrance)/100.0;
    const double tmp = scalarAdj(adjustments,AdjustmentType::Temperature);
    const double tnt = scalarAdj(adjustments,AdjustmentType::Tint);
    const double rbal=1.0+tmp/400.0, bbal=1.0-tmp/400.0, gbal=1.0+tnt/400.0;

    bool hL=false,hR=false,hG=false,hB=false;
    std::vector<double> lumaLut,rLut,gLut,bLut;
    if (const Adjustment* a=findAdj(adjustments,AdjustmentType::ToneCurveLuma)){lumaLut=buildCurveLut(a->parameters.value("points").toArray());hL=true;}
    if (const Adjustment* a=findAdj(adjustments,AdjustmentType::ToneCurveR)){rLut=buildCurveLut(a->parameters.value("points").toArray());hR=true;}
    if (const Adjustment* a=findAdj(adjustments,AdjustmentType::ToneCurveG)){gLut=buildCurveLut(a->parameters.value("points").toArray());hG=true;}
    if (const Adjustment* a=findAdj(adjustments,AdjustmentType::ToneCurveB)){bLut=buildCurveLut(a->parameters.value("points").toArray());hB=true;}

    const bool hasMask=!mask.isNull();
    QImage sm=hasMask?mask.scaled(image.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32):QImage();

    for (int y=0;y<image.height();++y) {
        if (cancelled&&*cancelled) return {};
        auto* sc=reinterpret_cast<QRgba64*>(image.scanLine(y));
        for (int x=0;x<image.width();++x) {
            const QRgba64 op=sc[x];
            double r=op.red()*combinedGain, g=op.green()*combinedGain, b=op.blue()*combinedGain;
            r=(r*wg)+bof; g=(g*wg)+bof; b=(b*wg)+bof;
            r=((r/65535.0-0.5)*cg+0.5)*65535.0; g=((g/65535.0-0.5)*cg+0.5)*65535.0; b=((b/65535.0-0.5)*cg+0.5)*65535.0;
            double lm=(0.2126*r+0.7152*g+0.0722*b)/65535.0;
            if (lm>0.5){double bl=(lm-0.5)*2.0,gn=1.0+hl*bl; r*=gn;g*=gn;b*=gn;}
            lm=(0.2126*r+0.7152*g+0.0722*b)/65535.0;
            if (lm<0.5){double bl=(0.5-lm)*2.0,gn=1.0+sh*bl; r*=gn;g*=gn;b*=gn;}
            const double l16=0.2126*r+0.7152*g+0.0722*b;
            r=l16+(r-l16)*sg; g=l16+(g-l16)*sg; b=l16+(b-l16)*sg;
            const double sm2=qMax(r,qMax(g,b)),smi=qMin(r,qMin(g,b));
            const double cf=(sm2-smi)/(sm2+1e-6),vg=1.0+vib*(1.0-cf);
            r=l16+(r-l16)*vg; g=l16+(g-l16)*vg; b=l16+(b-l16)*vg;
            r*=rbal; g*=gbal; b*=bbal;
            if (hR) r=rLut[clamp16(r)]; if (hG) g=gLut[clamp16(g)]; if (hB) b=bLut[clamp16(b)];
            if (hL){const double l2=0.2126*r+0.7152*g+0.0722*b,l2m=lumaLut[clamp16(l2)],sc2=l2>0?l2m/l2:1.0; r*=sc2;g*=sc2;b*=sc2;}
            if (hasMask){
                const QRgb mp=reinterpret_cast<const QRgb*>(sm.constScanLine(y))[x];
                const double al=qAlpha(mp)/255.0;
                r=op.red()*(1.0-al)+r*al; g=op.green()*(1.0-al)+g*al; b=op.blue()*(1.0-al)+b*al;
            }
            sc[x]=QRgba64::fromRgba64(clamp16(r),clamp16(g),clamp16(b),op.alpha());
        }
    }
#ifdef HAVE_OPENCV
    const double nr=scalarAdj(adjustments,AdjustmentType::NoiseReduction);
    const double shp=scalarAdj(adjustments,AdjustmentType::Sharpening);
    if (nr>0.0||shp>0.0) {
        QImage img8=image.convertToFormat(QImage::Format_RGB888);
        cv::Mat bgrMat;
        { cv::Mat tmp(img8.height(),img8.width(),CV_8UC3,const_cast<uchar*>(img8.constBits()),static_cast<size_t>(img8.bytesPerLine()));
          cv::cvtColor(tmp,bgrMat,cv::COLOR_RGB2BGR); }
        cv::Mat work=bgrMat;
        if (nr>0.0){cv::Mat f; cv::bilateralFilter(bgrMat,f,-1,8.0+nr*0.4,4.0+nr*0.12); work=std::move(f);}
        if (shp>0.0){
            const double sigma=0.6+shp*0.025,amount=shp*0.018;
            const int thresh=static_cast<int>(shp*0.3);
            cv::Mat blurred; cv::GaussianBlur(work,blurred,cv::Size(0,0),sigma);
            cv::Mat sc2=work.clone();
            for (int y=0;y<work.rows;++y){
                const uchar* s=sc2.ptr<uchar>(y); const uchar* bl=blurred.ptr<uchar>(y); uchar* d=work.ptr<uchar>(y);
                for (int x=0;x<work.cols*3;++x){
                    const int e=int(s[x])-int(bl[x]);
                    d[x]=std::abs(e)>thresh?static_cast<uchar>(std::clamp(int(s[x])+int(std::round(amount*e)),0,255)):s[x];
                }
            }
        }
        cv::Mat ro; cv::cvtColor(work,ro,cv::COLOR_BGR2RGB);
        QImage res(ro.data,ro.cols,ro.rows,static_cast<int>(ro.step),QImage::Format_RGB888);
        image=res.copy().convertToFormat(QImage::Format_RGBA64);
    }
#endif
    return image;
}
} // namespace lumen
