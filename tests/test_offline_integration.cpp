#include "src/modules/exporter/public/archive_exchange.hpp"
#include "src/modules/run_library/public/annotation_controller.hpp"
#include <iostream>

namespace ex=supreme::exporter::exchange;
namespace lib=supreme::run_library;
namespace view=lib::presentation;
namespace edit=lib::management;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)

// One shared fake repository and an explicitly injected archive port. No actual
// ZIP, filesystem mutation, native DB, trusted observation or production adapter.
struct Repo final:lib::QueryRepository {
    std::optional<lib::Record> record;
    std::optional<lib::Annotation> label;
    unsigned inserts=0,edits=0;
    bool fail_read=false,fail_label=false;
    lib::Insert insert(const lib::Record& r) override {
        ++inserts;
        if(record)return lib::identical(*record,r)?lib::Insert::identical:lib::Insert::conflict;
        record.emplace(r);return lib::Insert::inserted;
    }
    std::optional<lib::Record> find(std::string_view id) const override {
        if(fail_read)throw std::runtime_error("index unavailable");
        return record && record->run_id==id?record:std::nullopt;
    }
    std::vector<lib::Record> recent(std::size_t limit) const override {
        if(fail_read)throw std::runtime_error("index unavailable");
        return record && limit?std::vector<lib::Record>{*record}:std::vector<lib::Record>{};
    }
    void annotate(std::string_view id,const lib::Annotation& a) override {
        if(!find(id))throw std::logic_error("unknown record");
        ++edits;label.emplace(a);
    }
    std::optional<lib::Annotation> annotation(std::string_view id) const override {
        if(fail_label)throw std::runtime_error("label unavailable");
        return find(id)?label:std::nullopt;
    }
    std::optional<lib::Record> best_local(const lib::Comparison&) const override {
        throw std::logic_error("unexpected PB query");
    }
};
struct Port final:ex::Port {
    Repo& repo;
    ex::Slot input,stored,dest;
    unsigned imports=0,exports=0;
    bool lose_ack=false,offline=false;
    explicit Port(Repo& r):repo(r){stored.state=ex::SlotState::absent;dest.state=ex::SlotState::absent;}
    ex::Slot selected(const ex::Selection&) const override {if(offline)throw std::runtime_error("archive unavailable");return input;}
    ex::Slot managed(const lib::Record&) const override {if(offline)throw std::runtime_error("archive unavailable");return stored;}
    ex::Slot destination(const ex::Selection&,lib::Origin) const override {return dest;}
    ex::Operation admit_import(const ex::Selection&,const ex::Verified& a,const ex::Slot& expected) override {
        ++imports;
        if(expected.state!=stored.state || expected.archive.has_value()!=stored.archive.has_value() ||
           (expected.archive && !ex::same(*expected.archive,*stored.archive)))return ex::Operation::conflict;
        stored.state=ex::SlotState::finalized;stored.archive.emplace(a);
        if(repo.insert(a.projection)==lib::Insert::conflict)return ex::Operation::conflict;
        if(lose_ack)throw std::runtime_error("lost acknowledgement");
        return ex::Operation::completed;
    }
    ex::Operation publish_exact(const ex::Selection&,const ex::Verified& a,const ex::Slot& expected) override {
        ++exports;
        if(expected.state!=ex::SlotState::absent || dest.state!=ex::SlotState::absent ||
           !stored.archive || !ex::same(*stored.archive,a))return ex::Operation::conflict;
        dest.state=ex::SlotState::finalized;dest.archive.emplace(a);return ex::Operation::completed;
    }
};
struct Fixture {
    const ex::Verified original{{std::string(32,'a'),"unclassified",ex::unclassified_profile,"events-v1",std::string(64,'b'),lib::Origin::imported,lib::Status::finished,0,{}},std::string(64,'c'),1024};
    const ex::Selection input=std::string(32,'d'),output=std::string(32,'e');
    Repo repo;
    Port port;
    view::History history;
    edit::AnnotationController labels;
    ex::Controller exchange;
    Fixture():port(repo),history(repo),labels(repo),exchange(repo,&port){port.input.state=ex::SlotState::finalized;port.input.archive.emplace(original);}
};
bool lost_ack_then_explicit_retry(){
    Fixture f;f.port.lose_ack=true;
    const auto imported=f.exchange.import_selected(f.input,f.original);
    CHECK(imported.code==ex::ResultCode::operation_uncertain && imported.attempted && !imported.acknowledgement);
    const auto visible=f.history.find(f.original.projection.run_id);
    CHECK(visible.state==view::ReadState::available && visible.row && lib::identical(visible.row->indexed,f.original.projection));
    CHECK(visible.row->indexed.igt_ms==0 && !visible.row->indexed.rta_ns && !lib::comparable(visible.row->indexed,f.original.projection,lib::Method::igt));
    CHECK(f.port.imports==1 && f.repo.inserts==1 && f.port.exports==0);
    CHECK(f.exchange.import_selected(f.input,f.original).code==ex::ResultCode::unchanged);
    CHECK(f.port.imports==1 && f.repo.inserts==1);
    return true;
}
bool annotation_preserves_exact_exchange_identity(){
    Fixture f;CHECK(f.exchange.import_selected(f.input,f.original).code==ex::ResultCode::completed);
    const auto old=f.history.find(f.original.projection.run_id);CHECK(old.row);
    CHECK(f.labels.rename(*old.row,"Practice clip").status==edit::EditStatus::updated);
    const auto current=f.history.find(f.original.projection.run_id);CHECK(current.row && current.row->annotation && current.row->annotation->name=="Practice clip");
    CHECK(f.labels.set_favorite(*old.row,true).status==edit::EditStatus::stale_annotation);
    CHECK(f.labels.set_favorite(*current.row,true).status==edit::EditStatus::updated);
    view::HistoryQuery q;q.favorites_only=true;
    const auto page=f.history.recent(q);CHECK(page.rows.size()==1 && lib::identical(page.rows[0].indexed,f.original.projection));
    CHECK(f.exchange.export_selected(page.rows[0].indexed,f.original,f.output).code==ex::ResultCode::completed);
    CHECK(f.port.dest.archive && ex::same(*f.port.dest.archive,f.original));
    CHECK(f.repo.edits==2 && f.repo.inserts==1 && f.port.exports==1);
    // Same manifest and index, different whole-container identity must not reuse
    // a previously approved export selection, even after a successful rename.
    f.port.stored.archive->zip_sha256=std::string(64,'f');
    CHECK(f.history.find(f.original.projection.run_id).state==view::ReadState::available);
    CHECK(f.exchange.export_selected(page.rows[0].indexed,f.original,f.output).code==ex::ResultCode::stale_source);
    CHECK(f.port.exports==1 && ex::same(*f.port.dest.archive,f.original));
    return true;
}
bool independent_failure_domains(){
    Fixture f;CHECK(f.exchange.import_selected(f.input,f.original).code==ex::ResultCode::completed);
    f.port.offline=true;
    const auto selected=f.history.find(f.original.projection.run_id);CHECK(selected.row);
    CHECK(f.labels.rename(*selected.row,"Still editable").status==edit::EditStatus::updated);
    CHECK(f.exchange.export_selected(selected.row->indexed,f.original,f.output).code==ex::ResultCode::unavailable);
    f.port.offline=false;f.repo.fail_label=true;
    const auto no_label=f.history.find(f.original.projection.run_id);
    CHECK(no_label.state==view::ReadState::available && no_label.row && no_label.row->annotation_state==view::ReadState::unavailable);
    CHECK(f.labels.rename(*no_label.row,"No overwrite").status==edit::EditStatus::selection_unavailable);
    CHECK(f.exchange.export_selected(no_label.row->indexed,f.original,f.output).code==ex::ResultCode::completed);
    f.repo.fail_read=true;
    CHECK(f.history.find(f.original.projection.run_id).state==view::ReadState::unavailable);
    CHECK(f.labels.rename(*selected.row,"No write").status==edit::EditStatus::unavailable);
    CHECK(f.exchange.export_selected(selected.row->indexed,f.original,f.output).code==ex::ResultCode::unavailable);
    CHECK(f.repo.edits==1 && f.port.exports==1);
    return true;
}
int main(){
    for(auto test:{lost_ack_then_explicit_retry,annotation_preserves_exact_exchange_identity,independent_failure_domains})if(!test())return 1;
    std::cout<<"B-INT-01: 3 offline cross-module scenario groups PASS\n";return 0;
}
