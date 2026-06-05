# LumenForge

LumenForge is an offline-first native desktop photo editor scaffolded around a non-destructive edit graph. The current build is a Qt 6/C++20 foundation for the app shell, image loading, preview rendering, scalar adjustments, project save/load, and export.

## Current Milestone

- Qt 6 + QML desktop shell with tool rail, canvas, adjustments panel, and bottom workspace.
- Source images are opened into a document model and kept unchanged.
- Adjustments are stored as parameters, then rendered from the source image.
- Preview rendering uses a reduced-size render path.
- Project files are local SQLite `.lfproj` databases with source, layer, mask, adjustment, history, preset, export job, and AI job tables.
- Export renders from the original source plus the current edit graph.

## Build

Install Qt 6.5 or newer and CMake 3.24 or newer, then run:

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

Run the app:

```powershell
.\build\app\LumenForge.exe
```

This workspace currently does not have `cmake`, `ninja`, or Qt tools on PATH, so the first verification here is static. A machine with Qt 6 installed should be used for the first compile.

## Roadmap

1. Complete image viewer ergonomics: accurate fit-to-screen, pan, rotate, crop, and before/after.
2. Move preview rendering onto background workers and add undo/redo commands.
3. Add an adjacent project asset directory for copied sources, masks, previews, and autosave snapshots.
4. Add OpenCV/Little CMS/LibRaw integrations behind `image-core`.
5. Add brush and parametric masks in `mask-core`.
6. Add ONNX Runtime model registry and local inference queue in `ai-core`.

## Offline Policy

The app is designed so core editing does not depend on network access or paid APIs. Future AI tools should load local ONNX models from disk and provide CPU/GPU fallback without cloud calls.
