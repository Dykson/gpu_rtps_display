#include "app/Application.hpp"
#include "cuda/CudaContext.hpp"
#include "ffmpeg/FFmpegInitializer.hpp"
#include "ffmpeg/RtspNvdecDecoder.hpp"
#include "gl/GlContext.hpp"
#include "gl/TextureInterop.hpp"
#include "gl/VideoRenderer.hpp"
#include "util/Exceptions.hpp"

#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace app {

Application::Application(Config config) : config_(std::move(config)) {}

int Application::run() {
    ffmpeg::FFmpegInitializer ffmpeg;
    gl::GlContext window(config_.windowWidth, config_.windowHeight, "GPU RTSP Display");
    cuda::CudaContext cuda(config_.cudaDevice);
    ffmpeg::RtspNvdecDecoder decoder(config_.url, config_.rtspTransport, config_.watermarkPath);
    gl::TextureInterop interop(decoder.width(), decoder.height());
    gl::VideoRenderer renderer;

    auto statsStart = std::chrono::steady_clock::now();
    auto lastTitle = statsStart;
    int frames = 0;
    double fps = 0.0;
    double lastLatencyMs = 0.0;
    double lastFilterMs = 0.0;

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

        ++frames;
        lastLatencyMs = frame.streamLatencyMs;
        lastFilterMs = frame.filterLatencyMs;
        const auto now = std::chrono::steady_clock::now();
        const auto titleElapsed = std::chrono::duration<double>(now - lastTitle).count();
        if (titleElapsed >= 1.0) {
            fps = static_cast<double>(frames) / std::chrono::duration<double>(now - statsStart).count();
            std::ostringstream title;
            title << "GPU RTSP Display | stream " << frame.width << "x" << frame.height
                  << " | screen " << fbw << "x" << fbh
                  << " | fps " << std::fixed << std::setprecision(1) << fps
                  << " | latency " << std::setprecision(1) << lastLatencyMs << " ms"
                  << " | overlay_cuda " << std::setprecision(2) << lastFilterMs << " ms"
                  << " | zero-copy CUDA";
            glfwSetWindowTitle(window.window(), title.str().c_str());
            std::cout << title.str() << std::endl;
            lastTitle = now;
        }
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
        else if (arg == "--watermark") config.watermarkPath = requireValue("--watermark");
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: gpu_rtsp_display [--width N] [--height N] [--cuda-device N] [--rtsp-transport tcp|udp] [--watermark assets/carrot_broadcast_logo.png] rtsp://...\n";
            std::exit(EXIT_SUCCESS);
        } else if (config.url.empty()) config.url = arg;
        else throw util::RuntimeError("Unexpected argument: " + arg);
    }
    if (config.url.empty()) throw util::RuntimeError("RTSP URL is required. Use --help for usage.");
    return config;
}

} // namespace app
