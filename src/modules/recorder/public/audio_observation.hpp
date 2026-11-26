#pragma once
#include <cstdint>
#include <limits>
#include <optional>
namespace supreme::recorder {
// Source device positions (PCM sample frames), not encoder packet indices.
struct AudioObservation {
    std::uint64_t packets=0, frames=0, silent=0, discontinuities=0, timestamp_errors=0, gaps=0, regressions=0;
    std::optional<std::uint64_t> next_position;
    bool observe(std::uint64_t position,std::uint32_t count,bool silence,bool discontinuity,bool timestamp_error) noexcept {
        const auto max=std::numeric_limits<std::uint64_t>::max();
        if(!count || position>max-count || frames>max-count || packets==max)return false;
        const auto gap=(!timestamp_error && next_position && position>*next_position)?position-*next_position:0;
        if(gaps>max-gap)return false;
        ++packets;frames+=count;
        if(silence)silent+=count;
        if(discontinuity)++discontinuities;
        if(timestamp_error){++timestamp_errors;next_position.reset();return true;}
        if(next_position && position<*next_position)++regressions;
        gaps+=gap;next_position=position+count;return true;
    }
};
}
