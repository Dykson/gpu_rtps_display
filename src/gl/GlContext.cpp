#include "gl/GlContext.hpp"
#include "util/Exceptions.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

namespace gl {

GlContext::GlContext(int width, int height, const std::string& title) {
    if (!glfwInit()) {
        throw util::RuntimeError("Failed to initialize GLFW");
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw util::RuntimeError("Failed to create OpenGL window");
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    if (!gladLoadGL(glfwGetProcAddress)) {
        throw util::RuntimeError("Failed to load OpenGL 4.6 symbols with GLAD");
    }
}

GlContext::~GlContext() {
    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

bool GlContext::shouldClose() const { return glfwWindowShouldClose(window_); }
void GlContext::pollEvents() const { glfwPollEvents(); }
void GlContext::swapBuffers() const { glfwSwapBuffers(window_); }

} // namespace gl
