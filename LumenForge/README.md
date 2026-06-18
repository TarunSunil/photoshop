# LumenForge - A Modern Photoshop Alternative Built with QML/C++

LumenForge is a powerful image editing application built using Qt Quick (QML), C++, and ONNX AI models, providing features like:
- Advanced brush/eraser tools with pressure sensitivity
- Mask-based selection system  
- RAW file support via LibRaw
- AI-powered object removal (MobileSAM)
- Image upscaling (Real-ESRGAN)
- Inpainting capabilities (LaMa)

## Requirements
- Qt 6.x with QML module
- CMake 3.16+
- ONNX Runtime
- OpenCV
- LibRaw, Little CMS for RAW support

## Building
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Running
The application will be in `build/app/LumenForge.exe` (Windows) or the appropriate binary on Linux/macOS.

## Models Required
Download these ONNX models and place them in the `models/` directory:
- `mobile_sam.onnx` (~40 MB) - Object detection/masking
- `big-lama.onnx` (~207 MB) - Inpainting
- `realesrgan-x4plus.onnx` (~67 MB) - Image upscaling

## License
This project is licensed under the MIT License. See LICENSE.txt for details.
