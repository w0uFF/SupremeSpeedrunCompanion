#pragma once
#include "src/modules/submission/public/track_interval_claims.hpp"
#include <vector>

namespace supreme::submission {
// Caller-supplied half-open coverage spans. No pixel or provenance verification.
struct CoverageSpan { std::uint64_t begin_tick=0,end_tick=0; };
struct TrackCoverageClaim {
    TrackAttemptKey key;
    ClaimClock clock;
    std::uint64_t pre_start_sequence=0,result_visible_sequence=0;
    std::uint64_t pre_start_tick=0,result_visible_end_tick=0;
    std::vector<CoverageSpan> spans;
};
inline constexpr std::size_t max_coverage_spans=256;
inline TrackClaimStatus compare_track_coverage_claims(
    const BoundTrackClaims& tracks,const std::optional<AttemptIntervalClaim>& attempt,
    const std::optional<VideoIntervalClaim>& video,
    const std::optional<TrackCoverageClaim>& coverage) noexcept {
    const auto base=compare_track_interval_claims(tracks,attempt,video);
    if(base==TrackClaimStatus::malformed)return base;
    if(coverage){
        const auto& c=*coverage;
        if(!valid_key(c.key)||!valid_claim_clock(c.clock)||!c.pre_start_sequence||
           c.pre_start_sequence>=c.result_visible_sequence||c.pre_start_tick>=c.result_visible_end_tick||
           c.spans.size()>max_coverage_spans)return TrackClaimStatus::malformed;
        std::uint64_t previous=0;
        for(const auto& span:c.spans){
            if(span.begin_tick>=span.end_tick||span.begin_tick<previous)return TrackClaimStatus::malformed;
            previous=span.end_tick;
        }
        if(attempt && same_key(c.key,attempt->key) && c.clock.epoch==attempt->clock.epoch && c.clock.frequency==attempt->clock.frequency &&
           (c.pre_start_sequence>=attempt->start_sequence||c.result_visible_sequence<=attempt->result_sequence||
            c.pre_start_tick>=attempt->start_tick||c.result_visible_end_tick<=attempt->result_tick))
            return TrackClaimStatus::malformed;
        if(video && same_key(c.key,video->key) && c.clock.epoch==video->clock.epoch && c.clock.frequency==video->clock.frequency)
            for(const auto& span:c.spans)
                if(span.begin_tick<video->begin_tick||span.end_tick>video->end_tick)return TrackClaimStatus::malformed;
    }
    if(base==TrackClaimStatus::conflicting || (coverage&&!same_key(coverage->key,tracks.expected)))
        return TrackClaimStatus::conflicting;
    if(base!=TrackClaimStatus::consistent_claims||!coverage||!attempt||!video)return TrackClaimStatus::incomplete;
    const auto& c=*coverage;
    if(c.clock.epoch!=attempt->clock.epoch||c.clock.frequency!=attempt->clock.frequency||
       c.pre_start_tick<video->begin_tick||c.result_visible_end_tick>video->end_tick)return TrackClaimStatus::incomplete;
    auto covered=c.pre_start_tick;
    for(const auto& span:c.spans){
        if(span.end_tick<=covered)continue;
        if(span.begin_tick>covered)return TrackClaimStatus::incomplete;
        covered=span.end_tick;
        if(covered>=c.result_visible_end_tick)return TrackClaimStatus::consistent_claims;
    }
    return TrackClaimStatus::incomplete;
}
}
