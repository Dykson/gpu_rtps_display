#pragma once

#include <cuda.h>
#include <memory>
#include <string>

struct AVBufferRef;
struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;

namespace ffmpeg {

struct GpuFrame {
    CUdeviceptr data = 0;
    int width = 0;
    int height = 0;
    int pitch = 0;
};

class RtspNvdecDecoder {
public:
    RtspNvdecDecoder(std::string url, std::string transport);
    ~RtspNvdecDecoder();
    RtspNvdecDecoder(const RtspNvdecDecoder&) = delete;
    RtspNvdecDecoder& operator=(const RtspNvdecDecoder&) = delete;

    bool readFrame(GpuFrame& frame);
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }

private:
    void open();
    std::string url_;
    std::string transport_;
    AVFormatContext* format_ = nullptr;
    AVCodecContext* codec_ = nullptr;
    AVBufferRef* hwDevice_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* frame_ = nullptr;
    int videoStream_ = -1;
    int width_ = 0;
    int height_ = 0;
    bool frameHeld_ = false;
};

} // namespace ffmpeg
