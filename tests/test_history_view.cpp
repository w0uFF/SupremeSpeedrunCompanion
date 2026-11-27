#include "src/modules/run_library/public/recovery_view.hpp"
#include <iostream>
#include <map>
using namespace supreme::run_library;
using namespace supreme::run_library::presentation;
using supreme::recorder::OutputIdentity;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
template<class F> bool rejects(F f){try{f();return false;}catch(const std::invalid_argument&){return true;}}
std::string id(char c){return std::string(32,c);}
Record record(char c,Origin o=Origin::imported){return {id(c),"category",std::string(64,'a'),"events",std::string(64,'b'),o,Status::finished,100,200};}
struct Repo final:QueryRepository {
    std::vector<Record> data;std::map<std::string,Annotation> labels;
    bool fail=false,oversize=false,wrong=false,fail_labels=false;
    mutable unsigned queries=0,annotation_calls=0,best_calls=0;unsigned writes=0;
    Insert insert(const Record&) override {++writes;throw std::logic_error("read only");}
    void annotate(std::string_view,const Annotation&) override {++writes;throw std::logic_error("read only");}
    std::optional<Record> find(std::string_view key)const override {
        ++queries;if(fail)throw std::runtime_error("index");if(wrong)return record('f');
        for(const auto& r:data)if(r.run_id==key)return r;
        return {};
    }
    std::vector<Record> recent(std::size_t limit)const override {
        ++queries;if(fail)throw std::runtime_error("index");
        if(oversize)return data;
        return {data.begin(),data.begin()+static_cast<std::ptrdiff_t>(std::min(limit,data.size()))};
    }
    std::optional<Annotation> annotation(std::string_view key)const override {
        ++annotation_calls;if(fail_labels)throw std::runtime_error("labels");
        const auto it=labels.find(std::string(key));return it==labels.end()?std::nullopt:std::optional<Annotation>(it->second);
    }
    std::optional<Record> best_local(const Comparison&)const override {++best_calls;return record('a',Origin::local);}
};
struct Archives final:ArchiveSource {
    ArchiveFacts facts{id('a'),Origin::imported,ArchiveState::absent,IndexState::missing,{},{}};
    bool fail=false;mutable unsigned calls=0;
    ArchiveFacts inspect(std::string_view,Origin)const override {++calls;if(fail)throw std::runtime_error("verifier");return facts;}
};
struct MediaStore final:staging::Store {
    struct Slot {std::optional<std::string> intent,incomplete,finalized;std::optional<std::uint64_t> size;};
    std::map<std::string,Slot> slots;mutable unsigned reads=0;unsigned writes=0;
    void reserve_slot(const OutputIdentity&)override {++writes;throw std::logic_error("read only");}
    void append(const std::string&,staging::Marker,const std::string&)override {++writes;throw std::logic_error("read only");}
    void flush_media(const std::string&)override {++writes;throw std::logic_error("read only");}
    bool exists(const std::string& t)const override {++reads;if(t==id('f'))throw std::runtime_error("media store");return slots.count(t)!=0;}
    std::optional<std::string> read(const std::string& t,staging::Marker m)const override {
        ++reads;const auto& s=slots.at(t);return m==staging::Marker::intent?s.intent:m==staging::Marker::incomplete?s.incomplete:s.finalized;
    }
    std::optional<std::uint64_t> media_size(const std::string& t)const override {++reads;return slots.at(t).size;}
    staging::fs::path destination(const std::string&)const override {throw std::logic_error("no path access");}
};
bool recent_bounds(){
    Repo repo;History h(repo);CHECK(h.recent().state==ReadState::available && h.recent().rows.empty());
    repo.data={record('a',Origin::local),record('b'),record('c',Origin::synthetic),record('d')};
    repo.labels[id('b')]={"Favorite",true};repo.labels[id('d')]={"Other",false};
    HistoryQuery q;q.scan_limit=3;q.row_limit=2;
    auto page=h.recent(q);CHECK(page.state==ReadState::available && page.scanned==3 && page.scan_limit_reached && page.row_limit_reached && page.rows.size()==2);
    CHECK(page.rows[0].indexed.run_id==id('a') && page.rows[1].annotation && page.rows[1].annotation->favorite);
    q.origin=Origin::imported;q.favorites_only=true;page=h.recent(q);CHECK(page.rows.size()==1 && page.rows[0].indexed.run_id==id('b'));
    q.category="other";CHECK(h.recent(q).rows.empty());q.category.reset();q.status=Status::failed;CHECK(h.recent(q).rows.empty());
    const auto calls=repo.queries;q.scan_limit=1001;CHECK(rejects([&]{h.recent(q);}));q.scan_limit=200;q.row_limit=101;CHECK(rejects([&]{h.recent(q);}));q.row_limit=0;CHECK(rejects([&]{h.recent(q);}));
    CHECK(repo.queries==calls && repo.best_calls==0 && repo.writes==0);return true;
}
bool ordering_and_annotations(){
    Repo repo;History h(repo);auto a=record('a'),b=record('b'),c=record('c'),d=record('d'),e=record('e'),f=record('f');
    a.igt_ms=0;b.igt_ms.reset();b.rta_ns=0;c.igt_ms=5;d.igt_ms=5;d.rta_ns=1;e.profile=std::string(64,'c');f.event_semantics="other";
    repo.data={b,d,e,c,f,a};repo.labels[id('a')]={"Name",true};repo.labels[id('b')]={"bad\n",true};
    HistoryQuery q;q.time_order=Comparison{"category",a.profile,"events",Method::igt};
    auto p=h.recent(q);CHECK(p.rows.size()==4 && p.rows[0].indexed.run_id==id('a') && p.rows[1].indexed.run_id==id('c') && p.rows[2].indexed.run_id==id('d') && p.rows[3].indexed.run_id==id('b'));
    CHECK(p.rows[0].indexed.igt_ms==0 && !p.rows[3].indexed.igt_ms && p.annotation_errors==1 && !p.rows[3].annotation && p.rows[3].annotation_state==ReadState::invalid_data);
    q.time_order->method=Method::rta;p=h.recent(q);CHECK(p.rows[0].indexed.run_id==id('b') && p.rows[1].indexed.run_id==id('d'));
    auto detail=h.find(id('a'));CHECK(detail.state==ReadState::available && detail.row->annotation->name=="Name");
    repo.labels[id('a')].name="Changed";CHECK(detail.row->annotation->name=="Name" && identical(detail.row->indexed,repo.data.back()));
    repo.fail_labels=true;detail=h.find(id('a'));CHECK(detail.row && detail.row->annotation_state==ReadState::unavailable && !detail.row->annotation);
    q.favorites_only=true;p=h.recent(q);CHECK(p.state==ReadState::available && p.rows.empty() && p.annotation_errors==4);
    CHECK(repo.best_calls==0 && repo.writes==0);return true;
}
bool invalid_and_unavailable(){
    Repo repo;History h(repo);CHECK(h.find(id('a')).state==ReadState::absent);repo.fail=true;
    CHECK(h.find(id('a')).state==ReadState::unavailable && h.recent().state==ReadState::unavailable);repo.fail=false;
    repo.wrong=true;CHECK(h.find(id('a')).state==ReadState::invalid_data);repo.wrong=false;
    auto r=record('a');r.manifest_sha256="bad";repo.data={r};CHECK(h.find(id('a')).state==ReadState::invalid_data && h.recent().state==ReadState::invalid_data);
    repo.data={record('a'),record('a')};CHECK(h.recent().state==ReadState::invalid_data);
    repo.data={record('a'),record('b')};repo.oversize=true;HistoryQuery q;q.scan_limit=q.row_limit=1;CHECK(h.recent(q).state==ReadState::invalid_data);
    auto calls=repo.queries;CHECK(rejects([&]{h.find("../bad");}));q.origin=static_cast<Origin>(99);CHECK(rejects([&]{h.recent(q);}));CHECK(calls==repo.queries && !repo.writes && !repo.best_calls);return true;
}
bool archive_states(){
    Repo repo;Archives source;Recovery rec(repo,&source);const auto token=id('a');
    const auto get=[&]{return rec.archive(token,Origin::imported);};
    CHECK(get().disposition==ArchiveDisposition::absent);
    auto& f=source.facts;f.archive=ArchiveState::pending;f.manifest_sha256=std::string(64,'b');CHECK(get().disposition==ArchiveDisposition::pending_publication);
    f.archive=ArchiveState::finalized;CHECK(get().disposition==ArchiveDisposition::finalized_unindexed && !get().indexed_record);
    f.index=IndexState::unavailable;CHECK(get().disposition==ArchiveDisposition::unavailable);f.index=IndexState::missing;
    repo.data={record('a')};CHECK(get().disposition==ArchiveDisposition::review_required);
    f.index=IndexState::identical;CHECK(get().disposition==ArchiveDisposition::review_required);f.compared_record=repo.data[0];
    auto matched=get();CHECK(matched.disposition==ArchiveDisposition::matching_nonlocal_index && matched.indexed_record && identical(*matched.indexed_record,repo.data[0]));
    repo.data[0].rta_ns=999;CHECK(get().disposition==ArchiveDisposition::review_required);repo.data[0]=*f.compared_record;
    f.archive=ArchiveState::finalized_with_pending_alias;CHECK(get().disposition==ArchiveDisposition::pending_alias_review);
    f.archive=ArchiveState::pending;CHECK(get().disposition==ArchiveDisposition::review_required);
    f.archive=ArchiveState::separate_pending_conflict;CHECK(get().disposition==ArchiveDisposition::review_required);
    f.archive=ArchiveState::review_required;CHECK(get().disposition==ArchiveDisposition::review_required);
    f.archive=ArchiveState::absent;f.manifest_sha256.reset();f.compared_record.reset();f.index=IndexState::present;CHECK(get().disposition==ArchiveDisposition::orphan_index);
    repo.data.clear();CHECK(get().disposition==ArchiveDisposition::review_required);f.index=IndexState::missing;CHECK(get().disposition==ArchiveDisposition::absent);
    CHECK(!repo.writes && !repo.best_calls);return true;
}
bool provenance_and_failures(){
    Repo repo;Archives source;Recovery rec(repo,&source);const auto token=id('a');auto& f=source.facts;
    repo.data={record('a',Origin::local)};const auto get=[&]{return rec.archive(token,Origin::imported);};
    CHECK(rec.archive(token,Origin::local).disposition==ArchiveDisposition::unsupported_local && source.calls==0 && repo.queries==0);
    f.archive=ArchiveState::finalized;f.manifest_sha256=std::string(64,'b');f.index=IndexState::identical;f.compared_record=record('a');
    CHECK(get().disposition==ArchiveDisposition::review_required);
    f.origin=Origin::local;CHECK(get().disposition==ArchiveDisposition::review_required && !get().facts);f.origin=Origin::imported;
    f.run_id=id('b');CHECK(get().disposition==ArchiveDisposition::review_required && !get().facts);f.run_id=token;
    f.manifest_sha256="bad";CHECK(get().disposition==ArchiveDisposition::review_required);f.manifest_sha256=std::string(64,'b');
    f.compared_record->origin=Origin::local;CHECK(get().disposition==ArchiveDisposition::review_required);f.compared_record=record('a');
    repo.fail=true;CHECK(get().disposition==ArchiveDisposition::unavailable);repo.fail=false;source.fail=true;CHECK(get().disposition==ArchiveDisposition::unavailable);source.fail=false;
    Recovery disabled(repo);CHECK(disabled.archive(token,Origin::imported).disposition==ArchiveDisposition::unavailable);
    f.origin=Origin::synthetic;f.compared_record=record('a',Origin::synthetic);repo.data={*f.compared_record};
    CHECK(rec.archive(token,Origin::synthetic).disposition==ArchiveDisposition::matching_nonlocal_index);
    const auto calls=source.calls;CHECK(rejects([&]{rec.archive("bad",Origin::imported);}));CHECK(rejects([&]{rec.archive(token,static_cast<Origin>(99));}));CHECK(source.calls==calls && !repo.writes && !repo.best_calls);return true;
}
bool media_states(){
    Repo repo;MediaStore store;Recovery rec(repo,nullptr,&store);const auto a=id('a'),b=id('b'),c=id('c'),d=id('d'),e=id('e'),f=id('f');
    store.slots[b]={staging::intent({1,b,100}),{},{},0};
    const auto ci=staging::intent({2,c,100});store.slots[c]={ci,ci+"incomplete\n",{},20};
    const auto di=staging::intent({3,d,100});store.slots[d]={di,{},di+"finalized\n40\n",40};
    store.slots[e]={staging::intent({4,a,100}),{},{},1};
    CHECK(rec.media({a,b},false).state==MediaReadState::not_quiescent && !store.reads);
    Recovery disabled(repo);CHECK(disabled.media({a},true).state==MediaReadState::unavailable);
    CHECK(rejects([&]{rec.media({a,"../bad"},true);}));CHECK(rejects([&]{rec.media({a,a},true);}));CHECK(rejects([&]{rec.media(std::vector<std::string>(65,a),true);}));CHECK(!store.reads);
    auto result=rec.media({a,b,c,d,e,f},true);CHECK(result.state==MediaReadState::available && result.rows.size()==6);
    const auto& rows=result.rows;CHECK(rows[0].inspection.status==staging::Status::absent && rows[1].inspection.status==staging::Status::pending && rows[1].inspection.observed_bytes==0);
    CHECK(rows[2].inspection.status==staging::Status::incomplete && rows[3].inspection.status==staging::Status::finalized_claim && rows[3].inspection.observed_bytes==40);
    CHECK(rows[4].inspection.status==staging::Status::review_required && rows[5].inspection.status==staging::Status::review_required);
    store.slots[d].size=41;CHECK(rec.media({d},true).rows[0].inspection.status==staging::Status::review_required && rows[3].inspection.status==staging::Status::finalized_claim);
    CHECK(!repo.queries && !repo.writes && !repo.best_calls && !store.writes);return true;
}
int main(){for(auto test:{recent_bounds,ordering_and_annotations,invalid_and_unavailable,archive_states,provenance_and_failures,media_states})if(!test())return 1;
    std::cout<<"B-RUN-01: 6 history/recovery scenario groups PASS\n";return 0;}
