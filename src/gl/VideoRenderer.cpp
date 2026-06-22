#include "gl/VideoRenderer.hpp"

namespace gl {
namespace {
constexpr const char* kVertex = R"glsl(
#version 460 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = texCoord;
    gl_Position = vec4(position, 0.0, 1.0);
}
)glsl";

constexpr const char* kFragment = R"glsl(
#version 460 core
layout(binding = 0) uniform sampler2D videoTexture;
in vec2 vTexCoord;
out vec4 fragColor;
void main() {
    fragColor = texture(videoTexture, vTexCoord);
}
)glsl";
} // namespace

VideoRenderer::VideoRenderer() : shader_(kVertex, kFragment) {
    constexpr float vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
    };
    glCreateVertexArrays(1, &vao_);
    glCreateBuffers(1, &vbo_);
    glNamedBufferStorage(vbo_, sizeof(vertices), vertices, 0);
    glVertexArrayVertexBuffer(vao_, 0, vbo_, 0, 4 * sizeof(float));
    glEnableVertexArrayAttrib(vao_, 0);
    glEnableVertexArrayAttrib(vao_, 1);
    glVertexArrayAttribFormat(vao_, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribFormat(vao_, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glVertexArrayAttribBinding(vao_, 0, 0);
    glVertexArrayAttribBinding(vao_, 1, 0);
}

VideoRenderer::~VideoRenderer() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void VideoRenderer::draw(GLuint texture, int framebufferWidth, int framebufferHeight) {
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    shader_.use();
    glBindTextureUnit(0, texture);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

} // namespace gl
