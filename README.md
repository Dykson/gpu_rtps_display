# GPU RTSP Display

Industrial C++17 RTSP H.264 player that decodes with FFmpeg NVDEC and renders with OpenGL 4.6 through CUDA/OpenGL interop. The decoded `AV_PIX_FMT_CUDA` frame stays in GPU memory; CUDA kernels convert NV12 to RGBA directly into a registered OpenGL texture, then OpenGL presents the texture.

## Dependencies

- CMake 3.24+
- C++17 compiler
- NVIDIA driver and CUDA Toolkit with `nvcuvid`
- FFmpeg development packages built with CUDA/NVDEC/filter support (`libavcodec`, `libavformat`, `libavfilter`, `libavutil`, `libswscale`)
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
./build/gpu_rtsp_display --width 1920 --height 1080 --rtsp-transport tcp --watermark assets/carrot_broadcast_logo.png rtsp://camera/stream
```

## Zero-copy pipeline

1. FFmpeg opens RTSP and selects the H.264 stream.
2. The decoder is configured with `AV_HWDEVICE_TYPE_CUDA`, so decoded frames are `AV_PIX_FMT_CUDA` surfaces.
3. The OpenGL render texture is registered with CUDA via `cudaGraphicsGLRegisterImage`.
4. FFmpeg applies the PNG watermark with `overlay_cuda` immediately after decode; the video frame remains an `AV_PIX_FMT_CUDA` hardware frame.
5. Each GPU frame is mapped as a CUDA resource, converted from NV12 to RGBA into the OpenGL texture, unmapped, and drawn.
6. No CPU pixel staging or `av_hwframe_transfer_data` is used in the display path.

## Live statistics and latency policy

The player configures RTSP/demux/decode for low delay (`nobuffer`, `low_delay`, `max_delay=0`) and the filter output is drained without requesting extra frames so stale overlay results can be skipped rather than queued. The window title and stdout are updated once per second with stream resolution, framebuffer resolution, measured display FPS, estimated stream latency (when timestamps are available), and `overlay_cuda` processing time.
