#include "image-core/RawImporter.hpp"
#ifdef HAVE_LIBRAW
#  include <libraw/libraw.h>
#endif
namespace lumen {
QStringList RawImporter::supportedExtensions()
{
    return {"cr2","cr3","nef","arw","dng","raf","orf","rw2","pef","srw","nrw"};
}
QImage RawImporter::load(const QString& path)
{
#ifdef HAVE_LIBRAW
    libraw_data_t* raw = libraw_init(0);
    if (!raw) return {};
    if (libraw_open_file(raw, path.toLocal8Bit().constData()) != LIBRAW_SUCCESS) {
        libraw_close(raw);
        return {};
    }
    raw->params.use_camera_wb  = 1;
    raw->params.output_bps     = 16;
    raw->params.no_auto_bright = 1;
    raw->params.gamm[0]        = 1.0;
    raw->params.gamm[1]        = 1.0;
    raw->params.output_color   = 1; // sRGB
    if (libraw_unpack(raw) != LIBRAW_SUCCESS ||
        libraw_dcraw_process(raw) != LIBRAW_SUCCESS) {
        libraw_close(raw);
        return {};
    }
    int errCode = 0;
    libraw_processed_image_t* img = libraw_dcraw_make_mem_image(raw, &errCode);
    if (!img || errCode != LIBRAW_SUCCESS) {
        libraw_close(raw);
        return {};
    }
    const int W = img->width;
    const int H = img->height;
    QImage result(W, H, QImage::Format_RGBA64);
    const quint16* src = reinterpret_cast<const quint16*>(img->data);
    for (int y = 0; y < H; ++y) {
        auto* dst = reinterpret_cast<QRgba64*>(result.scanLine(y));
        for (int x = 0; x < W; ++x) {
            const quint16 r = src[(y * W + x) * 3 + 0];
            const quint16 g = src[(y * W + x) * 3 + 1];
            const quint16 b = src[(y * W + x) * 3 + 2];
            dst[x] = QRgba64::fromRgba64(r, g, b, 65535);
        }
    }
    libraw_dcraw_clear_mem(img);
    libraw_close(raw);
    return result;
#else
    Q_UNUSED(path)
    return {};
#endif
}
} // namespace lumen