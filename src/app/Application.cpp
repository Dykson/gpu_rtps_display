#include "app/Application.hpp"
#include "cuda/CudaContext.hpp"
#include "ffmpeg/FFmpegInitializer.hpp"
#include "ffmpeg/RtspNvdecDecoder.hpp"
#include "gl/GlContext.hpp"
#include "gl/TextureInterop.hpp"
#include "gl/VideoRenderer.hpp"
#include "util/Exceptions.hpp"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <utility>

namespace app {

Application::Application(Config config) : config_(std::move(config)) {}

int Application::run() {
    ffmpeg::FFmpegInitializer ffmpeg;
    gl::GlContext window(config_.windowWidth, config_.windowHeight, "GPU RTSP Display");
    cuda::CudaContext cuda(config_.cudaDevice);
    ffmpeg::RtspNvdecDecoder decoder(config_.url, config_.rtspTransport);
    gl::TextureInterop interop(decoder.width(), decoder.height());
    gl::VideoRenderer renderer;

    while (!window.shouldClose()) {
        window.pollEvents();
        ffmpeg::GpuFrame frame;
        if (!decoder.readFrame(frame)) break;
        interop.copyNv12ToTexture(frame.data, frame.width, frame.height, frame.pitch);
        int fbw = 0;
        int fbh = 0;
        glfwGetFramebufferSize(window.window(), &fbw, &fbh);
        renderer.draw(interop.texture(), fbw, fbh);
        window.swapBuffers();
    }
    return EXIT_SUCCESS;
}

Config parseArgs(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto requireValue = [&](const char* name) -> std::string {
            if (++i >= argc) throw util::RuntimeError(std::string("Missing value for ") + name);
            return argv[i];
        };
        if (arg == "--width") config.windowWidth = std::stoi(requireValue("--width"));
        else if (arg == "--height") config.windowHeight = std::stoi(requireValue("--height"));
        else if (arg == "--cuda-device") config.cudaDevice = std::stoi(requireValue("--cuda-device"));
        else if (arg == "--rtsp-transport") config.rtspTransport = requireValue("--rtsp-transport");
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: gpu_rtsp_display [--width N] [--height N] [--cuda-device N] [--rtsp-transport tcp|udp] rtsp://...\n";
            std::exit(EXIT_SUCCESS);
        } else if (config.url.empty()) config.url = arg;
        else throw util::RuntimeError("Unexpected argument: " + arg);
    }
    if (config.url.empty()) throw util::RuntimeError("RTSP URL is required. Use --help for usage.");
    return config;
}

} // namespace app
