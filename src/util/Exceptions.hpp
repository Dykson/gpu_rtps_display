#pragma once

#include <stdexcept>
#include <string>

namespace util {

class RuntimeError : public std::runtime_error {
public:
    explicit RuntimeError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace util
