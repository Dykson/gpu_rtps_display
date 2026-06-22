#pragma once

#include <string>

struct GLFWwindow;

namespace gl {

class GlContext {
public:
    GlContext(int width, int height, const std::string& title);
    ~GlContext();
    GlContext(const GlContext&) = delete;
    GlContext& operator=(const GlContext&) = delete;

    GLFWwindow* window() const noexcept { return window_; }
    bool shouldClose() const;
    void pollEvents() const;
    void swapBuffers() const;

private:
    GLFWwindow* window_ = nullptr;
};

} // namespace gl
