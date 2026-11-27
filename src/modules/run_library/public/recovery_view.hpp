#pragma once
#include "src/modules/run_library/public/history_view.hpp"
#include "src/modules/recorder/public/persistent_output.hpp"

namespace supreme::run_library::presentation {
// Typed projection of the existing archive_status.inspect result, not a new
// archive format or verifier. Source must independently verify bytes and use
// full index projection comparison, never read a claimed result from a manifest.
enum class ArchiveState { absent, pending, finalized, finalized_with_pending_alias, separate_pending_conflict, review_required };
enum class IndexState { missing, present, identical, conflict, unavailable };
struct ArchiveFacts {
    std::string run_id;
    Origin origin;
    ArchiveState archive;
    IndexState index;
    std::optional<std::string> manifest_sha256;
    // Exact nonlocal Record compared by the trusted verifier/controller.
    std::optional<Record> compared_record;
};
class ArchiveSource {
public:
    virtual ~ArchiveSource()=default;
    virtual ArchiveFacts inspect(std::string_view run_id,Origin nonlocal_origin) const=0;
};
enum class ArchiveDisposition { unavailable, unsupported_local, absent, pending_publication, finalized_unindexed,
    matching_nonlocal_index, pending_alias_review, orphan_index, review_required };
struct ArchiveView {
    ArchiveDisposition disposition=ArchiveDisposition::unavailable;
    std::optional<ArchiveFacts> facts;
    std::optional<Record> indexed_record;
};
inline bool valid_facts(const ArchiveFacts& f,std::string_view id,Origin origin) {
    if(f.run_id!=id || f.origin!=origin || (origin!=Origin::imported && origin!=Origin::synthetic) ||
       static_cast<unsigned>(f.archive)>static_cast<unsigned>(ArchiveState::review_required) ||
       static_cast<unsigned>(f.index)>static_cast<unsigned>(IndexState::unavailable) ||
       (f.manifest_sha256 && !hex_id(*f.manifest_sha256,64)))return false;
    if(f.archive==ArchiveState::absent && f.manifest_sha256)return false;
    if(f.archive!=ArchiveState::absent && f.archive!=ArchiveState::review_required && !f.manifest_sha256)return false;
    if(f.manifest_sha256 && f.index==IndexState::present)return false;
    if(!f.manifest_sha256 && (f.index==IndexState::identical || f.index==IndexState::conflict))return false;
    if(f.index==IndexState::identical){
        if(!f.compared_record || !f.manifest_sha256)return false;
        try {validate(*f.compared_record);}catch(const std::exception&){return false;}
        if(f.compared_record->run_id!=id || f.compared_record->origin!=origin || f.compared_record->manifest_sha256!=*f.manifest_sha256)return false;
    }else if(f.compared_record)return false;
    return true;
}
namespace staging=recorder::staging;
enum class MediaReadState { available, unavailable, not_quiescent };
struct MediaRow { std::string token; staging::Inspection inspection; };
struct MediaPage { MediaReadState state=MediaReadState::unavailable; std::vector<MediaRow> rows; };
class Recovery {
    const QueryRepository& repository_;
    const ArchiveSource* archives_;
    const staging::Store* media_;
public:
    Recovery(const QueryRepository& repository,const ArchiveSource* archives=nullptr,const staging::Store* media=nullptr)
        :repository_(repository),archives_(archives),media_(media){}
    ArchiveView archive(std::string_view id,Origin origin) const {
        validate_id(id);if(!valid_origin(origin))throw std::invalid_argument("archive origin");
        if(origin==Origin::local)return {ArchiveDisposition::unsupported_local,{},{}};
        ArchiveView out;if(!archives_)return out;
        try {
            auto f=archives_->inspect(id,origin);
            if(!valid_facts(f,id,origin)){out.disposition=ArchiveDisposition::review_required;return out;}
            out.facts=std::move(f);
        }catch(const std::exception&){return out;}
        const auto& f=*out.facts;
        if(f.index==IndexState::unavailable){
            if(f.archive==ArchiveState::review_required || f.archive==ArchiveState::separate_pending_conflict)
                out.disposition=ArchiveDisposition::review_required;
            return out;
        }
        std::optional<Record> record;
        try {record=repository_.find(id);}catch(const std::exception&){return out;}
        out.disposition=ArchiveDisposition::review_required;
        if(record){
            try {validate(*record);}catch(const std::exception&){return out;}
            if(record->run_id!=id)return out;
            out.indexed_record=std::move(record);
        }
        // Recheck presence, origin and the entire compared projection against
        // the current derived index. This is not a cross-store transaction.
        const auto& r=out.indexed_record;
        if((f.index==IndexState::missing && r) || (f.index!=IndexState::missing && !r) ||
           (r && r->origin!=origin) || (f.index==IndexState::identical && !identical(*r,*f.compared_record)))return out;
        if(f.index==IndexState::conflict)return out;
        switch(f.archive){
        case ArchiveState::absent:
            if(f.index==IndexState::missing)out.disposition=ArchiveDisposition::absent;
            else if(f.index==IndexState::present)out.disposition=ArchiveDisposition::orphan_index;
            break;
        case ArchiveState::pending:
            if(f.index==IndexState::missing)out.disposition=ArchiveDisposition::pending_publication;
            break;
        case ArchiveState::finalized:
            if(f.index==IndexState::missing)out.disposition=ArchiveDisposition::finalized_unindexed;
            else if(f.index==IndexState::identical)out.disposition=ArchiveDisposition::matching_nonlocal_index;
            break;
        case ArchiveState::finalized_with_pending_alias:
            if(f.index==IndexState::missing || f.index==IndexState::identical)out.disposition=ArchiveDisposition::pending_alias_review;
            break;
        default:break;
        }
        return out;
    }
    // Explicit tokens, never guessed from run IDs. Caller attests quiescence;
    // no automatic scan, repair, resumption, mutation or admission.
    MediaPage media(const std::vector<std::string>& tokens,bool quiescent) const {
        if(tokens.size()>64)throw std::invalid_argument("media query bound");
        std::set<std::string> unique;
        for(const auto& token:tokens)if(!staging::valid_token(token) || !unique.insert(token).second)
            throw std::invalid_argument("media query identity");
        if(!quiescent)return {MediaReadState::not_quiescent,{}};
        if(!media_)return {};
        MediaPage out{MediaReadState::available,{}};out.rows.reserve(tokens.size());
        for(const auto& token:tokens)out.rows.push_back({token,staging::inspect(*media_,token)});
        return out;
    }
};
}
