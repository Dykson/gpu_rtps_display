#pragma once

#include <glad/gl.h>
#include <string_view>

namespace gl {

class ShaderProgram {
public:
    ShaderProgram(std::string_view vertex, std::string_view fragment);
    ~ShaderProgram();
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    void use() const;
    GLuint id() const noexcept { return id_; }
private:
    GLuint id_ = 0;
};

} // namespace gl
