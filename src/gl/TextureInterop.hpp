#pragma once

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <glad/gl.h>

namespace gl {

class TextureInterop {
public:
    TextureInterop(int width, int height);
    ~TextureInterop();
    TextureInterop(const TextureInterop&) = delete;
    TextureInterop& operator=(const TextureInterop&) = delete;

    GLuint texture() const noexcept { return texture_; }
    void copyNv12ToTexture(CUdeviceptr nv12DevicePtr, int width, int height, int pitch);

private:
    int width_ = 0;
    int height_ = 0;
    GLuint texture_ = 0;
    cudaGraphicsResource_t cudaResource_ = nullptr;
};

} // namespace gl
