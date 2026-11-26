#pragma once
#include "src/modules/submission/public/bound_track_claims.hpp"

namespace supreme::submission {
// All timestamps are supplied claims in one explicitly identified tick domain.
// Interval containment is not proof of actual continuous video or native events.
struct ClaimClock {
    std::string epoch;
    std::uint64_t frequency=0;
};
struct AttemptIntervalClaim {
    TrackAttemptKey key;
    ClaimClock clock;
    std::uint64_t start_sequence=0,finish_sequence=0,result_sequence=0;
    std::uint64_t start_tick=0,finish_tick=0,result_tick=0;
};
struct VideoIntervalClaim {
    TrackAttemptKey key;
    ClaimClock clock;
    std::uint64_t begin_tick=0,end_tick=0;
};
inline bool valid_claim_clock(const ClaimClock& c) noexcept {
    return valid_claim_id(c.epoch) && c.frequency!=0;
}
inline TrackClaimStatus compare_track_interval_claims(
        const BoundTrackClaims& tracks,
        const std::optional<AttemptIntervalClaim>& attempt,
        const std::optional<VideoIntervalClaim>& video) noexcept {
    const auto base=compare_bound_track_claims(tracks);
    if(base==TrackClaimStatus::malformed)return base;
    if(attempt && (!valid_key(attempt->key) || !valid_claim_clock(attempt->clock) ||
       !attempt->start_sequence || attempt->start_sequence>=attempt->finish_sequence ||
       attempt->finish_sequence>=attempt->result_sequence ||
       attempt->start_tick>=attempt->finish_tick || attempt->finish_tick>attempt->result_tick))
        return TrackClaimStatus::malformed;
    if(video && (!valid_key(video->key) || !valid_claim_clock(video->clock) ||
       video->begin_tick>=video->end_tick))return TrackClaimStatus::malformed;
    if(base==TrackClaimStatus::conflicting ||
       (attempt && !same_key(attempt->key,tracks.expected)) ||
       (video && !same_key(video->key,tracks.expected)))return TrackClaimStatus::conflicting;
    if(!attempt || !video)return TrackClaimStatus::incomplete;
    if(attempt->clock.epoch!=video->clock.epoch ||
       attempt->clock.frequency!=video->clock.frequency)return TrackClaimStatus::incomplete;
    if(video->begin_tick>attempt->start_tick || video->end_tick<attempt->result_tick)
        return TrackClaimStatus::incomplete;
    return base;
}
}
