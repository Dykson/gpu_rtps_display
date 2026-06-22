#pragma once

#include <utility>

namespace util {

template <typename F>
class ScopeExit {
public:
    explicit ScopeExit(F&& fn) noexcept : fn_(std::forward<F>(fn)), active_(true) {}
    ScopeExit(ScopeExit&& other) noexcept : fn_(std::move(other.fn_)), active_(other.active_) { other.active_ = false; }
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ScopeExit& operator=(ScopeExit&&) = delete;
    ~ScopeExit() noexcept { if (active_) fn_(); }
private:
    F fn_;
    bool active_;
};

template <typename F>
ScopeExit<F> makeScopeExit(F&& fn) { return ScopeExit<F>(std::forward<F>(fn)); }

} // namespace util
