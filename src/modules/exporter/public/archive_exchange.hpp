#pragma once
#include "src/modules/run_library/public/record_validation.hpp"
#include <utility>

namespace supreme::exporter::exchange {
using run_library::Record;
using run_library::Origin;
using run_library::Repository;
using run_library::identical;
using run_library::hex_id;
// Existing evidence_archive.py physical ZIP bound, not a new format or quota.
inline constexpr std::uint64_t archive_limit=1024ull*1024*1024+64*1024+1024*1024;
inline constexpr char unclassified_profile[]="08b64d85d27f15b13d4baf59bd2601cbac76748d6f5109d660e7188a6564d35e";
struct Verified {
    Record projection; // exact existing checked()/project() output, not manifest origin
    std::string zip_sha256;
    std::uint64_t bytes;
};
inline bool nonlocal(Origin o) noexcept{return o==Origin::imported || o==Origin::synthetic;}
inline bool valid(const Verified& a){
    try {run_library::validate(a.projection);}catch(const std::invalid_argument&){return false;}
    return nonlocal(a.projection.origin) && a.projection.category=="unclassified" &&
        a.projection.profile==unclassified_profile && hex_id(a.zip_sha256,64) && a.bytes>=22 && a.bytes<=archive_limit;
}
inline bool same(const Verified& a,const Verified& b) noexcept {
    return identical(a.projection,b.projection) && a.zip_sha256==b.zip_sha256 && a.bytes==b.bytes;
}
// Selection tokens refer to explicitly chosen files/destinations in an adapter's
// private, expiring selection table. Never interpreted as file paths or run IDs.
using Selection=std::string;
enum class SlotState { absent, pending, finalized, alias, review, unavailable };
struct Slot { SlotState state=SlotState::unavailable; std::optional<Verified> archive; };
enum class Operation { completed, conflict, review, unavailable };
// Trusted owner-thread adapter. All reads independently verify existing bytes and
// full projection; no raw manifest/JSON claims. Guard expected source, exact slot
// observation, selected destination and same bound repository BEFORE effects.
// Only existing checked/project, exclusive copy/publication and nonlocal admission
// operations; no extraction, replacement, annotation writes, or network.
class Port {
public:
    virtual ~Port()=default;
    virtual Slot selected(const Selection&) const=0; // ordinary import projection ONLY
    virtual Slot managed(const Record&) const=0;
    virtual Slot destination(const Selection&,Origin) const=0;
    virtual Operation admit_import(const Selection&,const Verified&,const Slot& expected_managed)=0;
    virtual Operation publish_exact(const Selection&,const Verified&,const Slot& expected_destination)=0;
};
enum class ResultCode { invalid_request, unsupported_local, unavailable, missing, conflict, review,
    stale_source, unchanged, completed, operation_uncertain };
struct Result {
    ResultCode code;
    bool attempted=false;
    std::optional<Operation> acknowledgement;
    Slot observed; // post-attempt target observation; no fabricated success
};
inline bool valid_slot(const Slot& s){
    switch(s.state){
    case SlotState::absent:case SlotState::review:case SlotState::unavailable:return !s.archive;
    case SlotState::pending:case SlotState::finalized:case SlotState::alias:return s.archive && valid(*s.archive);
    }
    return false;
}
// Serialized application operations on the same I/O owner as the bound repository.
// One explicit archive per call; no queued jobs, retries, filesystem scanning or UI I/O.
class Controller {
    const Repository& repository_;
    Port* port_;
    template<class F> static Slot read(F f){try {auto s=f();return valid_slot(s)?s:Slot{SlotState::review,{}};}catch(...){return {};}}
    enum class RowState { missing, identical, conflict, unavailable };
    RowState row(const Record& expected) const {
        try {
            const auto r=repository_.find(expected.run_id);
            if(!r)return RowState::missing;
            try {run_library::validate(*r);}catch(const std::invalid_argument&){return RowState::conflict;}
            return identical(*r,expected)?RowState::identical:RowState::conflict;
        }catch(...){return RowState::unavailable;}
    }
    static Result stop(ResultCode code,Slot observed={}){return {code,false,{},std::move(observed)};}
    static ResultCode source_error(const Slot& s,const Verified& expected){
        if(s.state==SlotState::unavailable)return ResultCode::unavailable;
        if(s.state==SlotState::absent)return ResultCode::missing;
        if(s.state!=SlotState::finalized)return ResultCode::review;
        return same(*s.archive,expected)?ResultCode::completed:ResultCode::stale_source;
    }
    static ResultCode target_error(const Slot& s,const Verified& expected){
        if(s.state==SlotState::unavailable)return ResultCode::unavailable;
        if(s.state==SlotState::review)return ResultCode::review;
        if(s.archive && !same(*s.archive,expected))return ResultCode::conflict;
        return ResultCode::completed;
    }
    template<class F> static std::optional<Operation> mutate(F f){try{return f();}catch(...){return {};}}
    static bool final_matches(const Slot& s,const Verified& a){return s.state==SlotState::finalized && same(*s.archive,a);}
public:
    explicit Controller(const Repository& repository,Port* port=nullptr):repository_(repository),port_(port){}
    Slot preview_import(const Selection& source) const {
        if(!hex_id(source,32))throw std::invalid_argument("source selection");
        if(!port_)return {};
        auto s=read([&]{return port_->selected(source);});
        if(s.archive && s.archive->projection.origin!=Origin::imported)return {SlotState::review,{}};
        if(s.state==SlotState::pending || s.state==SlotState::alias)return {SlotState::review,{}};
        return s;
    }
    Slot preview_export(const Record& selected) const {
        run_library::validate(selected);
        if(!nonlocal(selected.origin))return {SlotState::review,{}};
        if(!port_)return {};
        const auto r=row(selected);
        if(r==RowState::unavailable)return {};
        if(r!=RowState::identical)return {SlotState::review,{}};
        auto s=read([&]{return port_->managed(selected);});
        if(s.archive && !identical(s.archive->projection,selected))return {SlotState::review,{}};
        return s;
    }
    Result import_selected(const Selection& source,const Verified& expected){
        if(expected.projection.origin==Origin::local)return stop(ResultCode::unsupported_local);
        if(!hex_id(source,32) || !valid(expected) || expected.projection.origin!=Origin::imported)return stop(ResultCode::invalid_request);
        if(!port_)return stop(ResultCode::unavailable);
        const auto input=preview_import(source);
        if(const auto c=source_error(input,expected);c!=ResultCode::completed)return stop(c,input);
        const auto& record=expected.projection;
        const auto existing=row(record);
        if(existing==RowState::unavailable)return stop(ResultCode::unavailable);
        if(existing==RowState::conflict)return stop(ResultCode::conflict);
        const auto target=read([&]{return port_->managed(record);});
        if(const auto c=target_error(target,expected);c!=ResultCode::completed)return stop(c,target);
        if(existing==RowState::identical){
            if(target.state==SlotState::finalized)return stop(ResultCode::unchanged,target);
            if(target.state!=SlotState::alias)return stop(ResultCode::review,target);
        }
        const auto ack=mutate([&]{return port_->admit_import(source,expected,target);});
        const auto after=read([&]{return port_->managed(record);});
        const auto indexed=row(record);
        const bool done=ack==Operation::completed && final_matches(after,expected) && indexed==RowState::identical;
        return {done?ResultCode::completed:ResultCode::operation_uncertain,true,ack,after};
    }
    Result export_selected(const Record& selected,const Verified& expected,const Selection& target){
        if(selected.origin==Origin::local || expected.projection.origin==Origin::local)return stop(ResultCode::unsupported_local);
        if(!hex_id(target,32) || !valid(expected) || !identical(selected,expected.projection))return stop(ResultCode::invalid_request);
        if(!port_)return stop(ResultCode::unavailable);
        const auto existing=row(selected);
        if(existing==RowState::missing)return stop(ResultCode::missing);
        if(existing==RowState::conflict)return stop(ResultCode::conflict);
        if(existing==RowState::unavailable)return stop(ResultCode::unavailable);
        const auto source=read([&]{return port_->managed(selected);});
        if(const auto c=source_error(source,expected);c!=ResultCode::completed)return stop(c,source);
        const auto dest=read([&]{return port_->destination(target,selected.origin);});
        if(const auto c=target_error(dest,expected);c!=ResultCode::completed)return stop(c,dest);
        if(dest.state==SlotState::alias)return stop(ResultCode::review,dest); // no export alias cleanup here
        if(dest.state==SlotState::finalized)return stop(ResultCode::unchanged,dest);
        const auto ack=mutate([&]{return port_->publish_exact(target,expected,dest);});
        const auto after=read([&]{return port_->destination(target,selected.origin);});
        const auto source_after=read([&]{return port_->managed(selected);});
        const auto indexed=row(selected);
        const bool done=ack==Operation::completed && final_matches(after,expected) && final_matches(source_after,expected) && indexed==RowState::identical;
        return {done?ResultCode::completed:ResultCode::operation_uncertain,true,ack,after};
    }
};
}
