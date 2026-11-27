#include "src/modules/recorder/public/orchestrator.hpp"
#include <iostream>
using namespace supreme;
using namespace supreme::recorder;
using namespace supreme::recorder::application;
using K=session::EventKind;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
struct Fake final:Backend {
    bool available=true;
    unsigned starts=0,stops=0,cancels=0;
    Id last_start=0,last_stop=0,last_cancel=0,through=0;
    bool start(const Request& r) noexcept override {++starts;last_start=r.capture;return available;}
    void stop(Id id,Id ms) noexcept override {++stops;last_stop=id;through=ms;}
    void cancel(Id id) noexcept override {++cancels;last_cancel=id;}
};
session::Event event(Id run,Id attempt,Id seq,Id ms,K kind){
    return {run,seq,attempt,ms,kind,kind==K::start||kind==K::finish?std::optional{categories::Track::alpine_easy}:std::nullopt,
        kind==K::finish?std::optional<Id>{1000}:std::nullopt};
}
bool terminal(Orchestrator& o,Id capture,State state=State::finalized,bool through=true,bool drop=false){
    std::array<StreamQuality,2> quality{};
    quality[0].accepted=1;quality[0].written.observe(0,400000);quality[0].dropped_full=drop?1:0;
    return o.completed(capture,state,{1,1,drop?1u:0u,0,0},quality,through);
}
bool automatic_result(){
    Fake b;Orchestrator o(b);
    CHECK(o.prepare(1,9,{10,1},100)==Decision::accepted);
    CHECK(b.starts==1 && o.prepare(1,9,{10,1},100)==Decision::rejected);
    CHECK(!o.acquired(2,101) && o.acquired(1,101) && !o.acquired(1,102));
    CHECK(!o.event(1,event(11,1,1,110,K::start)));
    CHECK(o.event(1,event(10,1,1,110,K::start)));
    CHECK(!o.event(1,event(10,1,1,111,K::start)));
    CHECK(o.event(1,event(10,1,2,120,K::reset_to_track)));
    CHECK(!o.event(1,event(10,1,2,121,K::reset_to_track)));
    CHECK(!o.event(1,event(10,1,3,119,K::finish)));
    CHECK(!o.result_visible(1,{10,1},125));
    CHECK(o.event(1,event(10,1,3,130,K::finish)) && b.stops==0);
    CHECK(!o.result_visible(1,{10,2},140));
    CHECK(o.result_visible(1,{10,1},140) && b.stops==1 && b.through==140);
    CHECK(!o.result_visible(1,{10,1},141) && !o.take(1));
    CHECK(o.prepare(2,9,{11,1},141)==Decision::accepted);
    CHECK(terminal(o,1) && !terminal(o,1));
    auto r=o.take(1);CHECK(r && !r->incomplete && r->attempts.size()==1 && r->stop_boundary_reached);
    CHECK(r->attempts[0].end_ms==130 && r->attempts[0].result_ms==140 && r->ledger.markers()[0].end==capture_boundary::End::finished);
    CHECK(r->ledger.state()==capture_boundary::State::finalized && !o.take(1));
    CHECK(!o.event(1,event(10,1,4,150,K::restart_race)) && o.acquired(2,142));
    o.shutdown(150);CHECK(terminal(o,2,State::cancelled,false));return true;
}
bool restart_and_manual(){
    Fake b;Orchestrator o(b);
    CHECK(o.prepare(1,9,{10,1},1)==Decision::accepted && o.acquired(1,2));
    CHECK(o.event(1,event(10,1,1,3,K::start)) && o.event(1,event(10,1,2,4,K::restart_race)));
    CHECK(b.stops==1 && o.manual_start(2,9,5)==Decision::accepted && o.acquired(2,6));
    CHECK(terminal(o,1));auto first=o.take(1);CHECK(first && first->incomplete && first->attempts[0].state==session::AttemptState::restarted);
    CHECK(o.selection(2,20,7) && o.event(2,event(20,1,1,8,K::start)));
    CHECK(o.prepare(3,9,{20,1},8)==Decision::busy && !o.interrupt(2,9));
    CHECK(o.event(2,event(20,1,2,10,K::reset_to_track)) && o.event(2,event(20,1,3,11,K::restart_race)));
    CHECK(o.event(2,event(20,2,4,12,K::start)) && !o.event(2,event(20,1,5,13,K::finish)));
    CHECK(o.event(2,event(20,2,5,14,K::finish)) && o.result_visible(2,{20,2},15));
    CHECK(b.stops==1 && o.selection(2,21,16));
    CHECK(o.event(2,event(21,1,1,17,K::start)) && o.event(2,event(21,1,2,18,K::finish)));
    CHECK(o.result_visible(2,{21,1},19) && o.manual_stop(2,20) && b.stops==2);
    CHECK(terminal(o,2));auto r=o.take(2);CHECK(r && !r->incomplete && r->attempts.size()==3);
    CHECK(r->attempts[0].selection_observed && r->attempts[1].selection_observed && r->attempts[2].selection_observed);
    CHECK(r->ledger.markers().size()==3 && r->request.mode==Mode::manual);return true;
}
bool late_or_missing_boundaries(){
    for(bool late:{false,true}){
        Fake b;Orchestrator o(b);CHECK(o.prepare(1,9,{10,1},1)==Decision::accepted);
        if(!late)CHECK(o.acquired(1,2));
        CHECK(o.event(1,event(10,1,1,3,K::start)));
        if(late)CHECK(o.acquired(1,4));
        CHECK(o.event(1,event(10,1,2,5,K::finish)));
        if(late)CHECK(o.result_visible(1,{10,1},6));else CHECK(o.interrupt(1,6));
        CHECK(terminal(o,1));auto r=o.take(1);CHECK(r && r->incomplete && r->attempts[0].state==session::AttemptState::finished);
        CHECK(r->attempts[0].marked==!late && r->attempts[0].result_ms.has_value()==late);
    }
    for(bool drop:{false,true}){
        Fake b;Orchestrator o(b);CHECK(o.manual_start(1,9,1)==Decision::accepted && o.acquired(1,2));
        CHECK(o.manual_stop(1,3) && terminal(o,1,State::finalized,drop,drop));
        auto r=o.take(1);CHECK(r && r->incomplete && r->stop_boundary_reached==drop);
    }
    return true;
}
bool failures_and_cancel(){
    Fake b;Orchestrator o(b);b.available=false;
    CHECK(o.prepare(1,9,{10,1},1)==Decision::unavailable && !o.acquired(1,2));
    auto unavailable=o.take(1);CHECK(unavailable && unavailable->incomplete && unavailable->terminal==State::failed);
    b.available=true;CHECK(o.manual_start(2,9,3)==Decision::accepted && o.cancel(2,4));
    CHECK(!o.cancel(2,5) && !o.acquired(2,5) && b.cancels==1);
    CHECK(terminal(o,2,State::cancelled,false));auto cancelled=o.take(2);CHECK(cancelled && cancelled->incomplete);
    CHECK(o.manual_start(3,9,6)==Decision::accepted && o.acquired(3,7));
    CHECK(o.event(3,event(10,1,1,8,K::start)) && terminal(o,3,State::failed,false));
    auto failed=o.take(3);CHECK(failed && failed->incomplete && failed->attempts[0].state==session::AttemptState::racing);
    CHECK(o.manual_start(4,9,9)==Decision::accepted && o.acquired(4,10) && o.manual_stop(4,11));
    CHECK(o.cancel(4,12) && b.last_cancel==4 && terminal(o,4));
    auto late=o.take(4);CHECK(late && late->terminal==State::finalized && late->incomplete);
    CHECK(o.manual_start(5,9,13)==Decision::accepted && o.acquired(5,14));
    CHECK(o.event(5,event(11,1,1,15,K::start)) && o.manual_stop(5,16) && terminal(o,5));
    auto truncated=o.take(5);CHECK(truncated && truncated->incomplete && truncated->attempts[0].state==session::AttemptState::racing);
    CHECK(truncated->ledger.markers()[0].end==capture_boundary::End::abandoned);return true;
}
bool bounded_and_shutdown(){
    Fake b;Orchestrator o(b);
    for(Id i=1;i<=4;++i){CHECK(o.manual_start(i,9,i*10)==Decision::accepted && o.acquired(i,i*10+1));CHECK(o.manual_stop(i,i*10+2));}
    CHECK(o.manual_start(5,9,50)==Decision::busy && b.starts==4);
    CHECK(terminal(o,2) && o.manual_start(5,9,50)==Decision::busy);
    CHECK(o.take(2) && o.manual_start(5,9,50)==Decision::accepted);
    o.shutdown(51);CHECK(b.cancels==4 && o.manual_start(6,9,60)==Decision::rejected);
    for(Id i:{1,3,4,5}){CHECK(terminal(o,i,State::cancelled,false));CHECK(o.take(i)->incomplete);}
    return true;
}
bool marker_bound(){
    Fake b;Orchestrator o(b);CHECK(o.manual_start(1,9,0)==Decision::accepted && o.acquired(1,1));
    for(Id i=1;i<=128;++i){
        CHECK(o.event(1,event(10,i,i*2-1,i*3,K::start)));
        CHECK(o.event(1,event(10,i,i*2,i*3+1,K::restart_race)));
    }
    CHECK(!o.event(1,event(10,129,257,387,K::start)) && b.stops==1 && terminal(o,1));
    auto r=o.take(1);CHECK(r && r->incomplete && r->attempts.size()==128 && r->ledger.markers().size()==128);return true;
}
int main(){
    for(auto f:{automatic_result,restart_and_manual,late_or_missing_boundaries,failures_and_cancel,bounded_and_shutdown,marker_bound})if(!f())return 1;
    std::cout<<"B-REC-01: 6 deterministic scenario groups PASS\n";return 0;
}
