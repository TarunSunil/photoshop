#include "RawImporter.hpp"
#include <LibRaw/LibRaw.h>
#include <QDebug>

QImage RawImporter::importRawFile(const QString &filePath) {
    LibRaw *raw = new LibRaw();
    
    if (raw->open_raw_file(filePath.toStdString().c_str()) != 0) {
        emit importComplete({}, "Failed to open RAW file");
        return {};
    }
    
    // Process RAW data and convert to RGBA64 QImage...
    raw->process_image(1); // Apply basic processing
    
    delete raw;
}
