#pragma once
#include "src/modules/run_library/public/history_view.hpp"

namespace supreme::run_library::management {
using presentation::Detail;
using presentation::ReadState;
using presentation::Row;

enum class EditStatus {
    invalid_request, selection_unavailable, missing, unavailable, invalid_data,
    evidence_conflict, stale_annotation, unchanged, updated, write_uncertain, readback_uncertain
};
enum class WriteState { not_attempted, returned, threw };
struct EditResult {
    EditStatus status;
    WriteState write=WriteState::not_attempted;
    // Validated preflight or post-attempt observation, never a fabricated success.
    std::optional<Detail> observed;
};

// Explicit snapshot-bound commands, not UI row positions. Use on the repository
// owner thread, serialized with ALL other writers. No cross-process CAS, queue,
// native callbacks, archive mutation, admission, or PB operations.
class AnnotationController {
    QueryRepository& repository_;
    const presentation::History history_;

    static bool same_annotation(const std::optional<Annotation>& a,const std::optional<Annotation>& b) noexcept {
        if(a.has_value()!=b.has_value())return false;
        return !a || (a->name==b->name && a->favorite==b->favorite);
    }
    static std::optional<EditStatus> selection_error(const Row& selected){
        try {
            validate(selected.indexed);
            if(selected.annotation_state==ReadState::unavailable || selected.annotation_state==ReadState::invalid_data)
                return EditStatus::selection_unavailable;
            if(selected.annotation_state!=(selected.annotation?ReadState::available:ReadState::absent))
                return EditStatus::invalid_request;
            if(selected.annotation)validate(*selected.annotation);
        }catch(const std::invalid_argument&){return EditStatus::invalid_request;}
        return {};
    }
    static std::optional<EditStatus> precondition(const Detail& current,const Record& expected){
        switch(current.state){
        case ReadState::absent:return EditStatus::missing;
        case ReadState::unavailable:return EditStatus::unavailable;
        case ReadState::invalid_data:return EditStatus::invalid_data;
        case ReadState::available:break;
        }
        if(!current.row)return EditStatus::invalid_data;
        if(!identical(current.row->indexed,expected))return EditStatus::evidence_conflict;
        if(current.row->annotation_state==ReadState::unavailable)return EditStatus::unavailable;
        if(current.row->annotation_state==ReadState::invalid_data)return EditStatus::invalid_data;
        return {};
    }
    Detail read(std::string_view id) const {
        try {return history_.find(id);}catch(...){return {};}
    }
    EditResult execute(const Row& selected,const std::optional<Annotation>& desired){
        auto current=read(selected.indexed.run_id);
        if(auto failure=precondition(current,selected.indexed))return {*failure,WriteState::not_attempted,std::move(current)};
        // Exact desired value already present: idempotent, even after lost acknowledgement.
        if(same_annotation(current.row->annotation,desired))return {EditStatus::unchanged,WriteState::not_attempted,std::move(current)};
        if(!same_annotation(current.row->annotation,selected.annotation))return {EditStatus::stale_annotation,WriteState::not_attempted,std::move(current)};
        // An absent desired label is only the absent/unfavorite no-op; never delete a label.
        if(!desired)return {EditStatus::invalid_request,WriteState::not_attempted,std::move(current)};
        auto write=WriteState::returned;
        try {repository_.annotate(selected.indexed.run_id,*desired);}catch(...){write=WriteState::threw;}
        auto after=read(selected.indexed.run_id);
        if(write==WriteState::threw)return {EditStatus::write_uncertain,write,std::move(after)};
        if(precondition(after,selected.indexed) || !same_annotation(after.row->annotation,desired))
            return {EditStatus::readback_uncertain,write,std::move(after)};
        return {EditStatus::updated,write,std::move(after)};
    }
public:
    explicit AnnotationController(QueryRepository& repository):repository_(repository),history_(repository){}
    EditResult rename(const Row& selected,std::string_view name){
        if(auto failure=selection_error(selected))return {*failure,WriteState::not_attempted,{}};
        if(name.empty() || name.size()>256)return {EditStatus::invalid_request,WriteState::not_attempted,{}};
        Annotation desired{std::string(name),selected.annotation && selected.annotation->favorite};
        try {validate(desired);}catch(const std::invalid_argument&){return {EditStatus::invalid_request,WriteState::not_attempted,{}};}
        return execute(selected,desired);
    }
    EditResult set_favorite(const Row& selected,bool favorite){
        if(auto failure=selection_error(selected))return {*failure,WriteState::not_attempted,{}};
        auto desired=selected.annotation;
        if(desired)desired->favorite=favorite;
        else if(favorite)desired=Annotation{selected.indexed.run_id,true}; // schema requires a name
        return execute(selected,desired);
    }
};
}
