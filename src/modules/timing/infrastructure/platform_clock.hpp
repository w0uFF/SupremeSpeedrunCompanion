#pragma once
#include "contracts/observation.hpp"
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <chrono>
#endif
namespace supreme::timing {
// No engine dependencies. QPC failure is explicit; never fall back to wall time.
class PlatformClock final : public IClock {
#ifdef _WIN32
    std::uint64_t frequency_;
public:
    PlatformClock() {
        LARGE_INTEGER f{};
        if(!QueryPerformanceFrequency(&f) || f.QuadPart<=0) throw std::runtime_error("QPC frequency unavailable");
        frequency_=static_cast<std::uint64_t>(f.QuadPart);
    }
    ClockSample sample() const override {
        LARGE_INTEGER ticks{};
        if(!QueryPerformanceCounter(&ticks) || ticks.QuadPart<0) throw std::runtime_error("QPC sample unavailable");
        return {static_cast<std::uint64_t>(ticks.QuadPart),frequency_};
    }
#else
public:
    ClockSample sample() const override {
        using namespace std::chrono;
        static_assert(steady_clock::is_steady,"monotonic clock required");
        const auto ticks=duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
        if(ticks<0) throw std::runtime_error("negative steady clock sample");
        return {static_cast<std::uint64_t>(ticks),1000000000};
    }
#endif
};
}
