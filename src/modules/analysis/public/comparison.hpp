#pragma once
#include "src/modules/categories/public/catalog.hpp"
#include "src/modules/playback/public/planner.hpp"

namespace supreme::analysis {
namespace exchange=exporter::exchange;
using exchange::Verified;
using playback::Member;
using run_library::Origin;
using run_library::Status;
using run_library::Repository;
using categories::Track;
// No conversion or fallback between these domains.
enum class Metric { igt_ms, rta_ns, video_interval_100ns, replay_candidate_ticks };
struct Subject { Verified archive; std::optional<Member> member; };
inline bool record_metric(Metric m) noexcept {return m==Metric::igt_ms || m==Metric::rta_ns;}
inline bool same_member(const Member& a,const Member& b) noexcept {
    return a.kind==b.kind && a.name==b.name && a.sha256==b.sha256 && a.bytes==b.bytes;
}
inline bool same(const Subject& a,const Subject& b) noexcept {
    return exchange::same(a.archive,b.archive) && a.member.has_value()==b.member.has_value() &&
        (!a.member || same_member(*a.member,*b.member));
}
inline bool valid(const Subject& s,Metric m){
    if(!exchange::valid(s.archive))return false;
    if(record_metric(m))return !s.member;
    if(m!=Metric::video_interval_100ns && m!=Metric::replay_candidate_ticks)return false;
    if(!s.member || !playback::valid_member(*s.member,s.archive))return false;
    return m==Metric::video_interval_100ns?s.member->kind==playback::Kind::video:
        s.member->kind==playback::Kind::native_replay && s.member->bytes<=8ull*1024*1024;
}
inline std::optional<std::uint64_t> claim(const Subject& s,Metric m) noexcept {
    if(m==Metric::igt_ms)return s.archive.projection.igt_ms;
    if(m==Metric::rta_ns)return s.archive.projection.rta_ns;
    return {};
}
// Trusted independent adapter establishes track/difficulty, single-attempt boundaries,
// measurement source and matching semantic version. Agreement of claims alone is insufficient.
struct Measurement { Subject bound; Metric metric; Track track; std::string semantics; std::uint64_t value; };
enum class Evidence { unavailable, insufficient, unsupported, review, established };
struct Observation {
    exchange::Slot slot;
    std::optional<Member> member; // exact independently verified requested member, if present
    Evidence evidence=Evidence::unavailable;
    std::optional<Measurement> measurement;
};
class Source {
public:
    virtual ~Source()=default;
    // Read-only trusted, serialized, quiescent I/O; no extraction/decoder/native calls.
    virtual Observation observe(const Subject&,Metric) const=0;
};
enum class State { compared, invalid_request, unsupported_local, missing, stale_selection,
    unavailable, insufficient_evidence, unsupported, incompatible, review_required };
struct Claims { std::optional<std::uint64_t> first,second; };
struct Values { Measurement first,second; std::int64_t second_minus_first; };
struct Result {
    State state;
    Metric metric;
    std::optional<Claims> recorded_claims; // unverified indexed values, never substitute for measurement
    std::optional<Values> values; // descriptive delta, not an official result or PB
};
inline constexpr auto signed_max=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
inline bool valid_value(Metric m,std::uint64_t v) noexcept {
    if(record_metric(m))return v<=signed_max;
    if(m==Metric::video_interval_100ns)return v<=(6ull*60*60+1)*10000000;
    return m==Metric::replay_candidate_ticks && v<=65535;
}
class Controller {
    const Repository& repository_;
    const Source* source_;
    State read(const Subject& s,Metric m,Observation& out) const {
        const auto row=repository_.find(s.archive.projection.run_id);
        if(!row)return State::missing;
        if(!run_library::identical(*row,s.archive.projection))return State::stale_selection;
        out=source_->observe(s,m);
        if(!exchange::valid_slot(out.slot) || (out.slot.state!=exchange::SlotState::finalized &&
            (out.member || out.measurement || out.evidence!=Evidence::unavailable)))return State::review_required;
        if(out.slot.state==exchange::SlotState::absent)return State::missing;
        if(out.slot.state==exchange::SlotState::unavailable)return State::unavailable;
        if(out.slot.state!=exchange::SlotState::finalized)return State::review_required;
        if(!exchange::same(*out.slot.archive,s.archive))return State::stale_selection;
        if(s.member && !out.member)return out.measurement?State::review_required:State::missing;
        if(!s.member && out.member)return State::review_required;
        if(out.member && (!playback::valid_member(*out.member,s.archive) || !same_member(*out.member,*s.member)))
            return State::stale_selection;
        return State::compared; // current identity only; no measurement decision yet
    }
    static State assess(const Subject& s,Metric m,const Observation& o){
        if(o.evidence!=Evidence::established){
            if(o.measurement)return State::review_required;
            switch(o.evidence){
            case Evidence::unavailable:return State::unavailable;
            case Evidence::insufficient:return State::insufficient_evidence;
            case Evidence::unsupported:return State::unsupported;
            default:return State::review_required;
            }
        }
        if(!o.measurement)return State::review_required;
        const auto& a=*o.measurement;
        if(!same(a.bound,s))return State::stale_selection;
        if(a.metric!=m || !categories::valid(a.track) || !run_library::key(a.semantics) || !valid_value(m,a.value))
            return State::review_required;
        if(s.archive.projection.status!=Status::finished || a.semantics=="unknown")return State::insufficient_evidence;
        if(record_metric(m)){
            const auto c=claim(s,m);
            if(!c || s.archive.projection.event_semantics=="unknown")return State::insufficient_evidence;
            if(a.semantics!=s.archive.projection.event_semantics || *c!=a.value)return State::review_required;
        }
        return State::compared;
    }
public:
    explicit Controller(const Repository& repo,const Source* source=nullptr):repository_(repo),source_(source){}
    Result compare(const Subject& first,const Subject& second,Metric metric) const {
        Result r{State::invalid_request,metric,{},{}};
        if(first.archive.projection.origin==Origin::local || second.archive.projection.origin==Origin::local){r.state=State::unsupported_local;return r;}
        if(!valid(first,metric) || !valid(second,metric) || first.archive.projection.run_id==second.archive.projection.run_id)return r;
        if(!source_){r.state=State::unavailable;return r;}
        try {
            Observation a{},b{};
            r.state=read(first,metric,a);if(r.state!=State::compared)return r;
            r.state=read(second,metric,b);if(r.state!=State::compared)return r;
            if(record_metric(metric))r.recorded_claims=Claims{claim(first,metric),claim(second,metric)};
            const auto x=assess(first,metric,a),y=assess(second,metric,b);
            // Refuse both sides; surface malformed/stale evidence before lesser omissions.
            for(auto state:{State::review_required,State::stale_selection,State::unsupported,State::unavailable,State::insufficient_evidence})
                if(x==state || y==state){r.state=state;return r;}
            const auto& l=*a.measurement;const auto& h=*b.measurement;
            if(l.track!=h.track || l.semantics!=h.semantics){r.state=State::incompatible;return r;}
            // Both inputs bounded by INT64_MAX: signed subtraction cannot overflow.
            r.values=Values{l,h,static_cast<std::int64_t>(h.value)-static_cast<std::int64_t>(l.value)};
            r.state=State::compared;return r;
        }catch(...){return {State::unavailable,metric,{},{}};}
    }
};
}
