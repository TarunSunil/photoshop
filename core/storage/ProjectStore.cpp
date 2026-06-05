#include "storage/ProjectStore.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

namespace lumen {

namespace {

QString connectionName()
{
    return QString("lumenforge-project-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
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
        && exec(query, "CREATE TABLE layers (id TEXT PRIMARY KEY, name TEXT NOT NULL, kind TEXT NOT NULL, source_asset_id TEXT, opacity REAL NOT NULL, visible INTEGER NOT NULL, locked INTEGER NOT NULL, order_index INTEGER NOT NULL)")
        && exec(query, "CREATE TABLE masks (id TEXT PRIMARY KEY, name TEXT NOT NULL, kind TEXT NOT NULL, asset_path TEXT, feather_radius REAL NOT NULL, inverted INTEGER NOT NULL)")
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
            query.prepare("INSERT INTO layers (id, name, kind, source_asset_id, opacity, visible, locked, order_index) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            query.addBindValue(layer.id);
            query.addBindValue(layer.name);
            query.addBindValue("image");
            query.addBindValue(layer.sourceAssetId);
            query.addBindValue(layer.opacity);
            query.addBindValue(layer.visible ? 1 : 0);
            query.addBindValue(layer.locked ? 1 : 0);
            query.addBindValue(layer.order);
            ok = query.exec();
        }

        for (const Mask& mask : document.masks()) {
            if (!ok) {
                break;
            }
            query.prepare("INSERT INTO masks (id, name, kind, asset_path, feather_radius, inverted) VALUES (?, ?, ?, ?, ?, ?)");
            query.addBindValue(mask.id);
            query.addBindValue(mask.name);
            query.addBindValue("brush");
            query.addBindValue(mask.assetPath);
            query.addBindValue(mask.featherRadius);
            query.addBindValue(mask.inverted ? 1 : 0);
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
