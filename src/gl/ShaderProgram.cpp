#include "gl/ShaderProgram.hpp"
#include "util/Exceptions.hpp"

#include <array>
#include <string>

namespace gl {
namespace {
GLuint compile(GLenum type, std::string_view source) {
    const GLuint shader = glCreateShader(type);
    const char* ptr = source.data();
    const GLint len = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &ptr, &len);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        std::array<char, 4096> log{};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        glDeleteShader(shader);
        throw util::RuntimeError(std::string("Shader compilation failed: ") + log.data());
    }
    return shader;
}
} // namespace

ShaderProgram::ShaderProgram(std::string_view vertex, std::string_view fragment) {
    const GLuint vs = compile(GL_VERTEX_SHADER, vertex);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragment);
    id_ = glCreateProgram();
    glAttachShader(id_, vs);
    glAttachShader(id_, fs);
    glLinkProgram(id_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(id_, GL_LINK_STATUS, &ok);
    if (!ok) {
        std::array<char, 4096> log{};
        glGetProgramInfoLog(id_, static_cast<GLsizei>(log.size()), nullptr, log.data());
        throw util::RuntimeError(std::string("Program link failed: ") + log.data());
    }
}

ShaderProgram::~ShaderProgram() { if (id_) glDeleteProgram(id_); }
void ShaderProgram::use() const { glUseProgram(id_); }

} // namespace gl
