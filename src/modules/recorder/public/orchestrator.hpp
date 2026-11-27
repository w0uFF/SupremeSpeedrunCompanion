#pragma once
#include "src/modules/capture_boundary/public/ledger.hpp"
#include "src/modules/recorder/public/worker.hpp"
#include "src/modules/session/public/session.hpp"
#include <algorithm>
#include <array>
#include <optional>

namespace supreme::recorder::application {
using capture_boundary::Mode;
using Id = std::uint64_t;
struct Identity { Id session, attempt; };
inline bool operator==(Identity a, Identity b) noexcept {
    return a.session==b.session && a.attempt==b.attempt;
}
struct Request { Id capture, target; Mode mode; std::optional<Identity> attempt; Id requested_ms; };
// All methods enqueue bounded commands only. No callbacks, waits, I/O or joining.
// A successful start obligates exactly one eventual terminal notification after cleanup.
class Backend {
public:
    virtual ~Backend()=default;
    virtual bool start(const Request&) noexcept=0;
    virtual void stop(Id capture,Id through_ms) noexcept=0;
    virtual void cancel(Id capture) noexcept=0;
};
struct AttemptRecord {
    Identity identity;
    Id start_ms, sequence;
    session::AttemptState state=session::AttemptState::racing;
    bool marked=false, selection_observed=false;
    std::optional<Id> end_ms, result_ms;
};
struct Report {
    Request request;
    capture_boundary::Ledger ledger;
    std::vector<AttemptRecord> attempts;
    std::optional<Id> acquired_ms, stop_boundary_ms;
    bool stop_boundary_reached=false;
    bool incomplete=false;
    State terminal=State::starting;
    Stats stats{};
    std::array<StreamQuality,2> quality{};
    explicit Report(Request r):request(r),ledger(r.mode,r.capture){attempts.reserve(capture_boundary::Ledger::max_markers);}
};
enum class Decision { accepted, rejected, busy, unavailable };
// Owner-thread application service. Call only with already accepted Session events.
// Capture IDs are strictly increasing within this instance, never reused by its caller.
class Orchestrator {
    enum class Phase { starting, acquiring, closing, terminal };
    struct Slot {
        Report report;
        Phase phase=Phase::starting;
        Id last_ms;
        bool cancel_requested=false;
        explicit Slot(Request r):report(r),last_ms(r.requested_ms){}
    };
    Backend& backend_;
    std::array<std::optional<Slot>,4> slots_{};
    Id active_=0, highest_=0;
    bool shutdown_=false;
    Slot* find(Id id) noexcept {
        for(auto& s:slots_) if(s && s->report.request.capture==id)return &*s;
        return nullptr;
    }
    Slot* current(Id id,Id ms) noexcept {
        auto* s=find(id);
        return id==active_ && s && ms>=s->last_ms?s:nullptr;
    }
    static AttemptRecord* latest(Slot& s) noexcept {
        return s.report.attempts.empty()?nullptr:&s.report.attempts.back();
    }
    void close(Slot& s,Id ms,bool cancel,bool incomplete) noexcept {
        auto& r=s.report;
        r.incomplete|=incomplete;
        if(auto* a=latest(s);a && a->state==session::AttemptState::racing){
            r.incomplete=true;
            if(a->marked && !r.ledger.end(a->identity.session,a->identity.attempt,capture_boundary::End::abandoned,ms))r.incomplete=true;
        }
        if(s.phase!=Phase::acquiring || !r.ledger.close(ms,r.request.mode==Mode::manual,r.incomplete)){
            r.incomplete=true;cancel=true;r.ledger.failure();
        }
        s.phase=Phase::closing;s.last_ms=ms;s.cancel_requested=cancel;active_=0;
        if(cancel)backend_.cancel(r.request.capture);
        else {r.stop_boundary_ms=ms;backend_.stop(r.request.capture,ms); }
    }
    Decision open(Request r){
        if(shutdown_ || !r.capture || r.capture<=highest_ || !r.target)return Decision::rejected;
        if(active_)return Decision::busy;
        for(auto& s:slots_)if(!s){
            s.emplace(r);highest_=r.capture;
            if(!backend_.start(r)){
                s->report.incomplete=true;s->report.terminal=State::failed;s->report.ledger.failure();s->phase=Phase::terminal;
                return Decision::unavailable;
            }
            active_=r.capture;return Decision::accepted;
        }
        return Decision::busy;
    }
public:
    explicit Orchestrator(Backend& backend):backend_(backend){}
    Orchestrator(const Orchestrator&)=delete;
    Orchestrator& operator=(const Orchestrator&)=delete;
    Decision manual_start(Id capture,Id target,Id ms){return open({capture,target,Mode::manual,{},ms});}
    Decision prepare(Id capture,Id target,Identity a,Id ms){
        if(!a.session || !a.attempt)return Decision::rejected;
        return open({capture,target,Mode::automatic,a,ms});
    }
    bool acquired(Id capture,Id ms) noexcept {
        auto* s=find(capture);
        if(!s || active_!=capture || s->phase!=Phase::starting || ms<s->report.request.requested_ms)return false;
        if(!s->report.ledger.acquired(ms))return false;
        s->report.acquired_ms=ms;s->phase=Phase::acquiring;s->last_ms=std::max(ms,s->last_ms);return true;
    }
    bool selection(Id capture,Id session_id,Id ms) noexcept {
        auto* s=current(capture,ms);
        if(!s || s->phase!=Phase::acquiring || s->report.request.mode!=Mode::manual || !s->report.ledger.selection_observed(session_id,ms))return false;
        s->last_ms=ms;return true;
    }
    bool event(Id capture,const session::Event& e){
        auto* s=current(capture,e.monotonic_ms);
        if(!s || !e.session_id || !e.attempt_id || !e.sequence)return false;
        auto& r=s->report;Identity id{e.session_id,e.attempt_id};
        if(r.request.attempt && !(id==*r.request.attempt))return false;
        auto* a=latest(*s);
        if(e.kind==session::EventKind::start){
            if(!e.track || !categories::valid(*e.track) || e.igt_ms || (a && a->state==session::AttemptState::racing))return false;
            for(const auto& old:r.attempts)if(old.identity.session==id.session && (old.identity.attempt>=id.attempt || old.sequence>=e.sequence))return false;
            if(r.attempts.size()==capture_boundary::Ledger::max_markers){close(*s,e.monotonic_ms,false,true);return false;}
            if(a && a->state==session::AttemptState::finished && !a->result_ms)r.incomplete=true;
            const bool marked=s->phase==Phase::acquiring && r.ledger.begin(id.session,id.attempt,e.monotonic_ms);
            r.attempts.push_back({id,e.monotonic_ms,e.sequence,session::AttemptState::racing,marked,marked && r.ledger.observed_menu(),{}, {}});
            if(!marked || !r.acquired_ms || *r.acquired_ms>=e.monotonic_ms)r.incomplete=true;
        }else{
            if(!a || !(a->identity==id) || a->state!=session::AttemptState::racing || e.sequence<=a->sequence)return false;
            using K=session::EventKind;
            if(e.kind==K::reset_to_track){a->sequence=e.sequence;s->last_ms=e.monotonic_ms;return true;}
            capture_boundary::End end;
            if(e.kind==K::finish && e.igt_ms){a->state=session::AttemptState::finished;end=capture_boundary::End::finished;}
            else if(e.kind==K::restart_race){a->state=session::AttemptState::restarted;end=capture_boundary::End::restarted;}
            else if(e.kind==K::abandon){a->state=session::AttemptState::abandoned;end=capture_boundary::End::abandoned;}
            else return false;
            a->sequence=e.sequence;a->end_ms=e.monotonic_ms;
            if(a->marked && !r.ledger.end(id.session,id.attempt,end,e.monotonic_ms))r.incomplete=true;
            if(end!=capture_boundary::End::finished && r.request.mode==Mode::automatic){close(*s,e.monotonic_ms,false,true);return true;}
        }
        s->last_ms=e.monotonic_ms;return true;
    }
    bool result_visible(Id capture,Identity id,Id ms) noexcept {
        auto* s=current(capture,ms);if(!s)return false;
        auto* a=latest(*s);auto& r=s->report;
        if(!a || !(a->identity==id) || a->state!=session::AttemptState::finished || a->result_ms)return false;
        a->result_ms=ms;
        if(!a->marked || !r.ledger.result_observed(id.session,id.attempt,ms))r.incomplete=true;
        s->last_ms=ms;
        if(r.request.mode==Mode::automatic)close(*s,ms,false,r.incomplete);
        return true;
    }
    bool manual_stop(Id capture,Id ms) noexcept {
        auto* s=current(capture,ms);if(!s || s->report.request.mode!=Mode::manual)return false;
        auto* a=latest(*s);close(*s,ms,false,a && a->state==session::AttemptState::finished && !a->result_ms);return true;
    }
    // Also handles a restart/exit before a race start, or before result visibility.
    bool interrupt(Id capture,Id ms) noexcept {
        auto* s=current(capture,ms);if(!s || s->report.request.mode!=Mode::automatic)return false;
        close(*s,ms,false,true);return true;
    }
    bool cancel(Id capture,Id ms) noexcept {
        auto* s=find(capture);if(!s || s->phase==Phase::terminal || s->cancel_requested || ms<s->last_ms)return false;
        if(s->phase==Phase::closing){s->cancel_requested=true;s->report.incomplete=true;s->last_ms=ms;backend_.cancel(capture);}
        else close(*s,ms,true,true);
        return true;
    }
    // Backend supplies one consistent terminal Worker snapshot after cleanup, not live counters.
    bool completed(Id capture,State state,Stats stats,std::array<StreamQuality,2> quality,bool stop_boundary_reached) noexcept {
        auto* s=find(capture);
        if(!s || s->phase==Phase::terminal || (state!=State::finalized && state!=State::failed && state!=State::cancelled))return false;
        auto& r=s->report;
        if(s->phase!=Phase::closing || state!=State::finalized || s->cancel_requested || !r.ledger.finalized()){
            r.incomplete=true;r.ledger.failure();
        }
        r.stop_boundary_reached=r.stop_boundary_ms.has_value() && stop_boundary_reached;
        if(!r.stop_boundary_reached || !quality[0].written.packets || stats.dropped_full || stats.rejected_invalid || stats.discarded || stats.accepted!=stats.written)r.incomplete=true;
        r.terminal=state;r.stats=stats;r.quality=quality;s->phase=Phase::terminal;
        if(active_==capture)active_=0;
        return true;
    }
    const Report* inspect(Id capture) noexcept {auto* s=find(capture);return s?&s->report:nullptr;}
    std::optional<Report> take(Id capture){
        for(auto& s:slots_)if(s && s->report.request.capture==capture && s->phase==Phase::terminal){
            std::optional<Report> result(std::move(s->report));s.reset();return result;
        }
        return {};
    }
    void shutdown(Id ms) noexcept {
        shutdown_=true;
        for(auto& s:slots_)if(s && s->phase!=Phase::terminal)cancel(s->report.request.capture,std::max(ms,s->last_ms));
    }
};
}
