#pragma once
#include "src/modules/recorder/public/worker_backend.hpp"
#include "src/modules/recorder/public/persistent_output.hpp"
#include <tuple>

namespace supreme::recorder::handoff {
using namespace application;
// Allocated by the trusted owner, not inferred from capture IDs or media metadata.
struct AttemptKey { Identity identity; std::string run_id, session_id; };
struct Binding { Request request; OutputIdentity output; std::vector<AttemptKey> attempts; };
struct Accepted { AttemptKey key; session::Attempt attempt; std::optional<Id> rta_ns; };
enum class Disposition { reconciled, incomplete, review_required };
struct Snapshot {
    Binding binding;
    Report report;
    std::optional<Completion> completion;
    std::vector<Accepted> accepted;
    staging::Inspection media;
    Disposition disposition;
};
// Reconciled means no known handoff inconsistency, never package readiness.
inline bool same_key(const AttemptKey& a,const AttemptKey& b) noexcept {
    return a.identity==b.identity && a.run_id==b.run_id && a.session_id==b.session_id;
}
inline bool same_request(const Request& a,const Request& b) noexcept {
    return a.capture==b.capture && a.target==b.target && a.mode==b.mode && a.attempt==b.attempt && a.requested_ms==b.requested_ms;
}
inline bool terminal(State s) noexcept {return s==State::finalized || s==State::failed || s==State::cancelled;}
inline bool valid(const Binding& b){
    if(!b.output.valid() || b.request.capture!=b.output.capture || !b.request.target ||
       b.attempts.size()>capture_boundary::Ledger::max_markers)return false;
    if(b.request.mode==Mode::automatic){
        if(!b.request.attempt || b.attempts.size()!=1 || !(b.attempts.front().identity==*b.request.attempt))return false;
    }else if(b.request.mode!=Mode::manual || b.request.attempt)return false;
    for(std::size_t i=0;i<b.attempts.size();++i){
        const auto& a=b.attempts[i];
        if(!a.identity.session || !a.identity.attempt || !staging::valid_token(a.run_id) || !staging::valid_token(a.session_id))return false;
        for(std::size_t j=0;j<i;++j){
            const auto& old=b.attempts[j];
            if(old.identity==a.identity || old.run_id==a.run_id ||
               ((old.identity.session==a.identity.session)!=(old.session_id==a.session_id)))return false;
        }
    }
    return true;
}
inline auto stats_fields(const Stats& s) noexcept {return std::tie(s.accepted,s.written,s.dropped_full,s.rejected_invalid,s.discarded);}
inline auto quality_fields(const StreamQuality& q) noexcept {
    return std::tie(q.written.packets,q.written.duration,q.written.first,q.written.end,q.accepted,q.dropped_full,q.rejected_invalid,q.discarded,q.dropped_duration);
}
namespace detail {
template<class R> inline bool matches_report(const Completion& c,const R& r) noexcept {
    if(c.capture!=r.request.capture || c.state!=r.terminal || !terminal(c.state) ||
       (c.worker_state && !terminal(*c.worker_state)) || c.high_water>128 ||
       static_cast<unsigned>(c.reason)>static_cast<unsigned>(BackendEnd::internal_error) ||
       stats_fields(c.stats)!=stats_fields(r.stats) || c.stop_boundary_reached!=r.stop_boundary_reached)return false;
    for(std::size_t i=0;i<c.quality.size();++i)if(quality_fields(c.quality[i])!=quality_fields(r.quality[i]))return false;
    return !c.stop_boundary_reached || (c.reason==BackendEnd::stopped && c.state==State::finalized && c.worker_state==State::finalized && r.stop_boundary_ms);
}
}
inline bool matches(const Completion& c,const Report& r) noexcept {return detail::matches_report(c,r);}
inline bool valid_accepted(const Accepted& a,const AttemptRecord& r,const AttemptKey& k) noexcept {
    const auto& v=a.attempt;
    constexpr auto max_time=static_cast<Id>(std::numeric_limits<std::int64_t>::max());
    if(!same_key(a.key,k) || !(k.identity==r.identity) || v.id!=k.identity.attempt || v.started_ms!=r.start_ms ||
       v.state!=r.state || v.end_ms!=r.end_ms || !categories::valid(v.track) || v.resets>1000000 ||
       (v.end_ms && *v.end_ms<v.started_ms) || (v.igt_ms && *v.igt_ms>max_time) || (a.rta_ns && *a.rta_ns>max_time))return false;
    switch(v.state){
    case session::AttemptState::racing:return !v.end_ms && !v.igt_ms && !a.rta_ns;
    case session::AttemptState::finished:return v.end_ms && v.igt_ms;
    case session::AttemptState::restarted:case session::AttemptState::abandoned:return v.end_ms && !v.igt_ms;
    }
    return false;
}
inline bool media_matches(const OutputIdentity& expected,const staging::Inspection& m) noexcept {
    if(m.claimed_identity && !(*m.claimed_identity==expected))return false;
    switch(m.status){
    case staging::Status::absent:return !m.claimed_identity && !m.observed_bytes;
    case staging::Status::pending:case staging::Status::incomplete:return m.claimed_identity.has_value();
    case staging::Status::finalized_claim:return m.claimed_identity && m.observed_bytes && *m.observed_bytes && *m.observed_bytes<=expected.byte_budget;
    default:return false;
    }
}
struct Result { Disposition disposition; std::optional<Snapshot> snapshot; };
namespace detail {
// Shared pure public-value classification; R may be a historical projection.
template<class R> inline Disposition assess(const Binding& b,const R& r,const std::optional<Completion>& c,
                       const std::vector<Accepted>& accepted,const staging::Inspection& m){
    const auto review=Disposition::review_required;
    if(!valid(b) || !same_request(b.request,r.request) || !terminal(r.terminal) || r.ledger.id()!=b.output.capture ||
       r.attempts.size()>b.attempts.size() || accepted.size()>r.attempts.size() ||
       (b.request.mode==Mode::manual && r.attempts.size()!=b.attempts.size()) || !media_matches(b.output,m) ||
       (c && !matches_report(*c,r)))return review;
    bool incomplete=r.incomplete || !r.terminal_stats_consistent || !c || c->reason!=BackendEnd::stopped ||
        c->worker_state!=State::finalized || r.terminal!=State::finalized || !r.stop_boundary_reached ||
        r.ledger.state()!=capture_boundary::State::finalized || m.status!=staging::Status::finalized_claim ||
        accepted.size()!=r.attempts.size() || r.attempts.empty();
    std::size_t supplied=0;
    for(std::size_t i=0;i<r.attempts.size();++i){
        const auto& a=r.attempts[i];const auto& k=b.attempts[i];
        if(!(a.identity==k.identity) || !a.sequence || a.start_ms<r.request.requested_ms ||
           (a.end_ms && *a.end_ms<a.start_ms) || (a.result_ms && (!a.end_ms || *a.result_ms<*a.end_ms || a.state!=session::AttemptState::finished)))return review;
        if(supplied<accepted.size() && same_key(accepted[supplied].key,k)){
            if(!valid_accepted(accepted[supplied],a,k))return review;
            if(!accepted[supplied].rta_ns)incomplete=true;
            ++supplied;
        }else incomplete=true;
        if(!a.marked || a.state!=session::AttemptState::finished || !a.result_ms || !r.acquired_ms || *r.acquired_ms>=a.start_ms)incomplete=true;
    }
    if(supplied!=accepted.size())return review;
    return incomplete?Disposition::incomplete:Disposition::reconciled;
}
}
// Only trusted owner snapshots after terminal take(), cleanup and quiescent media inspection.
// Missing accepted results may be retained as incomplete; contradictory identities refuse.
// Caller retains original inputs on refusal; no mutation, journal codec or local admission.
inline Result assemble(const Binding& b,const Report& r,const std::optional<Completion>& c,
                       const std::vector<Accepted>& accepted,const staging::Inspection& m){
    const auto disposition=detail::assess(b,r,c,accepted,m);
    if(disposition==Disposition::review_required)return {disposition,{}};
    return {disposition,Snapshot{b,r,c,accepted,m,disposition}};
}
enum class Recovery { matching_claim, incomplete, changed_or_review, not_quiescent, unavailable };
// Compare current read-only staging observation with the retained snapshot. No
// journal reconstruction, restart, automatic repair or promotion from a marker.
inline Recovery recover(const Snapshot& retained,const staging::Store* store,bool quiescent){
    if(!quiescent)return Recovery::not_quiescent;
    if(!store)return Recovery::unavailable;
    const auto prior=assemble(retained.binding,retained.report,retained.completion,retained.accepted,retained.media);
    if(!prior.snapshot || prior.disposition!=retained.disposition)return Recovery::changed_or_review;
    const auto current=staging::inspect(*store,retained.binding.output.token);
    if(!media_matches(retained.binding.output,current) || current.status!=retained.media.status ||
       current.observed_bytes!=retained.media.observed_bytes || current.claimed_identity.has_value()!=retained.media.claimed_identity.has_value())return Recovery::changed_or_review;
    return retained.disposition==Disposition::reconciled?Recovery::matching_claim:Recovery::incomplete;
}
// Compare only the public immutable terminal evidence; no raw object or private
// Ledger memory is serialized. A journal adapter must preserve these fields.
inline bool same_snapshot(const Snapshot& a,const Snapshot& b) noexcept {
    if(!same_request(a.binding.request,b.binding.request) || !(a.binding.output==b.binding.output) ||
       a.binding.attempts.size()!=b.binding.attempts.size() || a.accepted.size()!=b.accepted.size() ||
       a.disposition!=b.disposition || a.media.status!=b.media.status || a.media.observed_bytes!=b.media.observed_bytes ||
       a.media.claimed_identity.has_value()!=b.media.claimed_identity.has_value() ||
       (a.media.claimed_identity && !(*a.media.claimed_identity==*b.media.claimed_identity)))return false;
    for(std::size_t i=0;i<a.binding.attempts.size();++i)if(!same_key(a.binding.attempts[i],b.binding.attempts[i]))return false;
    for(std::size_t i=0;i<a.accepted.size();++i){
        const auto& x=a.accepted[i];const auto& y=b.accepted[i];const auto& v=x.attempt;const auto& w=y.attempt;
        if(!same_key(x.key,y.key) || x.rta_ns!=y.rta_ns ||
           std::tie(v.id,v.started_ms,v.track,v.state,v.end_ms,v.igt_ms,v.resets)!=
           std::tie(w.id,w.started_ms,w.track,w.state,w.end_ms,w.igt_ms,w.resets))return false;
    }
    const auto& x=a.report;const auto& y=b.report;
    if(!same_request(x.request,y.request) || x.ledger.id()!=y.ledger.id() || x.ledger.state()!=y.ledger.state() ||
       x.ledger.observed_menu()!=y.ledger.observed_menu() || x.attempts.size()!=y.attempts.size() ||
       std::tie(x.acquired_ms,x.stop_boundary_ms,x.stop_boundary_reached,x.incomplete,x.terminal_stats_consistent,x.terminal)!=
       std::tie(y.acquired_ms,y.stop_boundary_ms,y.stop_boundary_reached,y.incomplete,y.terminal_stats_consistent,y.terminal) ||
       stats_fields(x.stats)!=stats_fields(y.stats))return false;
    for(std::size_t i=0;i<x.quality.size();++i)if(quality_fields(x.quality[i])!=quality_fields(y.quality[i]))return false;
    for(std::size_t i=0;i<x.attempts.size();++i){
        const auto& v=x.attempts[i];const auto& w=y.attempts[i];
        if(!(v.identity==w.identity) || std::tie(v.start_ms,v.sequence,v.state,v.marked,v.selection_observed,v.end_ms,v.result_ms)!=
           std::tie(w.start_ms,w.sequence,w.state,w.marked,w.selection_observed,w.end_ms,w.result_ms))return false;
    }
    const auto& l=x.ledger.markers();const auto& m=y.ledger.markers();
    if(l.size()!=m.size())return false;
    for(std::size_t i=0;i<l.size();++i){
        const auto& v=l[i];const auto& w=m[i];
        if(std::tie(v.run_id,v.attempt_id,v.start_ms,v.end_ms,v.end)!=std::tie(w.run_id,w.attempt_id,w.start_ms,w.end_ms,w.end))return false;
    }
    if(a.completion.has_value()!=b.completion.has_value())return false;
    if(!a.completion)return true;
    const auto& c=*a.completion;const auto& d=*b.completion;
    if(std::tie(c.capture,c.reason,c.state,c.worker_state,c.high_water,c.stop_boundary_reached)!=
       std::tie(d.capture,d.reason,d.state,d.worker_state,d.high_water,d.stop_boundary_reached) || stats_fields(c.stats)!=stats_fields(d.stats))return false;
    for(std::size_t i=0;i<c.quality.size();++i)if(quality_fields(c.quality[i])!=quality_fields(d.quality[i]))return false;
    return true;
}
// Trusted serialized owner, append-only typed boundary, not an on-disk schema.
// Adapter must enforce global token/run-ID uniqueness, bounded readback and
// immutable no-replace storage, and report failures rather than absence.
class Journal {
public:
    virtual ~Journal()=default;
    virtual std::optional<Snapshot> find(const std::string& token) const=0;
    virtual void append_new(const Snapshot&)=0;
};
enum class Retention { invalid, unavailable, conflict, unchanged, retained, uncertain };
// Explicit one-attempt retention, never automatically retry or release caller data.
// An exception after attempting append stays uncertain even with matching readback.
inline Retention retain(const Snapshot& desired,Journal* journal){
    const auto validated=assemble(desired.binding,desired.report,desired.completion,desired.accepted,desired.media);
    if(!validated.snapshot || validated.disposition!=desired.disposition)return Retention::invalid;
    if(!journal)return Retention::unavailable;
    try {
        const auto existing=journal->find(desired.binding.output.token);
        if(existing)return same_snapshot(*existing,desired)?Retention::unchanged:Retention::conflict;
    }catch(...){return Retention::unavailable;}
    bool acknowledged=false;
    try {journal->append_new(desired);acknowledged=true;}catch(...){/* read back, but keep uncertainty */}
    try {
        const auto observed=journal->find(desired.binding.output.token);
        if(acknowledged && observed && same_snapshot(*observed,desired))return Retention::retained;
    }catch(...){/* caller keeps original snapshot */}
    return Retention::uncertain;
}
}
