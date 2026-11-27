#pragma once
#include "src/modules/exporter/public/archive_exchange.hpp"

namespace supreme::playback {
namespace exchange=exporter::exchange;
using run_library::Record;
using run_library::Repository;
using run_library::Origin;
using exchange::Verified;
enum class Kind { video, native_replay };
struct Member {
    Kind kind;
    std::string name, sha256;
    std::uint64_t bytes;
};
struct Selection { Verified archive; Member member; };
inline bool valid_kind(Kind k) noexcept {return k==Kind::video || k==Kind::native_replay;}
// Bounded display/selection shape only. The existing verifier owns member policy.
inline bool valid_name(std::string_view s) noexcept {
    const auto alnum=[](char c){return (c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9');};
    if(s.empty() || s.size()>127 || !alnum(s.front()) || s.back()=='.')return false;
    for(char c:s)if(!alnum(c) && c!='_' && c!='-' && c!='.')return false;
    return true;
}
inline bool valid_member(const Member& m,const Verified& a) noexcept {
    return valid_kind(m.kind) && valid_name(m.name) && run_library::hex_id(m.sha256,64) &&
        m.bytes && m.bytes<=1024ull*1024*1024 && m.bytes<=a.bytes;
}
inline bool same(const Selection& a,const Selection& b) noexcept {
    return exchange::same(a.archive,b.archive) && a.member.kind==b.member.kind &&
        a.member.name==b.member.name && a.member.sha256==b.member.sha256 && a.member.bytes==b.member.bytes;
}
struct Observation {
    exchange::Slot slot;
    std::optional<Member> member; // exact verified member, absent if not listed with requested role
};
// Trusted read-only owner-thread port. Reuse existing archive verification and
// whole-container identity; never interpret a member name as a filesystem path.
class Source {
public:
    virtual ~Source()=default;
    virtual Observation observe(const Record&,std::string_view,Kind) const=0;
};
enum class Gate { unavailable, unsupported, review, established };
struct Capabilities {
    Selection bound;
    Gate member_policy=Gate::unavailable, reader_isolation=Gate::unavailable, format=Gate::unavailable;
    bool open=false, pause_resume=false, seek=false, rate=false;
    std::optional<std::uint64_t> video_duration_100ns;
};
class CapabilitySource {
public:
    virtual ~CapabilitySource()=default;
    // Independent trusted assessment, never inferred from manifest, suffix or digest.
    virtual std::optional<Capabilities> inspect(const Selection&) const=0;
};
enum class Action { open, pause, resume, seek, rate };
struct Intent {
    Action action=Action::open;
    std::optional<std::uint64_t> position_100ns;
    std::optional<unsigned> rate_permille;
};
inline bool valid_intent(const Intent& i) noexcept {
    switch(i.action){
    case Action::open:case Action::pause:case Action::resume:return !i.position_100ns && !i.rate_permille;
    case Action::seek:return i.position_100ns && !i.rate_permille;
    case Action::rate:return !i.position_100ns && i.rate_permille &&
        (*i.rate_permille==500 || *i.rate_permille==1000 || *i.rate_permille==1500 || *i.rate_permille==2000);
    }
    return false;
}
enum class State { selected, planned, invalid_request, unsupported_local, missing, stale_selection, unavailable, unsupported, review_required };
struct SelectionResult { State state; std::optional<Selection> selection; };
// A copied UI intent, NOT permission to execute, a player handle, or live state.
struct Plan { Selection selection; Intent intent; std::optional<std::uint64_t> video_duration_100ns; };
struct Result { State state; std::optional<Plan> plan; };
// No execution interface, extraction, timer, queue, owned resource or mutation.
// Caller serializes on a quiescent I/O owner, not the game/render thread.
class Controller {
    const Repository& repository_;
    const Source* source_;
    const CapabilitySource* capabilities_;
    static bool known_gate(Gate g) noexcept {
        return g==Gate::unavailable || g==Gate::unsupported || g==Gate::review || g==Gate::established;
    }
    static State capability_state(const Capabilities& c) noexcept {
        for(auto g:{c.member_policy,c.reader_isolation,c.format})if(!known_gate(g) || g==Gate::review)return State::review_required;
        for(auto g:{c.member_policy,c.reader_isolation,c.format})if(g==Gate::unsupported)return State::unsupported;
        for(auto g:{c.member_policy,c.reader_isolation,c.format})if(g==Gate::unavailable)return State::unavailable;
        if(!c.open && (c.pause_resume || c.seek || c.rate || c.video_duration_100ns))return State::review_required;
        constexpr std::uint64_t max_duration=(6ull*60*60+1)*10000000;
        if(c.video_duration_100ns && (!*c.video_duration_100ns || *c.video_duration_100ns>max_duration))return State::review_required;
        if(c.bound.member.kind==Kind::native_replay && (c.pause_resume || c.seek || c.rate || c.video_duration_100ns))return State::review_required;
        if(c.seek && !c.video_duration_100ns)return State::review_required;
        return c.open?State::planned:State::unsupported;
    }
public:
    explicit Controller(const Repository& repo,const Source* source=nullptr,const CapabilitySource* capabilities=nullptr):
        repository_(repo),source_(source),capabilities_(capabilities){}
    SelectionResult select(const Verified& expected,std::string_view name,Kind kind) const {
        if(expected.projection.origin==Origin::local)return {State::unsupported_local,{}};
        if(!exchange::valid(expected) || !valid_name(name) || !valid_kind(kind))return {State::invalid_request,{}};
        if(!source_)return {State::unavailable,{}};
        try {
            const auto row=repository_.find(expected.projection.run_id);
            if(!row)return {State::missing,{}};
            if(!run_library::identical(*row,expected.projection))return {State::stale_selection,{}};
            const auto o=source_->observe(expected.projection,name,kind);
            if(!exchange::valid_slot(o.slot) || (o.slot.state!=exchange::SlotState::finalized && o.member))return {State::review_required,{}};
            if(o.slot.state==exchange::SlotState::unavailable)return {State::unavailable,{}};
            if(o.slot.state==exchange::SlotState::absent)return {State::missing,{}};
            if(o.slot.state!=exchange::SlotState::finalized)return {State::review_required,{}};
            if(!exchange::same(*o.slot.archive,expected))return {State::stale_selection,{}};
            if(!o.member)return {State::missing,{}};
            if(!valid_member(*o.member,expected) || o.member->name!=name || o.member->kind!=kind)return {State::review_required,{}};
            return {State::selected,Selection{expected,*o.member}};
        }catch(...){return {State::unavailable,{}};}
    }
    Result plan(const Selection& expected,const Intent& intent) const {
        if(expected.archive.projection.origin==Origin::local)return {State::unsupported_local,{}};
        if(!exchange::valid(expected.archive) || !valid_member(expected.member,expected.archive) || !valid_intent(intent))return {State::invalid_request,{}};
        auto observed=select(expected.archive,expected.member.name,expected.member.kind);
        if(observed.state!=State::selected)return {observed.state,{}};
        if(!same(*observed.selection,expected))return {State::stale_selection,{}};
        if(!capabilities_)return {State::unavailable,{}};
        try {
            const auto cap=capabilities_->inspect(*observed.selection);
            if(!cap)return {State::unavailable,{}};
            if(!same(cap->bound,expected))return {State::stale_selection,{}};
            const auto state=capability_state(*cap);
            if(state!=State::planned)return {state,{}};
            switch(intent.action){
            case Action::open:break;
            case Action::pause:case Action::resume:if(!cap->pause_resume)return {State::unsupported,{}};break;
            case Action::seek:
                if(!cap->seek)return {State::unsupported,{}};
                if(*intent.position_100ns>*cap->video_duration_100ns)return {State::invalid_request,{}};
                break;
            case Action::rate:if(!cap->rate)return {State::unsupported,{}};break;
            }
            return {State::planned,Plan{*observed.selection,intent,cap->video_duration_100ns}};
        }catch(...){return {State::unavailable,{}};}
    }
};
}
