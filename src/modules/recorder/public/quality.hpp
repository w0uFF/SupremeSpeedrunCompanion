#pragma once
#include <cstdint>
#include <limits>
#include <optional>
namespace supreme::recorder {
// Relative to a common QPC epoch; no inference of native race start.
inline std::optional<std::uint64_t> qpc_to_100ns(std::uint64_t origin,std::uint64_t tick,std::uint64_t frequency) noexcept {
    constexpr std::uint64_t units=10000000;
    if(!frequency || frequency>1000000000 || tick<origin) return {};
    const auto delta=tick-origin;
    const auto whole=delta/frequency;
    const auto fraction=(delta%frequency)*units/frequency;
    if(whole>(std::numeric_limits<std::uint64_t>::max()-fraction)/units) return {};
    return whole*units+fraction;
}
struct Coverage {
    std::uint64_t packets=0, duration=0, first=0, end=0;
    bool observe(std::uint64_t pts,std::uint64_t length) noexcept {
        if(!length || pts>std::numeric_limits<std::uint64_t>::max()-length || (packets && pts<end)) return false;
        if(!packets) first=pts;
        end=pts+length; duration+=length; ++packets; return true;
    }
    std::optional<std::uint64_t> internal_gap() const noexcept {
        if(!packets) return {};
        return end-first-duration;
    }
};
// Only successfully handed-off packets; not proof of decoded video or audio signal.
struct StreamQuality {
    Coverage written;
    std::uint64_t accepted=0, dropped_full=0, rejected_invalid=0, discarded=0;
    std::uint64_t dropped_duration=0;
};
}
