#pragma once
#include "src/modules/submission/public/track_claims.hpp"
#include <cstdint>
#include <string>

namespace supreme::submission {
// Caller-supplied per-leg binding, not proof of native acquisition or timing.
struct TrackAttemptKey {
    std::string run_id,session_id;
    std::uint64_t attempt=0,leg=0;
};
inline bool valid_claim_id(const std::string& id) noexcept {
    if(id.size()!=32)return false;
    for(char c:id)if(!((c>='0' && c<='9') || (c>='a' && c<='f')))return false;
    return true;
}
inline bool valid_key(const TrackAttemptKey& k) noexcept {
    return valid_claim_id(k.run_id) && valid_claim_id(k.session_id) && k.attempt && k.leg;
}
inline bool same_key(const TrackAttemptKey& a,const TrackAttemptKey& b) noexcept {
    return a.run_id==b.run_id && a.session_id==b.session_id && a.attempt==b.attempt && a.leg==b.leg;
}
struct BoundTrackClaim {
    TrackAttemptKey key;
    std::optional<categories::Track> track;
};
struct BoundTrackClaims {
    TrackAttemptKey expected;
    std::optional<categories::Track> category_track;
    std::optional<BoundTrackClaim> start,finish,video_label,manifest,replay;
    bool replay_expected=false,video_coverage_claimed=false;
};
inline TrackClaimStatus compare_bound_track_claims(const BoundTrackClaims& claims) noexcept {
    if(!valid_key(claims.expected) || (claims.category_track && !categories::valid(*claims.category_track)))
        return TrackClaimStatus::malformed;
    const std::array<const std::optional<BoundTrackClaim>*,5> sources{
        &claims.start,&claims.finish,&claims.video_label,&claims.manifest,&claims.replay};
    // Check malformed data first, even if an earlier source conflicts.
    for(auto source:sources)if(*source){
        const auto& c=**source;
        if(!valid_key(c.key) || (c.track && !categories::valid(*c.track)))return TrackClaimStatus::malformed;
    }
    for(auto source:sources)if(*source){
        const auto& c=**source;
        if(!same_key(c.key,claims.expected) ||
           (claims.category_track && c.track && *claims.category_track!=*c.track))
            return TrackClaimStatus::conflicting;
    }
    const auto track=[](const std::optional<BoundTrackClaim>& c) {
        return c?c->track:std::optional<categories::Track>{};
    };
    return compare_track_claims({track(claims.start),track(claims.finish),track(claims.video_label),
                                track(claims.manifest),track(claims.replay),claims.replay_expected,claims.video_coverage_claimed});
}
}
