# LumenForge

> A native C++/Qt6 desktop photo editor with an ONNX Runtime AI inference pipeline, raw file support, and GPU-accelerated image processing.

[![C++](https://img.shields.io/badge/C++-17-00599C?style=flat-square&logo=cplusplus&logoColor=white&labelColor=0f0f0f)](https://isocpp.org/)
[![Qt](https://img.shields.io/badge/Qt-6-41CD52?style=flat-square&logo=qt&logoColor=white&labelColor=0f0f0f)](https://www.qt.io/)
[![ONNX Runtime](https://img.shields.io/badge/ONNX_Runtime-inference-005CED?style=flat-square&labelColor=0f0f0f)](https://onnxruntime.ai/)
[![OpenCV](https://img.shields.io/badge/OpenCV-4-5C3EE8?style=flat-square&logo=opencv&logoColor=white&labelColor=0f0f0f)](https://opencv.org/)
[![CMake](https://img.shields.io/badge/CMake-build-064F8C?style=flat-square&logo=cmake&logoColor=white&labelColor=0f0f0f)](https://cmake.org/)
[![License](https://img.shields.io/badge/license-MIT-c8b89a?style=flat-square&labelColor=0f0f0f)](LICENSE)

---

## What it is

LumenForge is a desktop photo editor built entirely in C++ and Qt6. It handles everything natively — raw file decoding, non-destructive adjustment pipelines, and AI-powered image enhancement — without any Python runtime or cloud API calls. The inference pipeline runs locally via ONNX Runtime, targeting both CPU and GPU execution providers.

This is a personal project built to push C++ systems knowledge and explore native AI inference outside the typical Python ML stack.

---

## Core features

- **Raw file support** — LibRaw decodes CR2, NEF, ARW, DNG, and other camera raw formats with full metadata
- **Non-destructive editing** — adjustment layers (exposure, contrast, curves, HSL, white balance) are stacked as a node graph and applied on export
- **AI inference** — ONNX Runtime pipeline for enhancement tasks (denoising, super-resolution, style transfer); runs on DirectML / CUDA / CPU automatically
- **GPU acceleration** — OpenCV CUDA / OpenCL backend for convolution-heavy operations
- **Export** — JPEG, PNG, TIFF with quality controls; preserves EXIF metadata

---

## Architecture

```
LumenForge/
├── src/
│   ├── core/           # Image pipeline, LUT engine, non-destructive node graph
│   ├── inference/      # ONNX Runtime session management, model loader
│   ├── io/             # LibRaw decoder, EXIF reader/writer, export encoder
│   ├── ui/             # Qt6 widgets, canvas renderer, panel layouts
│   └── utils/          # Memory pool, thread queue, timer utilities
├── models/             # Bundled ONNX model files
├── CMakeLists.txt
└── vcpkg.json          # Dependency manifest
```

**Pipeline flow:**

```
Raw file (LibRaw) → Linear float32 buffer → Adjustment node graph
    → AI inference pass (ONNX Runtime) → OpenCV post-process
    → Qt6 display (QImage / OpenGL) → Export encoder
```

---

## Dependencies

Managed via **vcpkg**. All dependencies are declared in `vcpkg.json`.

| Library | Purpose |
|---|---|
| Qt 6.x | UI framework, canvas rendering |
| ONNX Runtime | AI model inference (CPU / DirectML / CUDA) |
| LibRaw | Raw camera file decoding |
| OpenCV 4.x | Image processing, GPU acceleration |
| CMake 3.28+ | Build system |

---

## Building

### Prerequisites

- CMake ≥ 3.28
- vcpkg (bootstrapped and `VCPKG_ROOT` set)
- Qt6 installed (via Qt Installer or vcpkg)
- C++17-capable compiler (MSVC 2022 / GCC 13 / Clang 17)
- *(Optional)* CUDA Toolkit for GPU inference

### Steps

```bash
git clone https://github.com/TarunSunil/LumenForge.git
cd LumenForge

# Install dependencies via vcpkg
vcpkg install

# Configure
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```
Run the app:

```powershell
.\build\app\LumenForge.exe
```


> **Note on file encoding:** All source files use UTF-8 without BOM. If you copy-paste code from web sources, verify no Unicode control characters (U+2028, U+2029, U+202C) have been introduced — MSVC silently miscompiles these.

### Windows (PowerShell safe pattern for file edits)

```powershell
# Use this instead of Get-Content/Set-Content to avoid UTF-16 LE re-encoding
[System.IO.File]::WriteAllText("path\to\file.cpp", $content, [System.Text.UTF8Encoding]::new($false))
```

---

## Status

Active development. Core adjustment pipeline and LibRaw decoding are functional. ONNX inference integration is in progress.

---

## License

MIT
