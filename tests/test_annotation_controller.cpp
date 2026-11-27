#include "src/modules/run_library/public/annotation_controller.hpp"
#include <iostream>
#include <map>

using namespace supreme::run_library;
using namespace supreme::run_library::presentation;
using namespace supreme::run_library::management;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
std::string id(char c){return std::string(32,c);}
Record record(char c,Origin origin=Origin::imported){return {id(c),"category",std::string(64,'a'),"events",std::string(64,'b'),origin,Status::finished,100,200};}
struct Repo final:QueryRepository {
    enum class Fault { none, before_write, after_write, no_effect, wrong_label, partial_label, changed_record,
        missing_record, corrupt_record, unreadable_record, unreadable_label, corrupt_label };
    std::map<std::string,Record> records;
    std::map<std::string,Annotation> labels;
    Fault fault=Fault::none;
    bool fail_find=false,fail_label=false,wrong_id=false;
    mutable unsigned finds=0,label_reads=0,recent_calls=0,best_calls=0;
    unsigned writes=0,inserts=0;
    std::string last_target;
    Insert insert(const Record&)override{++inserts;throw std::logic_error("no evidence writes");}
    std::vector<Record> recent(std::size_t)const override{++recent_calls;throw std::logic_error("no scans");}
    std::optional<Record> best_local(const Comparison&)const override{++best_calls;throw std::logic_error("no PB");}
    std::optional<Record> find(std::string_view key)const override{
        ++finds;if(fail_find)throw std::runtime_error("read");if(wrong_id)return record('f');
        const auto it=records.find(std::string(key));return it==records.end()?std::nullopt:std::optional<Record>(it->second);
    }
    std::optional<Annotation> annotation(std::string_view key)const override{
        ++label_reads;if(fail_label)throw std::runtime_error("label read");
        const auto it=labels.find(std::string(key));return it==labels.end()?std::nullopt:std::optional<Annotation>(it->second);
    }
    void annotate(std::string_view key,const Annotation& a)override{
        ++writes;last_target=std::string(key);validate(a);
        if(!records.count(last_target))throw std::runtime_error("missing FK");
        if(fault==Fault::before_write)throw std::runtime_error("before commit");
        if(fault==Fault::no_effect)return;
        labels[last_target]=a;
        switch(fault){
        case Fault::after_write:throw std::runtime_error("lost acknowledgement");
        case Fault::wrong_label:labels.at(last_target).name="Unexpected";break;
        case Fault::partial_label:labels.at(last_target).favorite=!a.favorite;throw std::runtime_error("partial write");
        case Fault::changed_record:records.at(last_target).rta_ns=999;break;
        case Fault::missing_record:records.erase(last_target);break;
        case Fault::corrupt_record:records.at(last_target).manifest_sha256="bad";break;
        case Fault::unreadable_record:fail_find=true;break;
        case Fault::unreadable_label:fail_label=true;break;
        case Fault::corrupt_label:labels.at(last_target).name="\n";break;
        default:break;
        }
    }
    bool no_other_calls()const{return !inserts && !recent_calls && !best_calls;}
};
Row select(const Repo& repo,char c='a'){
    const auto detail=History(repo).find(id(c));
    if(detail.state!=ReadState::available || !detail.row)throw std::logic_error("test selection");
    return *detail.row;
}
bool success_and_idempotence(){
    Repo repo;repo.records.emplace(id('a'),record('a'));repo.records.emplace(id('b'),record('b',Origin::synthetic));
    repo.labels[id('b')]={"Other",true};AnnotationController controller(repo);
    auto selected=select(repo);auto initial=selected.indexed;
    auto edit=controller.rename(selected,"First run");CHECK(edit.status==EditStatus::updated && edit.write==WriteState::returned && edit.observed && edit.observed->row);
    CHECK(edit.observed->row->annotation->name=="First run" && !edit.observed->row->annotation->favorite && repo.writes==1 && repo.last_target==id('a'));
    CHECK(controller.rename(selected,"First run").status==EditStatus::unchanged && repo.writes==1);
    auto named=*edit.observed->row;edit=controller.set_favorite(named,true);
    CHECK(edit.status==EditStatus::updated && edit.observed->row->annotation->name=="First run" && edit.observed->row->annotation->favorite);
    CHECK(controller.set_favorite(named,true).status==EditStatus::unchanged && repo.writes==2);
    auto favored=*edit.observed->row;edit=controller.rename(favored,u8"Cr\u00e8me \u2014 \u96ea \U0001f3c2");
    CHECK(edit.status==EditStatus::updated && edit.observed->row->annotation->name==u8"Cr\u00e8me \u2014 \u96ea \U0001f3c2" && edit.observed->row->annotation->favorite);
    edit=controller.set_favorite(*edit.observed->row,false);
    CHECK(edit.status==EditStatus::updated && !edit.observed->row->annotation->favorite && edit.observed->row->annotation->name==u8"Cr\u00e8me \u2014 \u96ea \U0001f3c2");
    CHECK(repo.writes==4 && identical(repo.records.at(id('a')),initial) && identical(repo.records.at(id('b')),record('b',Origin::synthetic)));
    CHECK(repo.labels.at(id('b')).name=="Other" && repo.labels.at(id('b')).favorite && !selected.annotation && named.annotation->name=="First run" && !named.annotation->favorite);
    CHECK(repo.no_other_calls());return true;
}
bool absent_annotation_and_origins(){
    for(const auto origin:{Origin::local,Origin::imported,Origin::synthetic}){
        Repo repo;repo.records.emplace(id('a'),record('a',origin));AnnotationController controller(repo);const auto selected=select(repo);
        auto result=controller.set_favorite(selected,false);CHECK(result.status==EditStatus::unchanged && !repo.writes && repo.labels.empty());
        result=controller.set_favorite(selected,true);CHECK(result.status==EditStatus::updated && repo.labels.at(id('a')).name==id('a') && repo.labels.at(id('a')).favorite);
        CHECK(controller.set_favorite(selected,true).status==EditStatus::unchanged && repo.writes==1);
        CHECK(controller.set_favorite(selected,false).status==EditStatus::stale_annotation && repo.writes==1);
        result=controller.set_favorite(*result.observed->row,false);CHECK(result.status==EditStatus::updated && !repo.labels.at(id('a')).favorite && repo.labels.at(id('a')).name==id('a'));
        CHECK(repo.records.at(id('a')).origin==origin && identical(repo.records.at(id('a')),selected.indexed) && repo.no_other_calls());
    }
    return true;
}
bool stale_annotation(){
    Repo repo;repo.records.emplace(id('a'),record('a'));repo.labels[id('a')]={"Old",false};AnnotationController controller(repo);
    const auto selected=select(repo);repo.labels.at(id('a')).favorite=true;
    auto result=controller.rename(selected,"New");CHECK(result.status==EditStatus::stale_annotation && result.write==WriteState::not_attempted && result.observed->row->annotation->favorite && !repo.writes);
    repo.labels.at(id('a'))={"Different",false};CHECK(controller.set_favorite(selected,true).status==EditStatus::stale_annotation && !repo.writes);
    repo.labels.at(id('a'))={"Different",true};CHECK(controller.set_favorite(selected,true).status==EditStatus::stale_annotation && !repo.writes);
    repo.labels.erase(id('a'));CHECK(controller.rename(selected,"New").status==EditStatus::stale_annotation && !repo.writes);
    repo.labels[id('a')]={"New",false};CHECK(controller.rename(selected,"New").status==EditStatus::unchanged && !repo.writes);
    CHECK(repo.no_other_calls());return true;
}
bool evidence_identity(){
    Repo repo;repo.records.emplace(id('a'),record('a'));repo.labels[id('a')]={"Old",false};AnnotationController controller(repo);const auto selected=select(repo);
    for(unsigned i=0;i<8;++i){
        auto r=selected.indexed;
        switch(i){case 0:r.manifest_sha256=std::string(64,'c');break;case 1:r.category="other";break;case 2:r.profile=std::string(64,'c');break;case 3:r.event_semantics="other";break;case 4:r.origin=Origin::local;break;case 5:r.status=Status::failed;r.igt_ms.reset();break;case 6:r.igt_ms=101;break;default:r.rta_ns.reset();break;}
        repo.records.at(id('a'))=r;
        CHECK(controller.rename(selected,"New").status==EditStatus::evidence_conflict && !repo.writes);
    }
    repo.records.erase(id('a'));CHECK(controller.set_favorite(selected,true).status==EditStatus::missing && !repo.writes);
    repo.records[id('a')]=selected.indexed;repo.wrong_id=true;CHECK(controller.set_favorite(selected,true).status==EditStatus::invalid_data && !repo.writes);repo.wrong_id=false;
    repo.records.at(id('a')).manifest_sha256="bad";CHECK(controller.rename(selected,"New").status==EditStatus::invalid_data && !repo.writes);
    repo.records.at(id('a'))=selected.indexed;repo.fail_find=true;CHECK(controller.rename(selected,"New").status==EditStatus::unavailable && !repo.writes);repo.fail_find=false;
    repo.fail_label=true;CHECK(controller.rename(selected,"New").status==EditStatus::unavailable && !repo.writes);repo.fail_label=false;
    repo.labels.at(id('a')).name="\n";CHECK(controller.rename(selected,"New").status==EditStatus::invalid_data && !repo.writes);
    CHECK(repo.no_other_calls());return true;
}
bool invalid_input(){
    Repo repo;repo.records.emplace(id('a'),record('a'));AnnotationController controller(repo);const auto selected=select(repo);const auto calls=repo.finds;
    for(const auto& name:std::vector<std::string>{"",std::string(257,'a'),"a\n","a\x7f",std::string("a\0b",3),"\xc0\x80","\xe2\x82","\xed\xa0\x80","\xf4\x90\x80\x80","\xc2\x80","\x80"}){
        CHECK(controller.rename(selected,name).status==EditStatus::invalid_request && !repo.writes);
    }
    auto row=selected;row.indexed.run_id="../x";CHECK(controller.set_favorite(row,true).status==EditStatus::invalid_request);
    row=selected;row.indexed.manifest_sha256="bad";CHECK(controller.rename(row,"OK").status==EditStatus::invalid_request);
    row=selected;row.annotation_state=ReadState::unavailable;CHECK(controller.set_favorite(row,true).status==EditStatus::selection_unavailable);
    row.annotation_state=ReadState::invalid_data;CHECK(controller.rename(row,"OK").status==EditStatus::selection_unavailable);
    row=selected;row.annotation_state=ReadState::available;CHECK(controller.rename(row,"OK").status==EditStatus::invalid_request);
    row=selected;row.annotation_state=static_cast<ReadState>(99);CHECK(controller.rename(row,"OK").status==EditStatus::invalid_request);
    row=selected;row.annotation=Annotation{"bad\r",false};row.annotation_state=ReadState::available;CHECK(controller.set_favorite(row,true).status==EditStatus::invalid_request);
    CHECK(repo.finds==calls && !repo.writes);
    CHECK(controller.rename(selected,std::string(256,'a')).status==EditStatus::updated && repo.labels.at(id('a')).name.size()==256 && repo.no_other_calls());
    return true;
}
bool write_errors(){
    for(auto fault:{Repo::Fault::before_write,Repo::Fault::after_write,Repo::Fault::partial_label}){
        Repo repo;repo.records.emplace(id('a'),record('a'));repo.labels[id('a')]={"Old",false};AnnotationController controller(repo);const auto selected=select(repo);repo.fault=fault;
        const auto before=repo.finds;auto result=controller.rename(selected,"New");
        CHECK(result.status==EditStatus::write_uncertain && result.write==WriteState::threw && result.observed && result.observed->row && repo.finds==before+2 && repo.writes==1);
        CHECK(identical(result.observed->row->indexed,selected.indexed));
        const auto label=result.observed->row->annotation;CHECK(label && label->name==(fault==Repo::Fault::before_write?"Old":"New") && label->favorite==(fault==Repo::Fault::partial_label));
        repo.fault=Repo::Fault::none;
        if(fault==Repo::Fault::before_write)CHECK(controller.rename(selected,"New").status==EditStatus::updated && repo.writes==2);
        else if(fault==Repo::Fault::after_write)CHECK(controller.rename(selected,"New").status==EditStatus::unchanged && repo.writes==1);
        else CHECK(controller.rename(selected,"New").status==EditStatus::stale_annotation && repo.writes==1);
        CHECK(repo.no_other_calls());
    }
    return true;
}
bool uncertain_readback(){
    for(auto fault:{Repo::Fault::no_effect,Repo::Fault::wrong_label,Repo::Fault::changed_record,Repo::Fault::missing_record,Repo::Fault::corrupt_record,Repo::Fault::unreadable_record,Repo::Fault::unreadable_label,Repo::Fault::corrupt_label}){
        Repo repo;repo.records.emplace(id('a'),record('a'));repo.labels[id('a')]={"Old",false};AnnotationController controller(repo);const auto selected=select(repo);repo.fault=fault;
        const auto before=repo.finds;auto result=controller.set_favorite(selected,true);
        CHECK(result.status==EditStatus::readback_uncertain && result.write==WriteState::returned && result.observed && repo.writes==1 && repo.finds==before+2);
        const auto& observed=*result.observed;
        if(fault==Repo::Fault::missing_record)CHECK(observed.state==ReadState::absent && !observed.row);
        else if(fault==Repo::Fault::corrupt_record)CHECK(observed.state==ReadState::invalid_data && !observed.row);
        else if(fault==Repo::Fault::unreadable_record)CHECK(observed.state==ReadState::unavailable && !observed.row);
        else {
            CHECK(observed.row && observed.state==ReadState::available);
            if(fault==Repo::Fault::changed_record)CHECK(!identical(observed.row->indexed,selected.indexed));
            else CHECK(identical(observed.row->indexed,selected.indexed));
            if(fault==Repo::Fault::unreadable_label)CHECK(observed.row->annotation_state==ReadState::unavailable && !observed.row->annotation);
            if(fault==Repo::Fault::corrupt_label)CHECK(observed.row->annotation_state==ReadState::invalid_data && !observed.row->annotation);
        }
        CHECK(repo.no_other_calls()); // no repair or retry even for broken adapter
    }
    return true;
}
int main(){
    for(auto test:{success_and_idempotence,absent_annotation_and_origins,stale_annotation,evidence_identity,invalid_input,write_errors,uncertain_readback})if(!test())return 1;
    std::cout<<"B-RUN-02: 7 annotation controller scenario groups PASS\n";return 0;
}
