#pragma once
#include "src/modules/categories/public/catalog.hpp"
#include <array>
#include <optional>

namespace supreme::submission {
// Offline claim comparison only. No adapter trust, video inspection, replay
// parsing, proof of continuity, attempt binding, or eligibility is implied.
enum class TrackClaimStatus { malformed, conflicting, incomplete, consistent_claims };
struct TrackClaims {
    std::optional<categories::Track> start, finish, video_label, manifest, replay;
    bool replay_expected=false;
    bool video_coverage_claimed=false;
};
inline TrackClaimStatus compare_track_claims(const TrackClaims& claims) noexcept {
    const std::array<std::optional<categories::Track>,5> values{
        claims.start,claims.finish,claims.video_label,claims.manifest,claims.replay};
    for(const auto& value:values)
        if(value && !categories::valid(*value))return TrackClaimStatus::malformed;
    std::optional<categories::Track> reference;
    for(const auto& value:values)if(value){
        if(reference && *reference!=*value)return TrackClaimStatus::conflicting;
        reference=value;
    }
    if(!claims.start || !claims.finish || !claims.video_label || !claims.manifest ||
       !claims.video_coverage_claimed || (claims.replay_expected && !claims.replay))
        return TrackClaimStatus::incomplete;
    return TrackClaimStatus::consistent_claims;
}
}
