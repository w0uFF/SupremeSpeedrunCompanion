#pragma once
#include "contracts/observation.hpp"
#include <limits>
namespace supreme::timing {
inline constexpr std::uint64_t ns_per_second=1000000000;
// Exact truncation for supported frequencies; raw ticks remain authoritative.
inline std::optional<std::uint64_t> elapsed_ns(ClockSample begin, ClockSample end) noexcept {
    const auto f=begin.frequency_hz;
    if (!f || f>ns_per_second || f!=end.frequency_hz || end.ticks<begin.ticks) return {};
    const auto delta=end.ticks-begin.ticks;
    const auto whole=delta/f, fraction=(delta%f)*ns_per_second/f;
    if (whole>(std::numeric_limits<std::uint64_t>::max()-fraction)/ns_per_second) return {};
    return whole*ns_per_second+fraction;
}
enum class State { idle, running, finished, invalid };
struct Result { ClockSample begin, end; std::uint64_t nanoseconds; };
// One interval per attempt, owner-thread only. Explicit caller boundaries, not engine events.
class Interval {
    const IClock& clock_;
    State state_=State::idle;
    ClockSample begin_{}, last_{};
    std::optional<Result> result_;
    bool observe() {
        try {
            auto next=clock_.sample();
            if (next.ticks>=last_.ticks && elapsed_ns(begin_,next)) {last_=next; return true;}
        } catch (...) {} // Clock failure must not fabricate a time.
        state_=State::invalid; return false;
    }
public:
    explicit Interval(const IClock& clock):clock_(clock) {}
    Interval(const Interval&)=delete;
    Interval& operator=(const Interval&)=delete;
    State state() const noexcept {return state_;}
    bool start() {
        if(state_!=State::idle) return false;
        try {
            begin_=last_=clock_.sample();
            if(elapsed_ns(begin_,begin_)) {state_=State::running; return true;}
        } catch (...) {}
        state_=State::invalid; return false;
    }
    std::optional<std::uint64_t> poll() {
        if(state_==State::finished) return result_->nanoseconds;
        if(state_!=State::running || !observe()) return {};
        return elapsed_ns(begin_,last_);
    }
    bool finish() {
        if(state_!=State::running || !observe()) return false;
        result_=Result{begin_,last_,*elapsed_ns(begin_,last_)};
        state_=State::finished; return true;
    }
    std::optional<Result> result() const noexcept {return result_;}
};
}
