#pragma once
#include "src/modules/recorder/public/timed_frames.hpp"
#include "src/modules/recorder/public/worker.hpp"
namespace supreme::recorder {
// Selected-frame span excludes the explicit terminal hold. Units: 100 ns.
struct LookaheadPolicy {
    std::size_t frame_limit=1440;
    std::uint64_t span_limit=900000000, hold=fixture_max_hold, tail=fixture_tail;
    // Provisional opt-in ceiling, not a recommended capture frame rate or storage budget.
    static constexpr std::size_t ceiling_frames=3000000;
    static constexpr std::uint64_t ceiling_span=6ull*60*60*10000000;
    static constexpr LookaheadPolicy extended() noexcept {
        return {ceiling_frames,ceiling_span,fixture_max_hold,fixture_tail};
    }
    constexpr bool valid() const noexcept {
        return frame_limit>=2 && frame_limit<=ceiling_frames && span_limit && span_limit<=ceiling_span &&
               hold && hold<=10000000 && tail && tail<=hold;
    }
};
// One producer, one owned lookahead frame. No encoding or blocking I/O here.
// Legacy fixture constructor and limits remain unchanged by default.
class FrameLookahead {
    std::optional<TimedFrame> pending_;
    std::uint64_t origin_=0;
    std::size_t count_=0;
    bool closed_=false;
    const std::size_t frame_bytes_;
    const LookaheadPolicy policy_;

public:
    static constexpr std::size_t max_frames=1440;
    static constexpr std::uint64_t max_span=900000000;
    explicit FrameLookahead(std::size_t frame_bytes=fixture_bytes,LookaheadPolicy policy={}):frame_bytes_(frame_bytes),policy_(policy){
        if(!frame_bytes || frame_bytes>16u*1024u*1024u || !policy.valid())throw std::invalid_argument("frame lookahead policy");
    }
    std::size_t count()const noexcept{return count_;}
    std::optional<Packet> push(TimedFrame frame){
        if(closed_ || count_==policy_.frame_limit || frame.pixels.size()!=frame_bytes_ ||
           (pending_ && (frame.time<=pending_->time || frame.time-origin_>policy_.span_limit)))
            throw std::invalid_argument("live frame sequence");
        std::optional<Packet> ready;
        if(pending_){
            ready.emplace(Packet{Stream::video,pending_->time-origin_,
                std::min(frame.time-pending_->time,policy_.hold),std::move(pending_->pixels)});
        }else origin_=frame.time;
        pending_=std::move(frame);++count_;return ready;
    }
    Packet finish(){
        if(closed_ || count_<2 || !pending_)throw std::logic_error("live frame terminal state");
        closed_=true;
        Packet ready{Stream::video,pending_->time-origin_,policy_.tail,std::move(pending_->pixels)};
        pending_.reset();return ready;
    }
};
}
