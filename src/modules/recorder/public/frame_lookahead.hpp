#pragma once
#include "src/modules/recorder/public/timed_frames.hpp"
#include "src/modules/recorder/public/worker.hpp"
namespace supreme::recorder {
// One producer, one owned lookahead frame. No encoding or blocking I/O here.
// This is a bounded fixture, not an unbounded session recorder.
class FrameLookahead {
    std::optional<TimedFrame> pending_;
    std::uint64_t origin_=0;
    std::size_t count_=0;
    bool closed_=false;
public:
    static constexpr std::size_t max_frames=720;
    static constexpr std::uint64_t max_span=450000000;
    std::size_t count()const noexcept{return count_;}
    std::optional<Packet> push(TimedFrame frame){
        if(closed_ || count_==max_frames || frame.pixels.size()!=fixture_bytes ||
           (pending_ && (frame.time<=pending_->time || frame.time-origin_>max_span)))
            throw std::invalid_argument("live frame sequence");
        std::optional<Packet> ready;
        if(pending_){
            ready.emplace(Packet{Stream::video,pending_->time-origin_,
                std::min(frame.time-pending_->time,fixture_max_hold),std::move(pending_->pixels)});
        }else origin_=frame.time;
        pending_=std::move(frame);++count_;return ready;
    }
    Packet finish(){
        if(closed_ || count_<2 || !pending_)throw std::logic_error("live frame terminal state");
        closed_=true;
        Packet ready{Stream::video,pending_->time-origin_,fixture_tail,std::move(pending_->pixels)};
        pending_.reset();return ready;
    }
};
}
