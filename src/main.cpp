#include "app/Application.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        app::Application app(app::parseArgs(argc, argv));
        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }
}
