#include "ffmpeg/FFmpegInitializer.hpp"

extern "C" {
#include <libavformat/avformat.h>
}

namespace ffmpeg {

FFmpegInitializer::FFmpegInitializer() {
    avformat_network_init();
}

FFmpegInitializer::~FFmpegInitializer() {
    avformat_network_deinit();
}

} // namespace ffmpeg
