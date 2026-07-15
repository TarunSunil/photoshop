#include "storage/ProjectStore.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

namespace lumen {

namespace {

QString connectionName()
{
    return QString("lumenforge-project-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

// Masks are painted/generated in-app (brush strokes, gradient/radial
// fills) rather than loaded from a pre-existing user file the way overlay
// layers are -- there's no original file path to just remember and
// reference the way Layer::sourcePath does. Instead each mask's pixel
// data is written to its own sidecar PNG next to the project file, named
// deterministically from the project file's own name plus the mask's id
// so re-saving the same project overwrites the same file rather than
// accumulating copies. masks.asset_path (schema) / Mask::assetPath
// already existed for exactly this, just never populated.
QString maskSidecarPath(const QString& projectPath, const QString& maskId)
{
    const QFileInfo info(projectPath);
    return info.absolutePath() + "/" + info.completeBaseName() + "_mask_" + maskId + ".png";
}

bool exec(QSqlQuery& query, const QString& sql)
{
    return query.exec(sql);
}

bool createSchema(QSqlDatabase& db)
{
    QSqlQuery query(db);
    return exec(query, "PRAGMA foreign_keys = ON")
        && exec(query, "CREATE TABLE project (id TEXT PRIMARY KEY, format TEXT NOT NULL, version INTEGER NOT NULL)")
        && exec(query, "CREATE TABLE source_assets (id TEXT PRIMARY KEY, path TEXT NOT NULL, width INTEGER, height INTEGER)")
        && exec(query, "CREATE TABLE layers (id TEXT PRIMARY KEY, name TEXT NOT NULL, kind TEXT NOT NULL, source_asset_id TEXT, opacity REAL NOT NULL, visible INTEGER NOT NULL, locked INTEGER NOT NULL, order_index INTEGER NOT NULL, pos_x REAL NOT NULL DEFAULT 0, pos_y REAL NOT NULL DEFAULT 0, scale_x REAL NOT NULL DEFAULT 1, scale_y REAL NOT NULL DEFAULT 1, rotation REAL NOT NULL DEFAULT 0)")
        && exec(query, "CREATE TABLE masks (id TEXT PRIMARY KEY, name TEXT NOT NULL, kind TEXT NOT NULL, asset_path TEXT, feather_radius REAL NOT NULL, inverted INTEGER NOT NULL, target_layer_id TEXT DEFAULT '')")
        && exec(query, "CREATE TABLE adjustments (id TEXT PRIMARY KEY, type TEXT NOT NULL, parameters TEXT NOT NULL, target_layer_id TEXT, target_mask_id TEXT, enabled INTEGER NOT NULL, order_index INTEGER NOT NULL)")
        && exec(query, "CREATE TABLE history_entries (id TEXT PRIMARY KEY, label TEXT NOT NULL, created_at TEXT NOT NULL)")
        && exec(query, "CREATE TABLE presets (id TEXT PRIMARY KEY, name TEXT NOT NULL, payload TEXT NOT NULL)")
        && exec(query, "CREATE TABLE export_jobs (id TEXT PRIMARY KEY, path TEXT NOT NULL, settings TEXT NOT NULL, status TEXT NOT NULL)")
        && exec(query, "CREATE TABLE ai_jobs (id TEXT PRIMARY KEY, model_id TEXT NOT NULL, settings TEXT NOT NULL, status TEXT NOT NULL)");
}

} // namespace

ProjectStore::ProjectStore(QObject* parent)
    : QObject(parent)
{
}

bool ProjectStore::saveProject(const DocumentModel& document, const QString& path) const
{
    if (!document.hasDocument()) {
        return false;
    }

    QFile::remove(path);

    const QString name = connectionName();
    bool ok = false;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(path);
        if (!db.open()) {
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(name);
            return false;
        }

        ok = createSchema(db) && db.transaction();
        QSqlQuery query(db);

        if (ok) {
            query.prepare("INSERT INTO project (id, format, version) VALUES (?, ?, ?)");
            query.addBindValue("default");
            query.addBindValue("lumenforge-project");
            query.addBindValue(1);
            ok = query.exec();
        }

        if (ok) {
            query.prepare("INSERT INTO source_assets (id, path, width, height) VALUES (?, ?, ?, ?)");
            query.addBindValue("source");
            query.addBindValue(document.sourcePath());
            query.addBindValue(document.sourceSize().width());
            query.addBindValue(document.sourceSize().height());
            ok = query.exec();
        }

        for (const Layer& layer : document.layers()) {
            if (!ok) {
                break;
            }
            // Overlay layers (added via Add Layer) carry the file path
            // they were loaded from in Layer::sourcePath -- the base
            // layer's path lives in document.sourcePath() instead and is
            // already written to source_assets above, so sourcePath
            // stays empty for it and its existing sourceAssetId ("source")
            // is used unchanged. Each overlay layer gets its OWN
            // source_assets row here: the exact same mechanism already
            // used for the base image, just applied once per overlay
            // layer instead of a single time, so loadProject() has a real
            // file path to resolve this layer's pixel data back from.
            QString sourceAssetId = layer.sourceAssetId;
            if (!layer.sourcePath.isEmpty()) {
                sourceAssetId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                const QSize imgSize = document.layerImage(layer.id).size();
                query.prepare("INSERT INTO source_assets (id, path, width, height) VALUES (?, ?, ?, ?)");
                query.addBindValue(sourceAssetId);
                query.addBindValue(layer.sourcePath);
                query.addBindValue(imgSize.width());
                query.addBindValue(imgSize.height());
                ok = query.exec();
                if (!ok) {
                    break;
                }
            }
            query.prepare("INSERT INTO layers (id, name, kind, source_asset_id, opacity, visible, locked, order_index, pos_x, pos_y, scale_x, scale_y, rotation) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            query.addBindValue(layer.id);
            query.addBindValue(layer.name);
            query.addBindValue("image");
            query.addBindValue(sourceAssetId);
            query.addBindValue(layer.opacity);
            query.addBindValue(layer.visible ? 1 : 0);
            query.addBindValue(layer.locked ? 1 : 0);
            query.addBindValue(layer.order);
            query.addBindValue(layer.posX);
            query.addBindValue(layer.posY);
            query.addBindValue(layer.scaleX);
            query.addBindValue(layer.scaleY);
            query.addBindValue(layer.rotation);
            ok = query.exec();
        }

        for (const Mask& mask : document.masks()) {
            if (!ok) {
                break;
            }
            // A mask created but never painted (e.g. "+ New Mask" clicked
            // then abandoned) has a null QImage -- nothing to write, and
            // restoreMask() below already handles an empty asset_path by
            // restoring the mask with no pixel data, matching this
            // pre-save state exactly.
            QString maskAssetPath;
            if (!mask.mask.isNull()) {
                maskAssetPath = maskSidecarPath(path, mask.id);
                if (!mask.mask.save(maskAssetPath, "PNG")) {
                    ok = false;
                    break;
                }
            }
            query.prepare("INSERT INTO masks (id, name, kind, asset_path, feather_radius, inverted, target_layer_id) VALUES (?, ?, ?, ?, ?, ?, ?)");
            query.addBindValue(mask.id);
            query.addBindValue(mask.name);
            query.addBindValue("brush");
            query.addBindValue(maskAssetPath);
            query.addBindValue(mask.featherRadius);
            query.addBindValue(mask.inverted ? 1 : 0);
            query.addBindValue(mask.targetLayerId);
            ok = query.exec();
        }

        for (const Adjustment& adjustment : document.adjustments()) {
            if (!ok) {
                break;
            }
            query.prepare("INSERT INTO adjustments (id, type, parameters, target_layer_id, target_mask_id, enabled, order_index) VALUES (?, ?, ?, ?, ?, ?, ?)");
            query.addBindValue(adjustment.id);
            query.addBindValue(adjustmentTypeToString(adjustment.type));
            query.addBindValue(QString::fromUtf8(QJsonDocument(adjustment.parameters).toJson(QJsonDocument::Compact)));
            query.addBindValue(adjustment.targetLayerId);
            query.addBindValue(adjustment.targetMaskId);
            query.addBindValue(adjustment.enabled ? 1 : 0);
            query.addBindValue(adjustment.order);
            ok = query.exec();
        }

        ok = ok ? db.commit() : (db.rollback() && false);
        query = QSqlQuery();
        db.close();
    }

    QSqlDatabase::removeDatabase(name);
    return ok;
}

bool ProjectStore::loadProject(DocumentModel& document, const QString& path) const
{
    const QString name = connectionName();
    bool ok = false;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(path);
        if (!db.open()) {
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(name);
            return false;
        }

        QSqlQuery query(db);
        ok = query.exec("SELECT path FROM source_assets ORDER BY rowid LIMIT 1") && query.next();
        const QString sourcePath = ok ? query.value(0).toString() : QString();
        ok = ok && !sourcePath.isEmpty() && document.openSourceImage(sourcePath);

        // Restore overlay image layers -- these were previously never read
        // back at all (loadProject() never queried the layers table in any
        // form), which is why an added/moved/resized/rotated layer
        // survived a save but vanished on reload. openSourceImage() above
        // already recreated the BASE layer fresh; its saved row is
        // identified by source_asset_id == "source" (the fixed id
        // DocumentModel::openSourceImage() now always assigns it, matching
        // the fixed "source" id its own source_assets row already used)
        // and is skipped here rather than relying on order_index, since
        // the base layer can be reordered like any other layer.
        if (ok && query.exec(
                "SELECT layers.id, layers.name, layers.source_asset_id, layers.opacity, "
                "layers.visible, layers.locked, layers.order_index, "
                "layers.pos_x, layers.pos_y, layers.scale_x, layers.scale_y, layers.rotation, "
                "source_assets.path "
                "FROM layers LEFT JOIN source_assets ON source_assets.id = layers.source_asset_id "
                "ORDER BY layers.order_index")) {
            while (query.next()) {
                const QString assetId = query.value(2).toString();
                if (assetId == "source") {
                    continue; // the base layer -- already recreated above
                }
                const QString assetPath = query.value(12).toString();
                if (assetPath.isEmpty()) {
                    continue; // no resolvable source_assets row -- skip this layer, don't fail the whole load
                }
                QImage img;
                if (!img.load(assetPath)) {
                    continue; // source file moved/deleted since save -- same graceful skip
                }

                Layer layer;
                layer.id            = query.value(0).toString();
                layer.name          = query.value(1).toString();
                layer.kind          = LayerKind::Image;
                layer.sourceAssetId = assetId;
                layer.sourcePath    = assetPath;
                layer.opacity       = query.value(3).toDouble();
                layer.visible       = query.value(4).toInt() != 0;
                layer.locked        = query.value(5).toInt() != 0;
                layer.order         = query.value(6).toInt();
                layer.posX          = query.value(7).toDouble();
                layer.posY          = query.value(8).toDouble();
                layer.scaleX        = query.value(9).toDouble();
                layer.scaleY        = query.value(10).toDouble();
                layer.rotation      = query.value(11).toDouble();
                document.restoreLayer(layer, img);
            }
        }

        // Restore masks -- same previously-missing story as overlay
        // layers, adapted for the fact that masks are painted/generated
        // in-app rather than loaded from a pre-existing file: each mask's
        // pixel data was written to its own sidecar PNG at save time (see
        // saveProject()/maskSidecarPath()). A mask saved with an empty
        // asset_path (never painted before save) restores with no pixel
        // data, matching its pre-save state; a mask whose sidecar file is
        // missing/unreadable (e.g. moved or deleted since save) still
        // restores its other metadata -- id, name, feather radius,
        // inverted flag -- rather than being dropped entirely, since an
        // empty mask slot is a normal, already-supported state (the same
        // one "+ New Mask" produces before any painting happens).
        // Migrate older project files (saved before masks became
        // layer-aware) that don't have this column yet -- SQLite has no
        // "ADD COLUMN IF NOT EXISTS", so just attempt it and ignore
        // failure (which means the column already exists). Existing rows
        // get the column's DEFAULT '' (base image), matching exactly
        // their only possible prior behavior -- masks always affected the
        // base image before this feature existed.
        query.exec("ALTER TABLE masks ADD COLUMN target_layer_id TEXT DEFAULT ''");

        if (ok && query.exec("SELECT id, name, asset_path, feather_radius, inverted, target_layer_id FROM masks")) {
            while (query.next()) {
                Mask mask;
                mask.id            = query.value(0).toString();
                mask.name          = query.value(1).toString();
                mask.kind          = MaskKind::Brush;
                mask.assetPath     = query.value(2).toString();
                mask.featherRadius = query.value(3).toDouble();
                mask.inverted      = query.value(4).toInt() != 0;
                mask.targetLayerId = query.value(5).toString();
                if (!mask.assetPath.isEmpty()) {
                    QImage img;
                    if (img.load(mask.assetPath)) {
                        mask.mask = img;
                    }
                }
                document.restoreMask(mask);
            }
        }

        if (ok && query.exec("SELECT type, parameters FROM adjustments WHERE enabled = 1 ORDER BY order_index")) {
            while (query.next()) {
                const AdjustmentType type = adjustmentTypeFromString(query.value(0).toString());
                const QJsonObject parameters = QJsonDocument::fromJson(query.value(1).toString().toUtf8()).object();
                document.setScalarAdjustment(type, parameters.value("value").toDouble(0.0));
            }
        }

        query = QSqlQuery();
        db.close();
    }

    QSqlDatabase::removeDatabase(name);
    return ok;
}

} // namespace lumen
