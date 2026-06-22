#pragma once

namespace cuda {

class CudaContext {
public:
    explicit CudaContext(int device = 0);
    ~CudaContext() = default;
    CudaContext(const CudaContext&) = delete;
    CudaContext& operator=(const CudaContext&) = delete;
};

void checkCuda(int status, const char* expression, const char* file, int line);

} // namespace cuda

#define CUDA_CHECK(expr) ::cuda::checkCuda((expr), #expr, __FILE__, __LINE__)
