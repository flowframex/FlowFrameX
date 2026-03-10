cmake_minimum_required(VERSION 3.20)
project(FlowFrameX VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows only
if(NOT WIN32)
    message(FATAL_ERROR "FlowFrameX only supports Windows (DirectX 11)")
endif()

add_executable(FlowFrameX WIN32
    src/main.cpp
    src/Engine.cpp
    src/Capture.cpp
    src/MotionEstimator.cpp
    src/Extrapolator.cpp
    src/SceneCut.cpp
    src/FramePacer.cpp
    src/ShaderCompiler.cpp
)

target_include_directories(FlowFrameX PRIVATE src)

target_link_libraries(FlowFrameX PRIVATE
    d3d11
    dxgi
    d3dcompiler
    dwmapi
    dxguid
    winmm
)

# Copy shaders next to the exe
add_custom_command(TARGET FlowFrameX POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:FlowFrameX>/shaders
    COMMENT "Copying shaders..."
)

# Optimization flags for release
target_compile_options(FlowFrameX PRIVATE
    $<$<CONFIG:Release>:/O2 /GL /DNDEBUG>
    $<$<CONFIG:Debug>:/Od /Zi>
)
