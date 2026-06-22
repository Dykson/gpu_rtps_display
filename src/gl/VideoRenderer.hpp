#pragma once

#include "gl/ShaderProgram.hpp"
#include <glad/gl.h>

namespace gl {

class VideoRenderer {
public:
    VideoRenderer();
    ~VideoRenderer();
    VideoRenderer(const VideoRenderer&) = delete;
    VideoRenderer& operator=(const VideoRenderer&) = delete;
    void draw(GLuint texture, int framebufferWidth, int framebufferHeight);
private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    ShaderProgram shader_;
};

} // namespace gl
