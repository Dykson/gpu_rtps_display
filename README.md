# GPU RTSP Display

Industrial C++17 RTSP H.264 player that decodes with FFmpeg NVDEC and renders with OpenGL 4.6 through CUDA/OpenGL interop. The decoded `AV_PIX_FMT_CUDA` frame stays in GPU memory; CUDA kernels convert NV12 to RGBA directly into a registered OpenGL texture, then OpenGL presents the texture.

## Dependencies

- CMake 3.24+
- C++17 compiler
- NVIDIA driver and CUDA Toolkit with `nvcuvid`
- FFmpeg development packages built with CUDA/NVDEC support (`libavcodec`, `libavformat`, `libavutil`, `libswscale`)
- GLFW 3.3+
- OpenGL 4.6 capable NVIDIA GPU/driver
- Network access during CMake configure to fetch GLAD, or configure with `-DGPU_RTSP_FETCH_GLAD=OFF` and provide a GLAD CMake package

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

```bash
./build/gpu_rtsp_display rtsp://user:password@camera/stream
```

Optional flags:

```bash
./build/gpu_rtsp_display --width 1920 --height 1080 --rtsp-transport tcp rtsp://camera/stream
```

## Zero-copy pipeline

1. FFmpeg opens RTSP and selects the H.264 stream.
2. The decoder is configured with `AV_HWDEVICE_TYPE_CUDA`, so decoded frames are `AV_PIX_FMT_CUDA` surfaces.
3. The OpenGL render texture is registered with CUDA via `cudaGraphicsGLRegisterImage`.
4. Each GPU frame is mapped as a CUDA resource, converted from NV12 to RGBA into the OpenGL texture, unmapped, and drawn.
5. No CPU pixel staging or `av_hwframe_transfer_data` is used in the display path.
