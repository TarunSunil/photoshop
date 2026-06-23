set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
qt_add_library(lumen_core STATIC
    shared-types/Adjustment.hpp
    shared-types/Layer.hpp
    shared-types/Mask.hpp
    editor-core/DocumentModel.cpp
    editor-core/DocumentModel.hpp
    image-core/RenderPipeline.cpp
    image-core/RenderPipeline.hpp
    image-core/BlendModes.hpp
    image-core/RawImporter.cpp
    image-core/RawImporter.hpp
    image-core/ColorManager.cpp
    image-core/ColorManager.hpp
    mask-core/BrushEngine.cpp
    mask-core/BrushEngine.hpp
    mask-core/MaskDocument.cpp
    mask-core/MaskDocument.hpp
    ai-core/AiRuntime.cpp
    ai-core/AiRuntime.hpp
    ai-core/OnnxSession.cpp
    ai-core/OnnxSession.hpp
    ai-core/MaskPredictor.cpp
    ai-core/MaskPredictor.hpp
    ai-core/InpaintEngine.cpp
    ai-core/InpaintEngine.hpp
    ai-core/UpscaleEngine.cpp
    ai-core/UpscaleEngine.hpp
    export-core/ExportService.cpp
    export-core/ExportService.hpp
    storage/ProjectStore.cpp
    storage/ProjectStore.hpp
)
target_include_directories(lumen_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(lumen_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Sql
        Qt6::Concurrent
)
# ── ONNX Runtime (AI features) ────────────────────────────────────────────
# Disabled by default.
#
# Why: onnxruntime_cxx_api.h instantiates a global template static member:
#
#   template <typename T>
#   const OrtApi* Global<T>::api_{ OrtGetApiBase()->GetApi(ORT_API_VERSION) };
#
# This calls OrtGetApiBase() — a function inside onnxruntime.dll — during the
# C++ static-initialisation phase, BEFORE main() runs.  Even with
# /DELAYLOAD:onnxruntime.dll, Windows must resolve onnxruntime.dll and all its
# transitive dependencies (abseil_dll, libprotobuf, re2 …) at that point.  If
# any one of those DLLs is absent or mismatched the delay-load throws
# 0xC06D007E and the process dies silently before main() is entered — no
# console output, no MessageBox, nothing visible.
#
# To enable AI features once you have confirmed all DLLs are present:
#   cmake … -DLUMEN_ENABLE_ONNX=ON
option(LUMEN_ENABLE_ONNX
    "Enable ONNX Runtime AI inference. Requires onnxruntime vcpkg package, all \
transitive DLLs present in the output directory, and model .onnx files in models/."
    OFF)

if(LUMEN_ENABLE_ONNX)
    find_package(onnxruntime QUIET
        HINTS "${ONNXRUNTIME_ROOT}/lib/cmake/onnxruntime"
    )
    if(onnxruntime_FOUND)
        message(STATUS "ONNX Runtime found  — AI inference ENABLED")
        target_compile_definitions(lumen_core PUBLIC HAVE_ONNXRUNTIME)
        target_link_libraries(lumen_core PUBLIC onnxruntime::onnxruntime)
    else()
        message(WARNING "LUMEN_ENABLE_ONNX=ON but onnxruntime package not found — AI disabled")
    endif()
else()
    message(STATUS "ONNX Runtime DISABLED (LUMEN_ENABLE_ONNX=OFF) — AI tools will report unavailable at runtime")
endif()
find_package(libraw CONFIG QUIET)
if(libraw_FOUND)
    message(STATUS "LibRaw found - RAW import enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_LIBRAW)
    # vcpkg exports libraw::raw (non-thread-safe) and libraw::raw_r (thread-safe)
    # NOT libraw::libraw - verified from librawTargets.cmake
    target_link_libraries(lumen_core PUBLIC libraw::raw)
else()
    message(STATUS "LibRaw NOT found - RAW import disabled")
endif()
find_package(lcms2 QUIET)
if(lcms2_FOUND)
    message(STATUS "lcms2 found - color management enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_LCMS2)
    target_link_libraries(lumen_core PUBLIC lcms2::lcms2)
else()
    message(STATUS "lcms2 NOT found - color profiles disabled")
endif()
find_package(OpenCV QUIET COMPONENTS core imgproc photo)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found - NR and sharpening enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_OPENCV)
    target_include_directories(lumen_core PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(lumen_core PUBLIC ${OpenCV_LIBS})
else()
    message(STATUS "OpenCV NOT found - NR/sharpening disabled")
endif()