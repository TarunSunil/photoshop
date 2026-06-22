#include "export-core/ExportService.hpp"
#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>
namespace lumen {
namespace {
// RenderPipeline keeps everything in Format_RGBA64 (16-bit/channel) for
// precision. That's correct internally but causes two problems if written
// straight to disk:
//
// 1. Export size explosion: Qt's PNG quality scale is INVERTED vs JPEG --
//    quality 100 = no compression, quality 0 = max compression.
//    ExportService was passing quality=92, which on PNG means ~level-1
//    zlib (barely any compression). A 12MP image at 16-bit with level-1
//    PNG compression is 500MB+. Converting to 8-bit first brings a
//    typical photo export to 2-10MB.
//
// 2. Compatibility: 16-bit-per-channel PNG is spec-valid but many common
//    viewers (Windows Photos, some older apps) struggle with it or show
//    a broken thumbnail before the full decoder kicks in.
//
// Convert to 8-bit ARGB32 exactly once, right before writing. All
// intermediate pipeline math stays at 16-bit.
QImage toExportFormat(const QImage& rendered)
{
    return rendered.convertToFormat(QImage::Format_ARGB32);
}
// PNG quality=92 means almost no compression. Use format-appropriate
// defaults: JPEG at 92 is "high quality", PNG at -1 is Qt's default
// zlib level 6 (good balance of size and speed), WebP at 85 is high.
int qualityFor(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "jpg" || ext == "jpeg") return 92;
    if (ext == "webp")                 return 85;
    return -1;  // PNG and everything else: Qt default (level 6 for PNG)
}
} // namespace
ExportService::ExportService(QObject* parent) : QObject(parent) {}
bool ExportService::exportImage(const DocumentModel& document,
                                 const QString& path, int /*quality*/) const
{
    const QImage rendered = m_renderPipeline.renderFullResolution(document);
    if (rendered.isNull()) return false;
    // Ignore the caller's quality param; use format-appropriate default.
    return toExportFormat(rendered).save(path, nullptr, qualityFor(path));
}
void ExportService::exportBatch(const DocumentModel& document,
                                 const QString& directory,
                                 const QStringList& formats)
{
    const QString baseName = QFileInfo(document.sourcePath()).baseName();
    const QImage rendered  = m_renderPipeline.renderFullResolution(document);
    if (rendered.isNull()) { emit batchFailed("Render failed"); return; }
    const QImage exportReady = toExportFormat(rendered);
    QtConcurrent::run([this, exportReady, directory, baseName, formats]() {
        bool allOk = true;
        for (const QString& fmt : formats) {
            const QString path = directory + "/" + baseName + "." + fmt.toLower();
            if (!exportReady.save(path, nullptr, qualityFor(path))) allOk = false;
        }
        QMetaObject::invokeMethod(this, [this, allOk]() {
            if (allOk) emit batchComplete();
            else       emit batchFailed("One or more formats failed");
        }, Qt::QueuedConnection);
    });
}
} // namespace lumen
