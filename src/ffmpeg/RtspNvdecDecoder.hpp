#pragma once

#include <cuda.h>
#include <cstdint>
#include <memory>
#include <string>

struct AVBufferRef;
struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
struct AVFilterContext;
struct AVFilterGraph;

namespace ffmpeg {

struct GpuFrame {
    CUdeviceptr data = 0;
    int width = 0;
    int height = 0;
    int pitch = 0;
    int64_t ptsUs = 0;
    double streamLatencyMs = 0.0;
    double filterLatencyMs = 0.0;
};

class RtspNvdecDecoder {
public:
    RtspNvdecDecoder(std::string url, std::string transport, std::string watermarkPath);
    ~RtspNvdecDecoder();
    RtspNvdecDecoder(const RtspNvdecDecoder&) = delete;
    RtspNvdecDecoder& operator=(const RtspNvdecDecoder&) = delete;

    bool readFrame(GpuFrame& frame);
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }

private:
    void open();
    void initWatermarkFilter(const AVFrame* firstFrame);
    bool filterFrame(AVFrame* decodedFrame, GpuFrame& out);
    void fillOutput(const AVFrame* frame, double filterLatencyMs, GpuFrame& out) const;
    std::string url_;
    std::string transport_;
    std::string watermarkPath_;
    AVFormatContext* format_ = nullptr;
    AVCodecContext* codec_ = nullptr;
    AVBufferRef* hwDevice_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVFrame* filteredFrame_ = nullptr;
    AVFilterGraph* filterGraph_ = nullptr;
    AVFilterContext* bufferSrc_ = nullptr;
    AVFilterContext* bufferSink_ = nullptr;
    int videoStream_ = -1;
    int width_ = 0;
    int height_ = 0;
    bool frameHeld_ = false;
};

} // namespace ffmpeg
