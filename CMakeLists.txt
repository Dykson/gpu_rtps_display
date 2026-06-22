cmake_minimum_required(VERSION 3.24)

project(gpu_rtsp_display VERSION 1.0.0 LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GPU_RTSP_FETCH_GLAD "Download GLAD at configure time" ON)

find_package(PkgConfig REQUIRED)
find_package(CUDAToolkit REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavfilter
    libavutil
    libswscale
)

include(FetchContent)
if(GPU_RTSP_FETCH_GLAD)
    FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG v2.0.6
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glad)
    glad_add_library(glad_gl_core_46 STATIC API gl:core=4.6)
else()
    find_package(glad CONFIG REQUIRED)
endif()

add_executable(gpu_rtsp_display
    src/main.cpp
    src/app/Application.cpp
    src/cuda/CudaContext.cpp
    src/ffmpeg/FFmpegInitializer.cpp
    src/ffmpeg/RtspNvdecDecoder.cpp
    src/gl/GlContext.cpp
    src/gl/ShaderProgram.cpp
    src/gl/TextureInterop.cu
    src/gl/VideoRenderer.cpp
)

target_include_directories(gpu_rtsp_display PRIVATE src)

target_link_libraries(gpu_rtsp_display PRIVATE
    PkgConfig::FFMPEG
    CUDA::cudart
    CUDA::cuda_driver
    CUDA::nvcuvid
    OpenGL::GL
    glfw
    glad_gl_core_46
)

target_compile_definitions(gpu_rtsp_display PRIVATE GLFW_INCLUDE_NONE)

if(MSVC)
    target_compile_options(gpu_rtsp_display PRIVATE /W4 /permissive-)
else()
    target_compile_options(gpu_rtsp_display PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS gpu_rtsp_display RUNTIME DESTINATION bin)
