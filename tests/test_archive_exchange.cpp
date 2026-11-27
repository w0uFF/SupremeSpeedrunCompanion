#include "src/modules/exporter/public/archive_exchange.hpp"
#include <iostream>
#include <map>
using namespace supreme::run_library;
using namespace supreme::exporter::exchange;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
std::string id(char c){return std::string(32,c);}
Verified sample(Origin origin=Origin::imported){return {{id('a'),"unclassified",unclassified_profile,"events",std::string(64,'b'),origin,Status::finished,0,100},std::string(64,'c'),123};}
Slot final(const Verified& a){return {SlotState::finalized,a};}
struct Repo final:Repository {
    std::optional<Record> stored;
    Annotation label{"Private favorite",true};
    bool fail=false;
    mutable unsigned reads=0,scans=0;
    unsigned inserts=0,annotations=0;
    Insert insert(const Record&)override{++inserts;throw std::logic_error("controller must not insert");}
    std::optional<Record> find(std::string_view)const override{++reads;if(fail)throw std::runtime_error("index");return stored;}
    std::vector<Record> recent(std::size_t)const override{++scans;throw std::logic_error("no scans");}
    void annotate(std::string_view,const Annotation&)override{++annotations;throw std::logic_error("no labels");}
    bool untouched()const{return !inserts && !annotations && !scans && label.name=="Private favorite" && label.favorite;}
};
struct Fake final:Port {
    enum class Fault { none, interrupted, after_commit, wrong_ack, wrong_bytes, missing_index, changed_record, readback_failure, guard_conflict };
    Repo& repo;
    Verified original=sample();
    Slot input=final(original),archive{SlotState::absent,{}},output{SlotState::absent,{}};
    Fault fault=Fault::none;
    mutable unsigned input_reads=0,archive_reads=0,output_reads=0;
    unsigned imports=0,exports=0;
    bool fail_input=false,fail_archive=false,fail_output=false;
    explicit Fake(Repo& r):repo(r){}
    Slot selected(const Selection& s)const override{++input_reads;if(fail_input)throw std::runtime_error("source");if(s!=id('d'))return {SlotState::absent,{}};return input;}
    Slot managed(const Record&)const override{++archive_reads;if(fail_archive)throw std::runtime_error("archive");return archive;}
    Slot destination(const Selection& s,Origin)const override{++output_reads;if(fail_output)throw std::runtime_error("destination");if(s!=id('e'))return {};return output;}
    static bool same_slot(const Slot& a,const Slot& b){return a.state==b.state && a.archive.has_value()==b.archive.has_value() && (!a.archive || same(*a.archive,*b.archive));}
    Operation admit_import(const Selection& s,const Verified& expected,const Slot& guard)override{
        ++imports;if(s!=id('d') || !input.archive || !same(*input.archive,expected) || !same_slot(guard,archive) || fault==Fault::guard_conflict)return Operation::conflict;
        if(fault==Fault::interrupted){archive={SlotState::pending,expected};throw std::runtime_error("interrupted");}
        archive=final(expected);
        if(fault!=Fault::missing_index)repo.stored=expected.projection; // fake existing admission operation
        if(fault==Fault::changed_record)repo.stored->rta_ns=999;
        if(fault==Fault::wrong_bytes)archive.archive->zip_sha256=std::string(64,'f');
        if(fault==Fault::readback_failure)fail_archive=true;
        if(fault==Fault::after_commit)throw std::runtime_error("lost response");
        return fault==Fault::wrong_ack?Operation::unavailable:Operation::completed;
    }
    Operation publish_exact(const Selection& s,const Verified& expected,const Slot& guard)override{
        ++exports;if(s!=id('e') || !archive.archive || !same(*archive.archive,expected) || !same_slot(guard,output) || fault==Fault::guard_conflict)return Operation::conflict;
        if(fault==Fault::interrupted){output={SlotState::pending,expected};throw std::runtime_error("interrupted");}
        output=final(expected);
        if(fault==Fault::wrong_bytes)output.archive->zip_sha256=std::string(64,'f');
        if(fault==Fault::changed_record)repo.stored->rta_ns=999;
        if(fault==Fault::readback_failure)fail_output=true;
        if(fault==Fault::after_commit)throw std::runtime_error("lost response");
        return fault==Fault::wrong_ack?Operation::unavailable:Operation::completed;
    }
    void ready(const Verified& a){archive=final(a);repo.stored=a.projection;}
};
bool import_success(){
    Repo repo;Fake port(repo);Controller c(repo,&port);const auto a=sample();
    CHECK(c.preview_import(id('d')).state==SlotState::finalized && repo.reads==0 && !port.imports);
    auto r=c.import_selected(id('d'),a);CHECK(r.code==ResultCode::completed && r.attempted && r.acknowledgement==Operation::completed && r.observed.archive && same(*r.observed.archive,a));
    CHECK(repo.stored && identical(*repo.stored,a.projection) && port.imports==1 && repo.reads==2);
    CHECK(c.import_selected(id('d'),a).code==ResultCode::unchanged && port.imports==1);
    CHECK(same(*port.input.archive,a) && repo.untouched() && !port.exports);return true;
}
bool export_success(){
    for(const auto origin:{Origin::imported,Origin::synthetic}){
        Repo repo;Fake port(repo);Controller c(repo,&port);const auto a=sample(origin);port.ready(a);
        const auto preview=c.preview_export(a.projection);CHECK(preview.archive && same(*preview.archive,a) && !port.exports);
        auto r=c.export_selected(a.projection,a,id('e'));CHECK(r.code==ResultCode::completed && r.attempted && r.observed.archive && same(*r.observed.archive,a));
        CHECK(c.export_selected(a.projection,a,id('e')).code==ResultCode::unchanged && port.exports==1);
        CHECK(same(*port.archive.archive,a) && identical(*repo.stored,a.projection) && repo.untouched() && !port.imports);
    }return true;
}
bool invalid_and_stale(){
    Repo repo;Fake port(repo);Controller c(repo,&port);const auto a=sample();
    auto bad=a;bad.projection.origin=Origin::local;CHECK(c.import_selected(id('d'),bad).code==ResultCode::unsupported_local);
    bad=a;bad.projection.origin=Origin::synthetic;CHECK(c.import_selected(id('d'),bad).code==ResultCode::invalid_request);
    bad=a;bad.bytes=archive_limit+1;CHECK(c.import_selected(id('d'),bad).code==ResultCode::invalid_request);
    bad=a;bad.bytes=21;CHECK(c.import_selected(id('d'),bad).code==ResultCode::invalid_request);
    bad=a;bad.projection.profile=std::string(64,'f');CHECK(c.import_selected(id('d'),bad).code==ResultCode::invalid_request);
    bad=a;bad.zip_sha256="bad";CHECK(c.import_selected(id('d'),bad).code==ResultCode::invalid_request);
    CHECK(c.import_selected("../file.zip",a).code==ResultCode::invalid_request && !repo.reads && !port.input_reads);
    for(unsigned i=0;i<4;++i){auto changed=a;switch(i){case 0:changed.projection.run_id=id('b');break;case 1:changed.projection.manifest_sha256=std::string(64,'f');break;case 2:changed.projection.rta_ns=101;break;default:changed.zip_sha256=std::string(64,'f');break;}port.input=final(changed);CHECK(c.import_selected(id('d'),a).code==ResultCode::stale_source);}
    port.input=final(sample(Origin::synthetic));CHECK(c.import_selected(id('d'),a).code==ResultCode::review);
    port.input=final(a);port.ready(a);repo.stored->rta_ns=101;CHECK(c.export_selected(a.projection,a,id('e')).code==ResultCode::conflict);
    repo.stored=a.projection;port.archive.archive->zip_sha256=std::string(64,'f');CHECK(c.export_selected(a.projection,a,id('e')).code==ResultCode::stale_source);
    CHECK(!port.imports && !port.exports && repo.untouched());return true;
}
bool collision_and_recovery(){
    Repo repo;Fake port(repo);Controller c(repo,&port);const auto a=sample();auto other=a;other.zip_sha256=std::string(64,'f');
    for(auto state:{SlotState::pending,SlotState::finalized,SlotState::alias}){port.archive={state,other};CHECK(c.import_selected(id('d'),a).code==ResultCode::conflict);}
    port.archive={SlotState::review,{}};CHECK(c.import_selected(id('d'),a).code==ResultCode::review);
    port.archive={SlotState::absent,{}};repo.stored=a.projection;CHECK(c.import_selected(id('d'),a).code==ResultCode::review);
    port.archive={SlotState::pending,a};CHECK(c.import_selected(id('d'),a).code==ResultCode::review);
    repo.stored->origin=Origin::local;CHECK(c.import_selected(id('d'),a).code==ResultCode::conflict && !port.imports);
    repo.stored.reset();port.archive=final(a);CHECK(c.import_selected(id('d'),a).code==ResultCode::completed && port.imports==1); // finalized unindexed
    port.archive={SlotState::alias,a};CHECK(c.import_selected(id('d'),a).code==ResultCode::completed && port.imports==2); // explicit import alias retry
    for(auto state:{SlotState::pending,SlotState::finalized,SlotState::alias}){port.output={state,other};CHECK(c.export_selected(a.projection,a,id('e')).code==ResultCode::conflict);}
    port.output={SlotState::alias,a};CHECK(c.export_selected(a.projection,a,id('e')).code==ResultCode::review);
    port.output={SlotState::review,{}};CHECK(c.export_selected(a.projection,a,id('e')).code==ResultCode::review && !port.exports);
    port.output={SlotState::pending,a};CHECK(c.export_selected(a.projection,a,id('e')).code==ResultCode::completed && port.exports==1 && repo.untouched());return true;
}
bool unavailable(){
    Repo repo;Fake port(repo);Controller c(repo,&port),disabled(repo);const auto a=sample();
    CHECK(disabled.import_selected(id('d'),a).code==ResultCode::unavailable && disabled.export_selected(a.projection,a,id('e')).code==ResultCode::unavailable);
    port.fail_input=true;CHECK(c.import_selected(id('d'),a).code==ResultCode::unavailable);port.fail_input=false;
    port.input={SlotState::absent,{}};CHECK(c.import_selected(id('d'),a).code==ResultCode::missing);port.input=final(a);
    port.input={SlotState::finalized,{}};CHECK(c.import_selected(id('d'),a).code==ResultCode::review);port.input=final(a);
    repo.fail=true;CHECK(c.import_selected(id('d'),a).code==ResultCode::unavailable);repo.fail=false;
    port.fail_archive=true;CHECK(c.import_selected(id('d'),a).code==ResultCode::unavailable);port.fail_archive=false;
    CHECK(c.export_selected(a.projection,a,id('e')).code==ResultCode::missing);port.ready(a);
    port.fail_output=true;CHECK(c.export_selected(a.projection,a,id('e')).code==ResultCode::unavailable);port.fail_output=false;
    port.archive={SlotState::pending,a};CHECK(c.export_selected(a.projection,a,id('e')).code==ResultCode::review);
    CHECK(!port.imports && !port.exports && repo.untouched());return true;
}
bool interrupted_and_ack(){
    for(bool exporting:{false,true})for(auto fault:{Fake::Fault::interrupted,Fake::Fault::after_commit,Fake::Fault::wrong_ack}){
        Repo repo;Fake port(repo);Controller c(repo,&port);const auto a=sample();if(exporting)port.ready(a);port.fault=fault;
        const auto run=[&]{return exporting?c.export_selected(a.projection,a,id('e')):c.import_selected(id('d'),a);};
        auto r=run();CHECK(r.code==ResultCode::operation_uncertain && r.attempted && r.observed.archive);
        CHECK(r.observed.state==(fault==Fake::Fault::interrupted?SlotState::pending:SlotState::finalized));
        CHECK((fault==Fake::Fault::wrong_ack?r.acknowledgement==Operation::unavailable:!r.acknowledgement));
        CHECK(port.imports+port.exports==1);port.fault=Fake::Fault::none;r=run();
        CHECK(r.code==(fault==Fake::Fault::interrupted?ResultCode::completed:ResultCode::unchanged));
        CHECK(port.imports+port.exports==(fault==Fake::Fault::interrupted?2u:1u) && repo.untouched());
    }return true;
}
bool postcondition_faults(){
    for(bool exporting:{false,true})for(auto fault:{Fake::Fault::wrong_bytes,Fake::Fault::changed_record,Fake::Fault::readback_failure,Fake::Fault::guard_conflict,Fake::Fault::missing_index}){
        if(exporting && fault==Fake::Fault::missing_index)continue;
        Repo repo;Fake port(repo);Controller c(repo,&port);const auto a=sample();if(exporting)port.ready(a);port.fault=fault;
        const auto r=exporting?c.export_selected(a.projection,a,id('e')):c.import_selected(id('d'),a);
        CHECK(r.code==ResultCode::operation_uncertain && r.attempted && port.imports+port.exports==1);
        CHECK(r.acknowledgement==(fault==Fake::Fault::guard_conflict?Operation::conflict:Operation::completed));
        if(fault==Fake::Fault::readback_failure)CHECK(r.observed.state==SlotState::unavailable);
        CHECK(repo.untouched());
    }return true;
}
int main(){for(auto test:{import_success,export_success,invalid_and_stale,collision_and_recovery,unavailable,interrupted_and_ack,postcondition_faults})if(!test())return 1;
    std::cout<<"B-EXP-01: 7 archive exchange scenario groups PASS\n";return 0;}
