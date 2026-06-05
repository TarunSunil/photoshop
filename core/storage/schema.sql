PRAGMA foreign_keys = ON;

CREATE TABLE project (
    id TEXT PRIMARY KEY,
    format TEXT NOT NULL,
    version INTEGER NOT NULL
);

CREATE TABLE source_assets (
    id TEXT PRIMARY KEY,
    path TEXT NOT NULL,
    width INTEGER,
    height INTEGER
);

CREATE TABLE layers (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    kind TEXT NOT NULL,
    source_asset_id TEXT,
    opacity REAL NOT NULL,
    visible INTEGER NOT NULL,
    locked INTEGER NOT NULL,
    order_index INTEGER NOT NULL
);

CREATE TABLE masks (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    kind TEXT NOT NULL,
    asset_path TEXT,
    feather_radius REAL NOT NULL,
    inverted INTEGER NOT NULL
);

CREATE TABLE adjustments (
    id TEXT PRIMARY KEY,
    type TEXT NOT NULL,
    parameters TEXT NOT NULL,
    target_layer_id TEXT,
    target_mask_id TEXT,
    enabled INTEGER NOT NULL,
    order_index INTEGER NOT NULL
);

CREATE TABLE history_entries (
    id TEXT PRIMARY KEY,
    label TEXT NOT NULL,
    created_at TEXT NOT NULL
);

CREATE TABLE presets (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    payload TEXT NOT NULL
);

CREATE TABLE export_jobs (
    id TEXT PRIMARY KEY,
    path TEXT NOT NULL,
    settings TEXT NOT NULL,
    status TEXT NOT NULL
);

CREATE TABLE ai_jobs (
    id TEXT PRIMARY KEY,
    model_id TEXT NOT NULL,
    settings TEXT NOT NULL,
    status TEXT NOT NULL
);
