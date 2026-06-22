#include "cuda/CudaContext.hpp"
#include "util/Exceptions.hpp"

#include <cuda.h>
#include <cuda_runtime_api.h>

#include <sstream>

namespace cuda {

void checkCuda(int status, const char* expression, const char* file, int line) {
    if (status == cudaSuccess) {
        return;
    }
    std::ostringstream oss;
    oss << "CUDA call failed: " << expression << " at " << file << ':' << line
        << " (" << cudaGetErrorString(static_cast<cudaError_t>(status)) << ')';
    throw util::RuntimeError(oss.str());
}

CudaContext::CudaContext(int device) {
    CUDA_CHECK(cudaSetDevice(device));
    cudaFree(nullptr);
}

} // namespace cuda
