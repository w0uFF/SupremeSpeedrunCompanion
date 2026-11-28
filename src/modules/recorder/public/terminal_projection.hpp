#pragma once
#include "src/modules/recorder/public/terminal_handoff.hpp"
#include <type_traits>

namespace supreme::recorder::handoff::projection {
// Historical public values only. No reconstruction of the live Ledger.
inline constexpr std::size_t max_bytes=128u*1024u, max_line=512, max_entries=128;
// Includes tags, separators and LF. The four repeated row maxima are
// key 112, attempt 148, marker 101, result 251; at most 21 fixed lines.
static_assert(max_entries*(112+148+101+251)+21*max_line<=max_bytes);
struct LedgerView {
    Id capture{};
    capture_boundary::State phase=capture_boundary::State::failed;
    bool menu=false;
    std::vector<capture_boundary::Marker> entries;
    Id id() const noexcept{return capture;}
    capture_boundary::State state() const noexcept{return phase;}
    bool observed_menu() const noexcept{return menu;}
    const auto& markers() const noexcept{return entries;}
};
struct ReportView {
    Request request{};
    LedgerView ledger;
    std::vector<AttemptRecord> attempts;
    std::optional<Id> acquired_ms,stop_boundary_ms;
    bool stop_boundary_reached=false,incomplete=false,terminal_stats_consistent=false;
    State terminal=State::failed;
    Stats stats{};
    std::array<StreamQuality,2> quality{};
};
struct Value {
    Binding binding{};
    ReportView report;
    std::optional<Completion> completion;
    std::vector<Accepted> accepted;
    staging::Inspection media;
    Disposition disposition=Disposition::review_required;
};
// Distinct result types record the API path, not authenticity or permission.
struct LiveValue : Value {};
struct HistoricalValue : Value {};
enum class Error { none, limit, syntax, version, domain, association, arithmetic, disposition };
template<class T> struct Result { Error error; std::optional<T> value; };
namespace codec {
// Named tokens are stable wire values, not integer enum ordinals.
#define SSC_ENUM(E,x) std::pair{E::x,std::string_view(#x)}
inline constexpr auto table(Mode*){using E=Mode;return std::array{SSC_ENUM(E,automatic),SSC_ENUM(E,manual)};}
inline constexpr auto table(State*){using E=State;return std::array{SSC_ENUM(E,starting),SSC_ENUM(E,running),SSC_ENUM(E,draining),SSC_ENUM(E,cancelling),SSC_ENUM(E,finalizing),SSC_ENUM(E,finalized),SSC_ENUM(E,cancelled),SSC_ENUM(E,failed)};}
inline constexpr auto table(capture_boundary::State*){using E=capture_boundary::State;return std::array{SSC_ENUM(E,armed),SSC_ENUM(E,acquiring),SSC_ENUM(E,finalizing),SSC_ENUM(E,finalized),SSC_ENUM(E,failed)};}
inline constexpr auto table(capture_boundary::End*){using E=capture_boundary::End;return std::array{SSC_ENUM(E,finished),SSC_ENUM(E,restarted),SSC_ENUM(E,abandoned)};}
inline constexpr auto table(session::AttemptState*){using E=session::AttemptState;return std::array{SSC_ENUM(E,racing),SSC_ENUM(E,finished),SSC_ENUM(E,restarted),SSC_ENUM(E,abandoned)};}
inline constexpr auto table(categories::Track*){using E=categories::Track;return std::array{SSC_ENUM(E,alpine_easy),SSC_ENUM(E,alpine_medium),SSC_ENUM(E,alpine_hard),SSC_ENUM(E,forest_easy),SSC_ENUM(E,forest_medium),SSC_ENUM(E,forest_hard),SSC_ENUM(E,village_easy),SSC_ENUM(E,village_medium),SSC_ENUM(E,village_hard)};}
inline constexpr auto table(BackendEnd*){using E=BackendEnd;return std::array{SSC_ENUM(E,stopped),SSC_ENUM(E,cancelled),SSC_ENUM(E,startup_error),SSC_ENUM(E,source_error),SSC_ENUM(E,source_ended),SSC_ENUM(E,invalid_frame),SSC_ENUM(E,timeout),SSC_ENUM(E,handoff_rejected),SSC_ENUM(E,worker_failed),SSC_ENUM(E,internal_error)};}
inline constexpr auto table(staging::Status*){using E=staging::Status;return std::array{SSC_ENUM(E,absent),SSC_ENUM(E,pending),SSC_ENUM(E,incomplete),SSC_ENUM(E,finalized_claim),SSC_ENUM(E,review_required)};}
inline constexpr auto table(Disposition*){using E=Disposition;return std::array{SSC_ENUM(E,reconciled),SSC_ENUM(E,incomplete),SSC_ENUM(E,review_required)};}
#undef SSC_ENUM
template<class E> inline std::string_view name(E v){for(const auto& e:table(static_cast<E*>(nullptr)))if(e.first==v)return e.second;return {};}
template<class E> inline E enumeration(std::string_view s){for(const auto& e:table(static_cast<E*>(nullptr)))if(e.second==s)return e.first;throw Error::domain;}
inline bool bounded(const Value& v) noexcept {
    return v.binding.attempts.size()<=max_entries && v.report.attempts.size()<=max_entries && v.report.ledger.entries.size()<=max_entries && v.accepted.size()<=max_entries;
}
// Same arithmetic as Orchestrator's private terminal consistency check. Keep
// source parity under review; do not use Coverage::internal_gap on decoded data.
inline bool consistent(const Stats& s,const std::array<StreamQuality,2>& q) noexcept {
    const auto sum=[](Id total,Id a,Id b){return a<=total && b==total-a;};
    if(!sum(s.accepted,s.written,s.discarded) || !sum(s.accepted,q[0].accepted,q[1].accepted) ||
       !sum(s.written,q[0].written.packets,q[1].written.packets) || !sum(s.discarded,q[0].discarded,q[1].discarded) ||
       !sum(s.dropped_full,q[0].dropped_full,q[1].dropped_full) || q[0].rejected_invalid>s.rejected_invalid ||
       q[1].rejected_invalid>s.rejected_invalid-q[0].rejected_invalid)return false;
    for(const auto& stream:q){
        const auto& w=stream.written;
        if(!sum(stream.accepted,w.packets,stream.discarded))return false;
        if(w.packets){if(w.end<=w.first || w.duration<w.packets || w.duration>w.end-w.first)return false;}
        else if(w.first || w.end || w.duration)return false;
        if((!stream.dropped_full && stream.dropped_duration) || stream.dropped_duration<stream.dropped_full)return false;
    }
    return true;
}
inline Error validate(const Value& v){
    if(!bounded(v))return Error::limit;
    const auto& r=v.report;
    using L=capture_boundary::State;using A=session::AttemptState;using E=capture_boundary::End;
    if(name(v.disposition).empty() || name(r.ledger.phase).empty() ||
       (r.ledger.phase!=L::finalized && r.ledger.phase!=L::failed) ||
       (v.completion && name(v.completion->reason).empty()))return Error::domain;
    for(const auto& a:r.attempts)if(name(a.state).empty())return Error::domain;
    for(const auto& m:r.ledger.entries)if(m.end && name(*m.end).empty())return Error::domain;
    const auto expected=handoff::detail::assess(v.binding,r,v.completion,v.accepted,v.media);
    if(expected==Disposition::review_required)return Error::association;
    if(expected!=v.disposition)return Error::disposition;
    if((r.acquired_ms && *r.acquired_ms<r.request.requested_ms) ||
       (r.stop_boundary_ms && (!r.acquired_ms || *r.stop_boundary_ms<*r.acquired_ms)) ||
       (r.stop_boundary_reached && !r.stop_boundary_ms) || (r.ledger.menu && !r.acquired_ms) ||
       (r.ledger.phase==L::finalized && (!r.stop_boundary_ms || r.terminal!=State::finalized)))return Error::association;
    std::size_t marker=0;
    for(std::size_t i=0;i<r.attempts.size();++i){
        const auto& a=r.attempts[i];
        if((a.state==A::racing)==a.end_ms.has_value() || (a.selection_observed && !a.marked))return Error::association;
        if(r.stop_boundary_ms && (a.start_ms>*r.stop_boundary_ms || a.end_ms.value_or(0)>*r.stop_boundary_ms || a.result_ms.value_or(0)>*r.stop_boundary_ms))return Error::association;
        if(i){const auto& prev=r.attempts[i-1];if(!prev.end_ms || a.start_ms<prev.result_ms.value_or(*prev.end_ms))return Error::association;}
        for(std::size_t j=0;j<i;++j){const auto& old=r.attempts[j];if(old.identity.session==a.identity.session && (old.identity.attempt>=a.identity.attempt || old.sequence>=a.sequence))return Error::association;}
        if(!a.marked)continue;
        if(!r.acquired_ms || *r.acquired_ms>a.start_ms || marker==r.ledger.entries.size())return Error::association;
        const auto& m=r.ledger.entries[marker++];
        if(m.run_id!=a.identity.session || m.attempt_id!=a.identity.attempt || m.start_ms!=a.start_ms ||
           m.end.has_value()!=m.end_ms.has_value() || (m.end_ms && (*m.end_ms<m.start_ms || (r.stop_boundary_ms && *m.end_ms>*r.stop_boundary_ms))))return Error::association;
        if(!m.end){if(r.ledger.phase!=L::failed || marker!=r.ledger.entries.size() || v.disposition!=Disposition::incomplete)return Error::association;}
        else if(a.state==A::racing){if(*m.end!=E::abandoned || (r.stop_boundary_ms && m.end_ms!=r.stop_boundary_ms))return Error::association;}
        else if(m.end_ms!=a.end_ms || *m.end!=(a.state==A::finished?E::finished:a.state==A::restarted?E::restarted:E::abandoned))return Error::association;
    }
    if(marker!=r.ledger.entries.size())return Error::association;
    // A false flag may be a truthful incomplete diagnostic (including no backend
    // completion). A true flag must not contradict the actual public counters.
    if(r.terminal_stats_consistent && !consistent(r.stats,r.quality))return Error::arithmetic;
    if(v.disposition==Disposition::reconciled && (!consistent(r.stats,r.quality) || !r.quality[0].written.packets ||
       r.stats.dropped_full || r.stats.rejected_invalid || r.stats.discarded || r.stats.accepted!=r.stats.written))return Error::arithmetic;
    return Error::none;
}
struct Writer {
    static constexpr bool reading=false;
    std::string bytes="SSC-TERMINAL-1\n";
    void token(std::string_view s){bytes.push_back(' ');bytes.append(s);}
    void field(const std::string& v){if(!staging::valid_token(v))throw Error::domain;token(v);}
    void field(bool v){token(v?"1":"0");}
    template<class T> void field(const std::optional<T>& v){if(v)field(*v);else token("-");}
    template<class T> void field(T v){
        if constexpr(std::is_enum_v<T>){const auto n=name(v);if(n.empty())throw Error::domain;token(n);}
        else {static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);token(std::to_string(v));}
    }
    template<class... T> void line(std::string_view tag,const T&... fields){const auto start=bytes.size();bytes.append(tag);(field(fields),...);bytes.push_back('\n');if(bytes.size()-start>max_line || bytes.size()>max_bytes)throw Error::limit;}
};
struct Reader {
    static constexpr bool reading=true;
    std::string_view bytes,line_rest;
    explicit Reader(std::string_view s):bytes(s){
        if(s.size()>max_bytes)throw Error::limit;
        for(unsigned char c:s)if(c!='\n' && (c<32 || c>126))throw Error::syntax;
        if(next_line()!="SSC-TERMINAL-1")throw Error::version;
    }
    std::string_view next_line(){const auto n=bytes.find('\n');if(n==std::string_view::npos)throw Error::syntax;if(n+1>max_line)throw Error::limit;const auto s=bytes.substr(0,n);bytes.remove_prefix(n+1);return s;}
    std::string_view next(){
        if(line_rest.empty() || line_rest.front()!=' ')throw Error::syntax;
        line_rest.remove_prefix(1);const auto n=line_rest.find(' ');const auto s=line_rest.substr(0,n);
        if(s.empty())throw Error::syntax;
        line_rest.remove_prefix(s.size());return s;
    }
    void assign(std::string_view s,std::string& v){if(s.size()!=32)throw Error::domain;v=std::string(s);if(!staging::valid_token(v))throw Error::domain;}
    void assign(std::string_view s,bool& v){if(s=="0")v=false;else if(s=="1")v=true;else throw Error::domain;}
    template<class T> void assign(std::string_view s,std::optional<T>& v){if(s=="-"){v.reset();return;}T t{};assign(s,t);v=t;}
    template<class T> void assign(std::string_view s,T& v){
        if constexpr(std::is_enum_v<T>)v=enumeration<T>(s);
        else {static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);const auto n=staging::number(s);if(!n || *n>std::numeric_limits<T>::max())throw Error::domain;v=static_cast<T>(*n);}
    }
    template<class... T> void line(std::string_view tag,T&... fields){line_rest=next_line();if(line_rest.substr(0,tag.size())!=tag)throw Error::syntax;line_rest.remove_prefix(tag.size());(assign(next(),fields),...);if(!line_rest.empty())throw Error::syntax;}
};
template<class C,class R> void request(C& c,std::string_view tag,R& r){
    std::optional<Id> session_id,attempt_id;
    if constexpr(!C::reading){if(r.attempt){session_id=r.attempt->session;attempt_id=r.attempt->attempt;}}
    c.line(tag,r.capture,r.target,r.mode,session_id,attempt_id,r.requested_ms);
    if constexpr(C::reading){if(session_id.has_value()!=attempt_id.has_value())throw Error::domain;if(session_id)r.attempt=Identity{*session_id,*attempt_id};else r.attempt.reset();}
}
template<class C,class O> void output(C& c,std::string_view tag,O& o){c.line(tag,o.capture,o.token,o.byte_budget);}
template<class C,class S> void stats(C& c,std::string_view tag,S& s){c.line(tag,s.accepted,s.written,s.dropped_full,s.rejected_invalid,s.discarded);}
template<class C,class Q> void quality(C& c,std::string_view tag,Q& q){c.line(tag,q.written.packets,q.written.duration,q.written.first,q.written.end,q.accepted,q.dropped_full,q.rejected_invalid,q.discarded,q.dropped_duration);}
template<class C,class V> void count(C& c,std::string_view tag,V& v){Id n=static_cast<Id>(v.size());c.line(tag,n);if(n>max_entries)throw Error::limit;if constexpr(C::reading)v.resize(static_cast<std::size_t>(n));}
template<class C,class P> void walk(C& c,P& p){
    auto& b=p.binding;auto& r=p.report;
    request(c,"binding",b.request);output(c,"output",b.output);
    count(c,"keys",b.attempts);for(auto& k:b.attempts)c.line("key",k.identity.session,k.identity.attempt,k.run_id,k.session_id);
    request(c,"request",r.request);
    c.line("report",r.acquired_ms,r.stop_boundary_ms,r.stop_boundary_reached,r.incomplete,r.terminal_stats_consistent,r.terminal);
    stats(c,"stats",r.stats);quality(c,"video",r.quality[0]);quality(c,"audio",r.quality[1]);
    count(c,"attempts",r.attempts);for(auto& a:r.attempts)c.line("attempt",a.identity.session,a.identity.attempt,a.start_ms,a.sequence,a.state,a.marked,a.selection_observed,a.end_ms,a.result_ms);
    c.line("ledger",r.ledger.capture,r.ledger.phase,r.ledger.menu);
    count(c,"markers",r.ledger.entries);for(auto& m:r.ledger.entries)c.line("marker",m.run_id,m.attempt_id,m.start_ms,m.end_ms,m.end);
    bool has_completion=p.completion.has_value();c.line("completion",has_completion);
    if constexpr(C::reading){if(has_completion)p.completion.emplace();else p.completion.reset();}
    if(has_completion){auto& v=*p.completion;c.line("done",v.capture,v.reason,v.state,v.worker_state,v.high_water,v.stop_boundary_reached);stats(c,"done_stats",v.stats);quality(c,"done_video",v.quality[0]);quality(c,"done_audio",v.quality[1]);}
    count(c,"accepted",p.accepted);for(auto& a:p.accepted){auto& k=a.key;auto& v=a.attempt;c.line("result",k.identity.session,k.identity.attempt,k.run_id,k.session_id,v.id,v.started_ms,v.track,v.state,v.end_ms,v.igt_ms,v.resets,a.rta_ns);}
    bool has_media=p.media.claimed_identity.has_value();c.line("media",p.media.status,p.media.observed_bytes,has_media);
    if constexpr(C::reading){if(has_media)p.media.claimed_identity.emplace();else p.media.claimed_identity.reset();}
    if(has_media)output(c,"claim",*p.media.claimed_identity);
    c.line("disposition",p.disposition);
}
} // namespace codec
// Historical data and a live projection have the same shape, never the same
// authority. Neither creates a native handle, live event, or local admission.
inline Result<LiveValue> project(const Snapshot& s){
    const auto& r=s.report;
    if(s.binding.attempts.size()>max_entries || r.attempts.size()>max_entries || r.ledger.markers().size()>max_entries || s.accepted.size()>max_entries)return {Error::limit,{}};
    const auto classification=handoff::detail::assess(s.binding,r,s.completion,s.accepted,s.media);
    if(classification==Disposition::review_required)return {Error::association,{}};
    if(classification!=s.disposition)return {Error::disposition,{}};
    Value v{s.binding,{r.request,{r.ledger.id(),r.ledger.state(),r.ledger.observed_menu(),r.ledger.markers()},r.attempts,
        r.acquired_ms,r.stop_boundary_ms,r.stop_boundary_reached,r.incomplete,r.terminal_stats_consistent,r.terminal,r.stats,r.quality},s.completion,s.accepted,s.media,s.disposition};
    const auto e=codec::validate(v);if(e!=Error::none)return {e,{}};return {Error::none,LiveValue{std::move(v)}};
}
inline Result<std::string> encode(const Value& v){
    const auto e=codec::validate(v);if(e!=Error::none)return {e,{}};
    try{codec::Writer w;codec::walk(w,v);return {Error::none,std::move(w.bytes)};}catch(Error error){return {error,{}};}
}
inline Result<HistoricalValue> decode(std::string_view bytes){
    try{codec::Reader reader(bytes);Value v{};codec::walk(reader,v);if(!reader.bytes.empty())return {Error::syntax,{}};
        const auto canonical=encode(v);if(!canonical.value)return {canonical.error,{}};
        if(*canonical.value!=bytes)return {Error::syntax,{}};
        return {Error::none,HistoricalValue{std::move(v)}};
    }catch(Error error){return {error,{}};}
}
} // namespace supreme::recorder::handoff::projection
