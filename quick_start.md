# LumenForge Quick Start

LumenForge is a native Qt 6 desktop photo editor. It is not a web app and does not need a server to run.

## 1. Install Prerequisites

Install these tools on Windows:

- Qt 6.5 or newer, including Qt Quick, Qt Quick Controls 2, Qt SQL, and Qt Concurrent.
- CMake 3.24 or newer.
- Ninja build system.
- A C++20 compiler, such as MSVC from Visual Studio Build Tools 2022.

Make sure `cmake` and `ninja` are available in PowerShell:

```powershell
cmake --version
ninja --version
```

If Qt is not automatically found, open the "Qt 6.x for Desktop" developer shell, or pass `CMAKE_PREFIX_PATH` when configuring:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\msvc2019_64"
```

Adjust the Qt path to match your installation.

## 2. Build

From the repository root:

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

## 3. Run

Run the desktop app:

```powershell
.\build\app\LumenForge.exe
```

If your generator or Qt version places the executable elsewhere, search under `build` for `LumenForge.exe`.

## 4. Basic Workflow

1. Click `Open` and choose a JPG, PNG, WebP, TIFF, or BMP image.
2. Use the canvas controls for `Fit`, `100%`, zoom in/out, and before/after.
3. Use the right panel for exposure, tone, color, transform, undo, and redo.
4. Click `Save` to write a local `.lfproj` project database.
5. Click `Project` to reopen a saved project.
6. Click `Export` to render the original source plus the edit graph to PNG, JPG, or WebP.

Keyboard shortcuts:

- `Ctrl+O`: open image
- `Ctrl+S`: save project
- `Ctrl+E`: export
- `Ctrl+Z`: undo
- `Ctrl+Y`: redo
- `Ctrl+0`: 100% zoom
- `Ctrl++` / `Ctrl+-`: zoom
- `\`: before/after toggle

## 5. Current Limitations

- Local Qt/CMake tooling is required; no installer is packaged yet.
- RAW import, masking, and local AI tools are planned but not complete.
- Preview rendering is backgrounded, but there is not yet cancellation or tiled rendering.
- Project files currently reference the original source path, so keep the source image available beside saved projects.
