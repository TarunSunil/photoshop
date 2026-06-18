# Quick Start Guide

## First Time Setup

1. **Clone the repository** (if using git):
```bash
git clone <repository-url> LumenForge
cd LumenForge
```

2. **Download AI Models**: The models are large and should be downloaded manually:
   - `models/mobile_sam.onnx` (~40 MB)
   - `models/big-lama.onnx` (~207 MB)  
   - `models/realesrgan-x4plus.onnx` (~67 MB)

3. **Build the project**:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

4. **Run the application**:
```bash
./LumenForge.exe  # Windows
# or ./bin/LumenForge on Linux/macOS
```

## Basic Usage

- Open images via File → Open
- Use brush/eraser tools from the toolbar
- Apply AI models: Select, Remove Object, Upscale
- Save your work with layers support

## Troubleshooting

**Models not found**: Ensure all `.onnx` files are in `models/` directory.

**Build errors**: Make sure Qt 6.x is installed and CMake can find it.
