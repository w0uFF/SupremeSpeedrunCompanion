#include "src/modules/recorder/public/recorded_overlay.hpp"
#include <iostream>
#include <set>
using namespace supreme;
using namespace supreme::recorder;
using namespace supreme::recorder::overlay;
using categories::Track;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
Snapshot sample(std::uint64_t seq,Tick time,Track track=Track::alpine_easy){
    Snapshot s;s.capture=5;s.sequence=seq;s.time=time;s.origin=Origin::synthetic;s.session=10;s.attempt=1;
    s.rta_ms=100;s.igt_ms=90;s.recording=Recording::acquiring;s.integrity=Integrity::no_known_anomaly;
    s.actions[0]={{'J','U','M','P'},4,false,std::int16_t{0}};s.action_count=1;s.validated_loaded_track=track;return s;
}
bool catalog_labels(){
    constexpr std::array<std::string_view,9> expected{"Alpine - easy","Alpine - medium","Alpine - hard","Forest - easy","Forest - medium","Forest - hard","Village - easy","Village - medium","Village - hard"};
    History h(5);BitmapRenderer r;std::set<std::vector<std::uint8_t>> bands;
    std::uint64_t seq=0;
    for(const auto& c:categories::catalog){
        if(!c.track){CHECK(c.format!=categories::Format::individual);continue;}
        ++seq;auto s=sample(seq,seq*10,*c.track);CHECK(h.publish(s));
        auto v=h.select(s.time,100);CHECK(v.sample && track_label(*v.sample)==expected.at(static_cast<std::size_t>(*c.track)));
        Canvas canvas(480,128);CHECK(r.draw(canvas,v));
        // Sixth line is the track/difficulty label. Each catalog member must have distinct pixels.
        const auto& pixels=canvas.bytes();bands.emplace(pixels.begin()+41*480*4,pixels.begin()+48*480*4);
    }
    CHECK(seq==9 && bands.size()==9);
    auto s=sample(10,100);s.validated_loaded_track.reset();CHECK(h.publish(s));
    auto v=h.select(100,100);CHECK(v.sample && track_label(*v.sample)=="UNKNOWN" && v.sample->integrity==Integrity::insufficient);
    Canvas c(480,128);CHECK(r.draw(c,v));CHECK(bands.count(std::vector<std::uint8_t>(c.bytes().begin()+41*480*4,c.bytes().begin()+48*480*4))==0);
    s=sample(11,110,static_cast<Track>(99));CHECK(!h.publish(s) && track_label(s)=="UNKNOWN");
    s=sample(11,110);s.origin=Origin::unavailable;CHECK(!h.publish(s));s.origin=Origin::synthetic;s.attempt=s.session=0;CHECK(!h.publish(s));
    return true;
}
bool ordered_ticks(){
    History h(5);auto a=sample(1,100);CHECK(h.publish(a));
    std::array<Snapshot,3> batch{sample(2,200),sample(3,200,Track::forest_hard),sample(4,200,Track::village_medium)};
    batch[1].attempt=2;batch[2].attempt=3;batch[2].rta_ms.reset();batch[2].igt_ms.reset();batch[2].action_count=0;
    CHECK(h.publish_tick(batch.data(),batch.size()));batch[2].attempt=999;
    CHECK(h.select(199,100).sample->attempt==1);
    auto exact=h.select(200,100);CHECK(exact.availability==Availability::ambiguous && !exact.sample);
    auto after=h.select(201,100);CHECK(after.sample && after.sample->sequence==4 && after.sample->attempt==3 && after.sample->validated_loaded_track==Track::village_medium && !after.sample->rta_ms && !after.sample->igt_ms && !after.sample->action_count && after.sample->integrity==Integrity::insufficient);
    CHECK(h.select(300,100).sample && h.select(301,100).availability==Availability::stale);
    CHECK(!h.publish(sample(4,201)) && !h.publish(sample(5,199)));
    CHECK(!h.publish(sample(5,200,Track::forest_easy))); // late group extension quarantines tick
    CHECK(h.select(200,100).availability==Availability::ambiguous && h.select(201,100).availability==Availability::ambiguous && !h.select(201,100).sample);
    CHECK(!h.publish(sample(5,201)) && h.publish(sample(6,202,Track::forest_easy)));
    CHECK(h.select(202,100).sample->validated_loaded_track==Track::forest_easy);
    CHECK(h.select(201,100).availability==Availability::ambiguous);
    return true;
}
bool bounded_and_invalid(){
    History h(5);CHECK(!h.publish_tick(nullptr,1));auto s=sample(1,1);CHECK(!h.publish_tick(&s,0) && !h.publish_tick(&s,History::max_tick_group+1));
    std::array<Snapshot,2> bad{sample(1,1),sample(1,1)};CHECK(!h.publish_tick(bad.data(),2));
    bad[1]=sample(2,2);CHECK(!h.publish_tick(bad.data(),2));bad[1].time=1;bad[1].capture=6;CHECK(!h.publish_tick(bad.data(),2));
    CHECK(h.select(1,100).availability==Availability::missing);
    bad[1].capture=5;CHECK(h.publish_tick(bad.data(),2));
    for(std::uint64_t i=2;i<=History::capacity;++i)CHECK(h.publish(sample(i+1,i)));
    CHECK(h.select(1,1000).availability==Availability::ambiguous);
    CHECK(h.publish(sample(History::capacity+2,History::capacity+1)));
    CHECK(h.select(1,1000).availability==Availability::missing && h.select(2,1000).sample->sequence==3);
    return true;
}
bool integrity_states(){
    History h(5);auto s=sample(1,100);CHECK(h.publish(s));CHECK(h.select(99,100).availability==Availability::missing);
    CHECK(h.select(100,100).sample->integrity==Integrity::no_known_anomaly);
    CHECK(h.select(201,100).availability==Availability::stale && !h.select(201,100).sample);
    for(unsigned i=0;i<8;++i){auto a=sample(i+2,101+i);
        switch(i){case 0:a.validated_loaded_track.reset();break;case 1:a.rta_ms.reset();break;case 2:a.igt_ms.reset();break;case 3:a.action_count=0;break;case 4:a.actions[0].value.reset();break;case 5:a.recording=Recording::unknown;break;case 6:a.recording=Recording::incomplete;break;default:a.origin=Origin::unavailable;a.session=a.attempt=0;a.rta_ms.reset();a.igt_ms.reset();a.action_count=0;a.validated_loaded_track.reset();break;}
        CHECK(h.publish(a));auto v=h.select(a.time,100);CHECK(v.sample && v.sample->integrity==Integrity::insufficient && a.integrity==Integrity::no_known_anomaly);
    }
    for(auto status:{Integrity::unknown,Integrity::insufficient,Integrity::anomaly,Integrity::violation}){s.validated_loaded_track.reset();s.integrity=status;CHECK(visible_integrity(s)==status);}
    // Even if directly passed a stale selection with a payload, renderer must ignore it.
    BitmapRenderer r;Canvas a(480,128),b(480,128),c(480,128),d(480,128);s=sample(1,100);
    CHECK(r.draw(a,{Availability::stale,s}) && r.draw(b,{Availability::stale,{}}) && a.bytes()==b.bytes());
    CHECK(r.draw(c,{Availability::missing,s}) && r.draw(d,{Availability::missing,{}}) && c.bytes()==d.bytes() && a.bytes()!=c.bytes());
    return true;
}
struct Failure final:Renderer {bool draw(Canvas& c,const Selection&) override {c.pixel(0,0,255);throw std::runtime_error("renderer");}};
bool composition(){
    History h(5);auto s=sample(1,100);s.action_count=8;for(std::size_t i=0;i<8;++i)s.actions[i]={{'A',static_cast<char>('0'+i)},2,false,std::int16_t{1}};CHECK(h.publish(s));
    BitmapRenderer r;Layout l;l.enabled=true;l.x=4;l.y=6;l.width=400;l.height=128;l.max_age=100;
    TimedFrame input{100,std::vector<std::uint8_t>(480*160*3/2,99)};const auto original=input.pixels;
    Composer good(h,r,l,480,160);auto result=good.compose(input);CHECK(result.state==ComposeState::rendered && result.frame && result.frame->time==100 && input.pixels==original);
    const auto& p=result.frame->pixels;for(std::size_t y=0;y<160;++y)for(std::size_t x=0;x<480;++x)if(x<4||x>=404||y<6||y>=134)CHECK(p[y*480+x]==99);
    for(std::size_t y=0;y<80;++y)for(std::size_t x=0;x<480;++x)if(x<4||x>=404||y<3||y>=67)CHECK(p[480*160+y*480+x]==99);
    l.height=120;Composer clipped(h,r,l,480,160);CHECK(clipped.compose(input).state==ComposeState::clipped);
    l.height=128;Failure f;Composer fail(h,f,l,480,160);auto failure=fail.compose(input);CHECK(failure.state==ComposeState::failed && !failure.frame && input.pixels==original);
    l.enabled=false;Composer off(h,f,l,480,160);CHECK(off.compose(input).state==ComposeState::disabled && input.pixels==original);
    return true;
}
int main(){for(auto f:{catalog_labels,ordered_ticks,bounded_and_invalid,integrity_states,composition})if(!f())return 1;
    std::cout<<"B-REC-03b: 5 overlay identity scenario groups PASS\n";return 0;}
