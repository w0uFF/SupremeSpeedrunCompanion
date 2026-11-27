#include "src/modules/playback/public/planner.hpp"
#include <array>
#include <iostream>
using namespace supreme::playback;
using supreme::run_library::Status;
using supreme::run_library::Insert;
using supreme::run_library::Annotation;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
Selection sample(Kind kind=Kind::video,Origin origin=Origin::imported){
    return {{{std::string(32,'a'),"unclassified",exchange::unclassified_profile,"events",std::string(64,'b'),origin,Status::finished,0,123},std::string(64,'c'),4096},
            {kind,kind==Kind::video?"video.mp4":"race.dat",std::string(64,'d'),1024}};
}
struct Repo final:Repository {
    std::optional<Record> row;
    mutable unsigned reads=0;
    unsigned writes=0;
    bool fail=false;
    Insert insert(const Record&) override {++writes;throw std::logic_error("insert");}
    std::optional<Record> find(std::string_view)const override {++reads;if(fail)throw std::runtime_error("read");return row;}
    std::vector<Record> recent(std::size_t)const override {throw std::logic_error("scan");}
    void annotate(std::string_view,const Annotation&)override {++writes;throw std::logic_error("annotate");}
};
struct Input final:Source {
    Observation value;
    mutable unsigned reads=0;
    bool fail=false;
    Observation observe(const Record&,std::string_view,Kind)const override {++reads;if(fail)throw std::runtime_error("source");return value;}
};
struct Backend final:CapabilitySource {
    std::optional<Capabilities> value;
    mutable unsigned reads=0;
    bool fail=false;
    std::optional<Capabilities> inspect(const Selection&)const override {++reads;if(fail)throw std::runtime_error("capability");return value;}
};
struct Fixture {
    Selection s;
    Repo repo;
    Input source;
    Backend backend;
    Controller c;
    explicit Fixture(Kind kind=Kind::video,Origin origin=Origin::imported):s(sample(kind,origin)),c(repo,&source,&backend){
        repo.row=s.archive.projection;source.value.slot.state=exchange::SlotState::finalized;source.value.slot.archive.emplace(s.archive);source.value.member.emplace(s.member);
        backend.value=Capabilities{s,Gate::established,Gate::established,Gate::established,true,
            kind==Kind::video,kind==Kind::video,kind==Kind::video,
            kind==Kind::video?std::optional<std::uint64_t>{100000000}:std::nullopt};
    }
    Result plan(Intent i={})const{return c.plan(s,i);}
};
bool video_intents(){
    Fixture f;auto selection=f.c.select(f.s.archive,"video.mp4",Kind::video);
    CHECK(selection.state==State::selected && selection.selection && same(*selection.selection,f.s) && !f.backend.reads);
    for(const auto& i:std::array<Intent,7>{{{Action::open,{},{}},{Action::pause,{},{}},{Action::resume,{},{}},
        {Action::seek,0,{}},{Action::seek,100000000,{}},{Action::rate,{},500u},{Action::rate,{},2000u}}}){
        auto r=f.plan(i);CHECK(r.state==State::planned && r.plan && same(r.plan->selection,f.s));
        CHECK(r.plan->intent.action==i.action && r.plan->intent.position_100ns==i.position_100ns && r.plan->intent.rate_permille==i.rate_permille);
        CHECK(r.plan->video_duration_100ns==100000000);
    }
    CHECK(f.plan({Action::seek,100000001,{}}).state==State::invalid_request);
    CHECK(f.repo.writes==0 && f.repo.reads==9 && f.source.reads==9 && f.backend.reads==8);return true;
}
bool native_separation(){
    Fixture f(Kind::native_replay,Origin::synthetic);auto r=f.plan();CHECK(r.state==State::planned && r.plan && !r.plan->video_duration_100ns);
    CHECK(f.plan({Action::pause,{},{}}).state==State::unsupported && f.plan({Action::seek,0,{}}).state==State::unsupported);
    CHECK(f.plan({Action::rate,{},1000u}).state==State::unsupported);
    f.backend.value->video_duration_100ns=100;CHECK(f.plan().state==State::review_required);
    f.backend.value->video_duration_100ns.reset();f.backend.value->seek=true;CHECK(f.plan().state==State::review_required);
    return true;
}
bool invalid_before_io(){
    Fixture f;auto bad=f.s;bad.archive.projection.origin=Origin::local;CHECK(f.c.plan(bad,{}).state==State::unsupported_local);
    for(auto name:{"../video.mp4","/video.mp4","video.mp4.","a\\b",""}){bad=f.s;bad.member.name=name;CHECK(f.c.plan(bad,{}).state==State::invalid_request);}
    bad=f.s;bad.member.bytes=0;CHECK(f.c.plan(bad,{}).state==State::invalid_request);
    bad=f.s;bad.member.bytes=f.s.archive.bytes+1;CHECK(f.c.plan(bad,{}).state==State::invalid_request);
    bad=f.s;bad.member.sha256="bad";CHECK(f.c.plan(bad,{}).state==State::invalid_request);
    bad=f.s;bad.member.kind=static_cast<Kind>(99);CHECK(f.c.plan(bad,{}).state==State::invalid_request);
    bad=f.s;bad.archive.zip_sha256="bad";CHECK(f.c.plan(bad,{}).state==State::invalid_request);
    for(auto i:{Intent{Action::open,0,{}},Intent{Action::seek,{},{}},Intent{Action::rate,{},0u},Intent{Action::rate,0,1000u},Intent{static_cast<Action>(99),{},{}}})CHECK(f.plan(i).state==State::invalid_request);
    CHECK(!f.repo.reads && !f.source.reads && !f.backend.reads);return true;
}
bool stale_identity(){
    Fixture f;f.repo.row->rta_ns=124;CHECK(f.plan().state==State::stale_selection && !f.source.reads);
    f.repo.row=f.s.archive.projection;
    for(unsigned i=0;i<3;++i){auto changed=f.s.archive;switch(i){case 0:changed.zip_sha256=std::string(64,'f');break;case 1:++changed.bytes;break;default:changed.projection.manifest_sha256=std::string(64,'e');break;}
        f.source.value.slot.archive=changed;CHECK(f.plan().state==State::stale_selection);}
    f.source.value.slot.archive=f.s.archive;f.source.value.member->sha256=std::string(64,'e');CHECK(f.plan().state==State::stale_selection);
    f.source.value.member=f.s.member;f.source.value.member->kind=Kind::native_replay;CHECK(f.plan().state==State::review_required);
    f.source.value.member=f.s.member;f.source.value.member->name="other.mp4";CHECK(f.plan().state==State::review_required);
    f.source.value.member=f.s.member;CHECK(!f.backend.reads);
    f.backend.value->bound.member.bytes++;CHECK(f.plan().state==State::stale_selection);return true;
}
bool source_outcomes(){
    Fixture f;Controller disabled(f.repo);CHECK(disabled.plan(f.s,{}).state==State::unavailable);
    f.repo.row.reset();CHECK(f.plan().state==State::missing);f.repo.row=f.s.archive.projection;
    f.repo.fail=true;CHECK(f.plan().state==State::unavailable);f.repo.fail=false;
    f.source.fail=true;CHECK(f.plan().state==State::unavailable);f.source.fail=false;
    f.source.value={{exchange::SlotState::absent,{}},{}};CHECK(f.plan().state==State::missing);
    f.source.value={{exchange::SlotState::unavailable,{}},{}};CHECK(f.plan().state==State::unavailable);
    for(auto state:{exchange::SlotState::pending,exchange::SlotState::alias}){f.source.value={{state,f.s.archive},{}};CHECK(f.plan().state==State::review_required);}
    f.source.value={{exchange::SlotState::finalized,{}},{}};CHECK(f.plan().state==State::review_required);
    f.source.value={{exchange::SlotState::absent,{}},f.s.member};CHECK(f.plan().state==State::review_required);
    f.source.value={{exchange::SlotState::finalized,f.s.archive},{}};CHECK(f.plan().state==State::missing && !f.backend.reads);
    return true;
}
bool capability_gates(){
    Fixture f;Controller no_cap(f.repo,&f.source);CHECK(no_cap.plan(f.s,{}).state==State::unavailable);
    const auto original=f.backend.value;f.backend.value.reset();CHECK(f.plan().state==State::unavailable);f.backend.value=original;
    f.backend.fail=true;CHECK(f.plan().state==State::unavailable);f.backend.fail=false;
    for(auto field:{&Capabilities::member_policy,&Capabilities::reader_isolation,&Capabilities::format})for(auto gate:{Gate::unavailable,Gate::unsupported,Gate::review,static_cast<Gate>(99)}){
        f.backend.value=original;(*f.backend.value).*field=gate;
        const auto r=f.plan();CHECK(!r.plan && r.state==(gate==Gate::unavailable?State::unavailable:gate==Gate::unsupported?State::unsupported:State::review_required));}
    f.backend.value=original;f.backend.value->open=false;CHECK(f.plan().state==State::review_required);
    f.backend.value=original;f.backend.value->video_duration_100ns.reset();CHECK(f.plan().state==State::review_required);
    f.backend.value=original;f.backend.value->video_duration_100ns=0;CHECK(f.plan().state==State::review_required);
    f.backend.value=original;f.backend.value->seek=false;f.backend.value->pause_resume=false;f.backend.value->rate=false;
    CHECK(f.plan().state==State::planned && f.plan({Action::pause,{},{}}).state==State::unsupported && f.plan({Action::seek,0,{}}).state==State::unsupported && f.plan({Action::rate,{},1000u}).state==State::unsupported);
    return true;
}
bool copied_and_no_authority(){
    Fixture f;auto before=f.s;auto r=f.plan();CHECK(r.plan && r.state==State::planned);
    f.source.value.member->bytes=1;f.repo.row.reset();f.backend.value.reset();
    CHECK(same(r.plan->selection,before) && r.plan->selection.archive.projection.igt_ms==0 && f.repo.writes==0);
    CHECK(f.plan().state==State::missing);return true;
}
int main(){for(auto test:{video_intents,native_separation,invalid_before_io,stale_identity,source_outcomes,capability_gates,copied_and_no_authority})if(!test())return 1;
    std::cout<<"B-PLAY-01: 7 playback planning scenario groups PASS\n";return 0;}
