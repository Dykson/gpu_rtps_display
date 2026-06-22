#include "gl/TextureInterop.hpp"
#include "cuda/CudaContext.hpp"
#include "util/Exceptions.hpp"

#include <cuda_gl_interop.h>

#include <algorithm>
#include <sstream>

namespace gl {
namespace {
__device__ unsigned char clampByte(float v) {
    return static_cast<unsigned char>(fminf(255.0f, fmaxf(0.0f, v)));
}

__global__ void nv12ToRgbaKernel(cudaSurfaceObject_t dst, const unsigned char* src, int width, int height, int pitch) {
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    const unsigned char yv = src[y * pitch + x];
    const unsigned char* uvPlane = src + pitch * height;
    const int uvIndex = (y / 2) * pitch + (x & ~1);
    const float u = static_cast<float>(uvPlane[uvIndex]) - 128.0f;
    const float v = static_cast<float>(uvPlane[uvIndex + 1]) - 128.0f;
    const float yy = static_cast<float>(yv);

    uchar4 rgba;
    rgba.x = clampByte(yy + 1.402f * v);
    rgba.y = clampByte(yy - 0.344136f * u - 0.714136f * v);
    rgba.z = clampByte(yy + 1.772f * u);
    rgba.w = 255;
    surf2Dwrite(rgba, dst, x * static_cast<int>(sizeof(uchar4)), y);
}
} // namespace

TextureInterop::TextureInterop(int width, int height) : width_(width), height_(height) {
    glCreateTextures(GL_TEXTURE_2D, 1, &texture_);
    glTextureStorage2D(texture_, 1, GL_RGBA8, width_, height_);
    glTextureParameteri(texture_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(texture_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    CUDA_CHECK(cudaGraphicsGLRegisterImage(&cudaResource_, texture_, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard));
}

TextureInterop::~TextureInterop() {
    if (cudaResource_) cudaGraphicsUnregisterResource(cudaResource_);
    if (texture_) glDeleteTextures(1, &texture_);
}

void TextureInterop::copyNv12ToTexture(CUdeviceptr nv12DevicePtr, int width, int height, int pitch) {
    if (width != width_ || height != height_) throw util::RuntimeError("Decoded frame size changed; stream renegotiation is not supported yet");
    CUDA_CHECK(cudaGraphicsMapResources(1, &cudaResource_));
    cudaArray_t array = nullptr;
    CUDA_CHECK(cudaGraphicsSubResourceGetMappedArray(&array, cudaResource_, 0, 0));
    cudaResourceDesc desc{};
    desc.resType = cudaResourceTypeArray;
    desc.res.array.array = array;
    cudaSurfaceObject_t surface = 0;
    CUDA_CHECK(cudaCreateSurfaceObject(&surface, &desc));
    const dim3 block(16, 16);
    const dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    nv12ToRgbaKernel<<<grid, block>>>(surface, reinterpret_cast<const unsigned char*>(nv12DevicePtr), width, height, pitch);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDestroySurfaceObject(surface));
    CUDA_CHECK(cudaGraphicsUnmapResources(1, &cudaResource_));
}

} // namespace gl
