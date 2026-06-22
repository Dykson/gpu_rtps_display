#pragma once

#include <string>

namespace app {

struct Config {
    std::string url;
    std::string rtspTransport = "tcp";
    int windowWidth = 1280;
    int windowHeight = 720;
    int cudaDevice = 0;
    std::string watermarkPath = "assets/carrot_broadcast_logo.png";
};

class Application {
public:
    explicit Application(Config config);
    int run();
private:
    Config config_;
};

Config parseArgs(int argc, char** argv);

} // namespace app
