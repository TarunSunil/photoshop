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

find_package(onnxruntime QUIET
    HINTS "${ONNXRUNTIME_ROOT}/lib/cmake/onnxruntime"
)

if(onnxruntime_FOUND)
    message(STATUS "ONNX Runtime found — AI inference enabled")
    target_compile_definitions(lumen_core PUBLIC HAVE_ONNXRUNTIME)
    target_link_libraries(lumen_core PUBLIC onnxruntime::onnxruntime)
else()
    message(STATUS "ONNX Runtime NOT found — AI features disabled")
endif()

find_package(LibRaw QUIET)

if(LibRaw_FOUND)
    message(STATUS "LibRaw found — RAW import enabled")
    # Previously: found but never defined HAVE_LIBRAW and never linked.
    # RawImporter.cpp is entirely wrapped in #ifdef HAVE_LIBRAW, so without
    # this define RAW files always fell through to image.load(), which
    # fails for camera RAW formats — RAW import was permanently a no-op
    # even on machines with LibRaw installed.
    target_compile_definitions(lumen_core PUBLIC HAVE_LIBRAW)
    # vcpkg's libraw port exports a lowercase namespace (libraw::raw),
    # not LibRaw::raw -- verified against vcpkg's own usage docs.
    target_link_libraries(lumen_core PUBLIC libraw::raw)
else()
    message(STATUS "LibRaw NOT found — RAW import disabled")
endif()

find_package(lcms2 QUIET)

if(lcms2_FOUND)
    message(STATUS "lcms2 found — color management enabled")
    # Same gap as LibRaw above: found but neither defined nor linked.
    target_compile_definitions(lumen_core PUBLIC HAVE_LCMS2)
    target_link_libraries(lumen_core PUBLIC lcms2::lcms2)
else()
    message(STATUS "lcms2 NOT found — color profiles disabled")
endif()

find_package(OpenCV QUIET COMPONENTS core imgproc photo)

if(OpenCV_FOUND)
    message(STATUS "OpenCV found — NR and sharpening enabled")
    # This is the actual cause of "Detail sliders don't do anything":
    # RenderPipeline.cpp's noise-reduction/sharpening block is wrapped in
    # #ifdef HAVE_OPENCV, but that macro was never defined here — only
    # the libraries were linked. The #ifdef was always false, so that
    # code never compiled in, regardless of OpenCV being installed.
    # Also adding the include dirs explicitly: ${OpenCV_LIBS} alone
    # (the legacy-style variable) does not reliably propagate
    # OpenCV's headers the way an imported target would.
    target_compile_definitions(lumen_core PUBLIC HAVE_OPENCV)
    target_include_directories(lumen_core PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(lumen_core PUBLIC ${OpenCV_LIBS})
else()
    message(STATUS "OpenCV NOT found — NR/sharpening disabled")
endif()