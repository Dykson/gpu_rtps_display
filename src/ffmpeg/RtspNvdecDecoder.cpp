#include "ffmpeg/RtspNvdecDecoder.hpp"
#include "util/Exceptions.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixfmt.h>
}

#include <array>
#include <sstream>
#include <utility>

namespace ffmpeg {
namespace {
std::string fferr(int err) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buf{};
    av_strerror(err, buf.data(), buf.size());
    return buf.data();
}

void check(int code, const char* action) {
    if (code < 0) throw util::RuntimeError(std::string(action) + ": " + fferr(code));
}

AVPixelFormat getCudaFormat(AVCodecContext*, const AVPixelFormat* formats) {
    for (const AVPixelFormat* p = formats; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == AV_PIX_FMT_CUDA) return *p;
    }
    return AV_PIX_FMT_NONE;
}
} // namespace

RtspNvdecDecoder::RtspNvdecDecoder(std::string url, std::string transport)
    : url_(std::move(url)), transport_(std::move(transport)) {
    open();
}

RtspNvdecDecoder::~RtspNvdecDecoder() {
    av_frame_free(&frame_);
    av_packet_free(&packet_);
    avcodec_free_context(&codec_);
    avformat_close_input(&format_);
    av_buffer_unref(&hwDevice_);
}

void RtspNvdecDecoder::open() {
    AVDictionary* options = nullptr;
    av_dict_set(&options, "rtsp_transport", transport_.c_str(), 0);
    av_dict_set(&options, "stimeout", "5000000", 0);
    check(avformat_open_input(&format_, url_.c_str(), nullptr, &options), "open RTSP input");
    av_dict_free(&options);
    check(avformat_find_stream_info(format_, nullptr), "read stream info");

    const AVCodec* decoder = nullptr;
    videoStream_ = av_find_best_stream(format_, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (videoStream_ < 0) throw util::RuntimeError("No video stream found");
    if (format_->streams[videoStream_]->codecpar->codec_id != AV_CODEC_ID_H264) {
        throw util::RuntimeError("Only H.264 RTSP streams are supported by this sample");
    }

    decoder = avcodec_find_decoder_by_name("h264_cuvid");
    if (!decoder) decoder = avcodec_find_decoder(format_->streams[videoStream_]->codecpar->codec_id);
    if (!decoder) throw util::RuntimeError("No H.264 decoder available");

    codec_ = avcodec_alloc_context3(decoder);
    if (!codec_) throw util::RuntimeError("Failed to allocate codec context");
    check(avcodec_parameters_to_context(codec_, format_->streams[videoStream_]->codecpar), "copy codec parameters");
    check(av_hwdevice_ctx_create(&hwDevice_, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0), "create CUDA FFmpeg device");
    codec_->hw_device_ctx = av_buffer_ref(hwDevice_);
    codec_->get_format = getCudaFormat;
    codec_->thread_count = 1;
    check(avcodec_open2(codec_, decoder, nullptr), "open NVDEC decoder");

    width_ = codec_->width;
    height_ = codec_->height;
    packet_ = av_packet_alloc();
    frame_ = av_frame_alloc();
    if (!packet_ || !frame_) throw util::RuntimeError("Failed to allocate FFmpeg packet/frame");
}

bool RtspNvdecDecoder::readFrame(GpuFrame& out) {
    while (true) {
        if (frameHeld_) {
            av_frame_unref(frame_);
            frameHeld_ = false;
        }
        int ret = avcodec_receive_frame(codec_, frame_);
        if (ret == 0) {
            if (frame_->format != AV_PIX_FMT_CUDA) throw util::RuntimeError("Decoder returned a non-CUDA frame; zero-copy path was broken");
            out.data = reinterpret_cast<CUdeviceptr>(frame_->data[0]);
            out.pitch = static_cast<int>(frame_->linesize[0]);
            out.width = frame_->width;
            out.height = frame_->height;
            frameHeld_ = true;
            return true;
        }
        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) check(ret, "receive decoded frame");
        ret = av_read_frame(format_, packet_);
        if (ret == AVERROR_EOF) return false;
        check(ret, "read packet");
        if (packet_->stream_index == videoStream_) check(avcodec_send_packet(codec_, packet_), "send packet");
        av_packet_unref(packet_);
    }
}

} // namespace ffmpeg
