#include "ffmpeg/RtspNvdecDecoder.hpp"
#include "util/Exceptions.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/mem.h>
#include <libavutil/pixfmt.h>
#include <libavutil/time.h>
}

#include <array>
#include <chrono>
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

std::string escapeFilterPath(const std::string& path) {
    std::string out;
    out.reserve(path.size() * 2);
    for (char c : path) {
        if (c == '\\' || c == '\'' || c == ':' || c == '[' || c == ']') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}
} // namespace

RtspNvdecDecoder::RtspNvdecDecoder(std::string url, std::string transport, std::string watermarkPath)
    : url_(std::move(url)), transport_(std::move(transport)), watermarkPath_(std::move(watermarkPath)) {
    open();
}

RtspNvdecDecoder::~RtspNvdecDecoder() {
    avfilter_graph_free(&filterGraph_);
    av_frame_free(&filteredFrame_);
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
    av_dict_set(&options, "fflags", "nobuffer", 0);
    av_dict_set(&options, "flags", "low_delay", 0);
    av_dict_set(&options, "flush_packets", "1", 0);
    av_dict_set(&options, "max_delay", "0", 0);
    check(avformat_open_input(&format_, url_.c_str(), nullptr, &options), "open RTSP input");
    av_dict_free(&options);
    format_->flags |= AVFMT_FLAG_NOBUFFER;
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
    codec_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    check(avcodec_open2(codec_, decoder, nullptr), "open NVDEC decoder");

    width_ = codec_->width;
    height_ = codec_->height;
    packet_ = av_packet_alloc();
    frame_ = av_frame_alloc();
    filteredFrame_ = av_frame_alloc();
    if (!packet_ || !frame_ || !filteredFrame_) throw util::RuntimeError("Failed to allocate FFmpeg packet/frame");
}

void RtspNvdecDecoder::initWatermarkFilter(const AVFrame* firstFrame) {
    filterGraph_ = avfilter_graph_alloc();
    if (!filterGraph_) throw util::RuntimeError("Failed to allocate FFmpeg filter graph");
    filterGraph_->hw_device_ctx = av_buffer_ref(hwDevice_);

    const AVFilter* buffer = avfilter_get_by_name("buffer");
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");
    std::ostringstream args;
    AVRational tb = format_->streams[videoStream_]->time_base;
    args << "video_size=" << firstFrame->width << "x" << firstFrame->height
         << ":pix_fmt=" << AV_PIX_FMT_CUDA
         << ":time_base=" << tb.num << "/" << tb.den
         << ":pixel_aspect=1/1";
    check(avfilter_graph_create_filter(&bufferSrc_, buffer, "in", args.str().c_str(), nullptr, filterGraph_), "create filter input");
    AVBufferSrcParameters* params = av_buffersrc_parameters_alloc();
    if (!params) throw util::RuntimeError("Failed to allocate buffer source parameters");
    params->hw_frames_ctx = av_buffer_ref(firstFrame->hw_frames_ctx);
    check(av_buffersrc_parameters_set(bufferSrc_, params), "set CUDA frames context on filter input");
    av_free(params);
    check(avfilter_graph_create_filter(&bufferSink_, buffersink, "out", nullptr, nullptr, filterGraph_), "create filter output");

    std::string desc = "movie='" + escapeFilterPath(watermarkPath_) + "':loop=0,format=rgba,hwupload_cuda[wm];"
                       "[in][wm]overlay_cuda=x=W-w-32:y=H-h-32[out]";
    AVFilterInOut* inputs = avfilter_inout_alloc();
    AVFilterInOut* outputs = avfilter_inout_alloc();
    outputs->name = av_strdup("in"); outputs->filter_ctx = bufferSrc_; outputs->pad_idx = 0; outputs->next = nullptr;
    inputs->name = av_strdup("out"); inputs->filter_ctx = bufferSink_; inputs->pad_idx = 0; inputs->next = nullptr;
    int ret = avfilter_graph_parse_ptr(filterGraph_, desc.c_str(), &inputs, &outputs, nullptr);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    check(ret, "parse watermark overlay_cuda filter graph");
    check(avfilter_graph_config(filterGraph_, nullptr), "configure watermark overlay_cuda filter graph");
}

void RtspNvdecDecoder::fillOutput(const AVFrame* frame, double filterLatencyMs, GpuFrame& out) const {
    out.data = reinterpret_cast<CUdeviceptr>(frame->data[0]);
    out.pitch = static_cast<int>(frame->linesize[0]);
    out.width = frame->width;
    out.height = frame->height;
    out.ptsUs = (frame->best_effort_timestamp == AV_NOPTS_VALUE) ? 0 : av_rescale_q(frame->best_effort_timestamp, format_->streams[videoStream_]->time_base, AVRational{1, 1000000});
    out.streamLatencyMs = out.ptsUs > 0 ? static_cast<double>(av_gettime_relative() - out.ptsUs) / 1000.0 : 0.0;
    out.filterLatencyMs = filterLatencyMs;
}

bool RtspNvdecDecoder::filterFrame(AVFrame* decodedFrame, GpuFrame& out) {
    if (!filterGraph_) initWatermarkFilter(decodedFrame);
    const auto start = std::chrono::steady_clock::now();
    check(av_buffersrc_add_frame_flags(bufferSrc_, decodedFrame, AV_BUFFERSRC_FLAG_KEEP_REF), "send CUDA frame to overlay_cuda");

    AVFrame* candidate = av_frame_alloc();
    if (!candidate) throw util::RuntimeError("Failed to allocate temporary filtered frame");
    bool gotFrame = false;
    while (true) {
        av_frame_unref(candidate);
        int ret = av_buffersink_get_frame_flags(bufferSink_, candidate, AV_BUFFERSINK_FLAG_NO_REQUEST);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        check(ret, "receive overlay_cuda frame");
        av_frame_unref(filteredFrame_);
        av_frame_move_ref(filteredFrame_, candidate);
        gotFrame = true;
    }
    av_frame_free(&candidate);
    if (!gotFrame) return false;
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    fillOutput(filteredFrame_, elapsed, out);
    return true;
}

bool RtspNvdecDecoder::readFrame(GpuFrame& out) {
    while (true) {
        if (frameHeld_) { av_frame_unref(frame_); frameHeld_ = false; }
        int ret = avcodec_receive_frame(codec_, frame_);
        if (ret == 0) {
            if (frame_->format != AV_PIX_FMT_CUDA) throw util::RuntimeError("Decoder returned a non-CUDA frame; zero-copy path was broken");
            if (filterFrame(frame_, out)) { frameHeld_ = true; return true; }
            av_frame_unref(frame_);
            continue;
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
