add_library(ai_core STATIC
    AiRuntime.cpp
    AiRuntime.hpp
)

target_include_directories(ai_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find ONNX Runtime for model inference
find_package(ONNXRuntime REQUIRED)

target_link_libraries(ai_core
    PRIVATE
        shared_types::shared_types
        editor_core::editor_core
        onnxruntime::onnxruntime
        Qt6::Core
        Qt6::Concurrent
)

# Create additional AI engines for specific tasks (optional, can be disabled with -DAI_CORE_FEATURES=OFF)
option(ENABLE_MASK_PREDICTOR "Enable MobileSAM mask prediction" ON)
if(ENABLE_MASK_PREDICTOR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/models/mobile_sam.onnx")
    add_library(mask_predictor STATIC
        MaskPredictor.cpp
        MaskPredictor.hpp
    )

    target_include_directories(mask_predictor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    
    # Link ONNX Runtime for mask prediction
    find_package(ONNXRuntime REQUIRED)
    target_link_libraries(mask_predictor PRIVATE onnxruntime::onnxruntime Qt6::Core ai_core::ai_core)
endif()

option(ENABLE_INPAINT_ENGINE "Enable LaMa inpainting" ON)
if(ENABLE_INPAINT_ENGINE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/models/big-lama.onnx")
    add_library(inpaint_engine STATIC
        InpaintEngine.cpp
        InpaintEngine.hpp
    )

    target_include_directories(inpaint_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    
    find_package(ONNXRuntime REQUIRED)
    target_link_libraries(inpaint_engine PRIVATE onnxruntime::onnxruntime Qt6::Core ai_core::ai_core OpenCV::imgcodecs)
endif()

option(ENABLE_UPSCALE_ENGINE "Enable Real-ESRGAN upscaling" ON)
if(ENABLE_UPSCALE_ENGINE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/models/realesrgan-x4plus.onnx")
    add_library(upscale_engine STATIC
        UpscaleEngine.cpp
        UpscaleEngine.hpp
    )

    target_include_directories(upscale_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    
    find_package(ONNXRuntime REQUIRED)
    target_link_libraries(upscale_engine PRIVATE onnxruntime::onnxruntime Qt6::Core ai_core::ai_core OpenCV::imgcodecs)
endif()

# Create OnnxSession helper for lazy model loading (optional utility library)
option(BUILD_ONNX_SESSION "Build ONNX session wrapper" OFF)
if(BUILD_ONNX_SESSION)
    add_library(onnx_session STATIC
        OnnxSession.cpp
        OnnxSession.hpp
    )

    target_include_directories(onnx_session PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    
    find_package(ONNXRuntime REQUIRED)
    target_link_libraries(onnx_session PRIVATE onnxruntime::onnxruntime Qt6::Core ai_core::ai_core)
endif()
