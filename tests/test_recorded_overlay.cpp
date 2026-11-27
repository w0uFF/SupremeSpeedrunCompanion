#include "src/modules/recorder/public/overlay_frame_source.hpp"
#include <atomic>
#include <iostream>
#include <thread>
using namespace supreme;
using namespace supreme::recorder;
using namespace supreme::recorder::overlay;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
Snapshot snap(std::uint64_t seq,Tick time,std::uint64_t attempt=1){
    Snapshot s;s.capture=5;s.sequence=seq;s.time=time;s.origin=Origin::synthetic;s.session=10;s.attempt=attempt;
    s.rta_ms=1234;s.igt_ms=567;s.recording=Recording::acquiring;s.integrity=Integrity::insufficient;
    s.actions[0]={{'T','U','R','N'},4,true,std::int16_t{-1000}};s.actions[1]={{'J','U','M','P'},4,false,std::int16_t{0}};s.action_count=2;return s;
}
TimedFrame frame(Tick t,std::size_t w=480,std::size_t h=160){return {t,std::vector<std::uint8_t>(w*h*3/2,99)};}
Layout layout(){Layout l;l.enabled=true;l.x=4;l.y=6;l.width=400;l.height=128;l.max_age=100;return l;}
struct Spy final:Renderer {
    unsigned calls=0;Selection observed;bool fail=false,fit=true;
    std::thread::id caller;
    bool draw(Canvas& c,const Selection& s) override {++calls;observed=s;caller=std::this_thread::get_id();c.pixel(0,0,255);c.pixel(c.width(),c.height(),255);if(fail)throw std::runtime_error("renderer");return fit;}
};
bool history_alignment(){
    History h(5);auto a=snap(1,100);CHECK(h.publish(a));a.rta_ms=9999;
    CHECK(h.publish(snap(2,200,2)) && h.select(99,100).availability==Availability::missing);
    auto v=h.select(199,100);CHECK(v.sample && v.sample->attempt==1 && v.sample->rta_ms==1234);
    v=h.select(200,100);CHECK(v.sample && v.sample->attempt==2);
    CHECK(h.select(300,100).availability==Availability::current && h.select(301,100).availability==Availability::stale && !h.select(301,100).sample);
    auto transition=snap(3,201,3);transition.rta_ms.reset();transition.igt_ms.reset();transition.action_count=0;
    CHECK(h.publish(transition));v=h.select(201,100);CHECK(v.sample && v.sample->attempt==3 && !v.sample->rta_ms && !v.sample->igt_ms && !v.sample->action_count);
    auto unknown=snap(4,202);unknown.origin=Origin::unavailable;unknown.session=unknown.attempt=0;unknown.rta_ms.reset();unknown.igt_ms.reset();unknown.action_count=0;
    CHECK(h.publish(unknown) && h.select(202,100).sample->origin==Origin::unavailable);
    CHECK(!h.publish(snap(4,203)) && !h.publish(snap(5,202)) && !h.publish(snap(5,201)));
    return true;
}
bool validation_and_capacity(){
    History h(5);auto a=snap(1,100);a.capture=6;CHECK(!h.publish(a));a.capture=5;
    a.actions[0].value=std::int16_t{-1001};CHECK(!h.publish(a));a.actions[0].value=std::int16_t{1000};a.actions[1].value=std::int16_t{2};CHECK(!h.publish(a));
    a.actions[1].value=std::int16_t{0};a.actions[1].label[0]='\n';CHECK(!h.publish(a));a=snap(1,100);a.actions[1]=a.actions[0];CHECK(!h.publish(a));
    a=snap(1,100);a.action_count=9;CHECK(!h.publish(a));a=snap(1,100);a.session=0;CHECK(!h.publish(a));
    a=snap(1,100);a.origin=Origin::unavailable;CHECK(!h.publish(a));
    for(std::uint64_t i=1;i<=History::capacity+2;++i)CHECK(h.publish(snap(i,i)));
    CHECK(h.select(2,1000).availability==Availability::missing && h.select(3,1000).sample->sequence==3);
    CHECK(h.select(258,1000).sample->sequence==258);return true;
}
bool pixels_and_thread(){
    History h(5);CHECK(h.publish(snap(1,100)));Spy spy;auto l=layout();Composer c(h,spy,l,480,160);
    auto input=frame(101);const auto copy=input;Composition result{ComposeState::failed,Availability::missing,{}};
    const auto owner=std::this_thread::get_id();std::thread lane([&]{result=c.compose(input);});lane.join();
    CHECK(spy.caller!=owner && spy.calls==1 && result.state==ComposeState::rendered && result.availability==Availability::current && result.frame);
    CHECK(input.pixels==copy.pixels && input.time==copy.time && result.frame->time==input.time && result.frame->pixels.size()==input.pixels.size());
    const auto& out=result.frame->pixels;constexpr std::size_t w=480,hgt=160;
    for(std::size_t y=0;y<hgt;++y)for(std::size_t x=0;x<w;++x){const bool inside=x>=4 && x<404 && y>=6 && y<134;
        if(!inside)CHECK(out[y*w+x]==99);else CHECK(out[y*w+x]==((x==4 && y==6)?235:16));}
    for(std::size_t y=0;y<hgt/2;++y)for(std::size_t x=0;x<w;++x){const bool inside=x>=4 && x<404 && y>=3 && y<67;CHECK(out[w*hgt+y*w+x]==(inside?128:99));}
    return true;
}
bool stale_and_bitmap(){
    History h(5);BitmapRenderer bitmap;auto l=layout();Composer c(h,bitmap,l,480,160);
    const auto missing=c.compose(frame(99));CHECK(missing.frame && missing.availability==Availability::missing && missing.state==ComposeState::rendered);
    CHECK(h.publish(snap(1,100)));const auto known=c.compose(frame(100));const auto stale=c.compose(frame(201));
    CHECK(known.frame && stale.frame && known.state==ComposeState::rendered && stale.availability==Availability::stale);
    CHECK(known.frame->pixels!=stale.frame->pixels && missing.frame->pixels!=stale.frame->pixels);
    auto next=snap(2,102);next.igt_ms.reset();next.actions[0].value.reset();CHECK(h.publish(next));
    auto unknown=c.compose(frame(102));CHECK(unknown.frame && unknown.frame->pixels!=known.frame->pixels);
    return true;
}
bool clipping_disabled_failure(){
    History h(5);Spy spy;auto l=layout();l.enabled=false;Composer disabled(h,spy,l,480,160);auto input=frame(100);auto original=input.pixels;
    auto d=disabled.compose(input);CHECK(d.state==ComposeState::disabled && !d.frame && !spy.calls && input.pixels==original);
    l.enabled=true;Composer c(h,spy,l,480,160);spy.fail=true;auto f=c.compose(input);CHECK(f.state==ComposeState::failed && !f.frame && input.pixels==original);
    spy.fail=false;auto bad=input;bad.pixels.pop_back();auto invalid=c.compose(bad);CHECK(invalid.state==ComposeState::invalid_frame && !invalid.frame && spy.calls==1);
    l.x=478;l.y=158;Composer clipped(h,spy,l,480,160);auto edge=clipped.compose(input);CHECK(edge.state==ComposeState::clipped && edge.frame);
    CHECK(edge.frame->pixels[157*480+478]==99 && edge.frame->pixels[158*480+477]==99);
    l.x=480;Composer outside(h,spy,l,480,160);CHECK(outside.compose(input).state==ComposeState::clipped && !outside.compose(input).frame);
    BitmapRenderer bitmap;l.x=0;l.y=0;Composer small(h,bitmap,l,16,16);auto small_result=small.compose(frame(1,16,16));CHECK(small_result.state==ComposeState::clipped && small_result.frame);
    CHECK(input.pixels==original);return true;
}
bool backend_composition(){
    using namespace supreme::recorder::application;
    using namespace std::chrono_literals;
    struct Record {
        std::atomic<bool> permit{false};
        std::thread::id lane,encoder;
        unsigned draws=0,aborts=0,finalizes=0;
        std::vector<Packet> packets;
        TimedFrame first=frame(10000000),second=frame(10400000);
    };
    struct Source final:FrameSource {
        Record& r;unsigned count=0;
        explicit Source(Record& record):r(record){}
        Read read(std::chrono::milliseconds budget) override {
            r.lane=std::this_thread::get_id();
            if(!count){++count;return {ReadKind::frame,r.first};}
            if(count==1 && r.permit){++count;return {ReadKind::frame,r.second};}
            std::this_thread::sleep_for(budget);return {ReadKind::idle,{}};
        }
    };
    struct Drawing final:Renderer {
        Record& r;bool fail;
        Drawing(Record& record,bool f):r(record),fail(f){}
        bool draw(Canvas& c,const Selection& s) override {
            if(r.lane!=std::this_thread::get_id() || !s.sample)throw std::runtime_error("wrong lane or sample");
            ++r.draws;c.pixel(0,0,255);
            if(fail && r.draws==2)throw std::runtime_error("injected second-frame failure");
            return true;
        }
    };
    struct Encoding final:Sink {
        Record& r;
        explicit Encoding(Record& record):r(record){}
        void begin() override {r.encoder=std::this_thread::get_id();}
        void write(const Packet& p) override {r.packets.push_back(p);}
        void finalize() override {++r.finalizes;}
        void abort() noexcept override {++r.aborts;}
    };
    struct Factory final:MediaFactory {
        Record r;unsigned mode;
        std::shared_ptr<History> history=std::make_shared<History>(5);
        explicit Factory(unsigned m):mode(m){}
        MediaSession open(const Request& request) override {
            auto l=layout();l.enabled=mode!=0;l.max_age=500000;
            auto source=std::make_unique<OverlayFrameSource>(std::make_unique<Source>(r),history,
                std::make_unique<Drawing>(r,mode==2),request.capture,l,480,160);
            return {std::move(source),std::make_unique<Encoding>(r),480u*160u*3u/2u};
        }
    };
    for(unsigned mode=0;mode<3;++mode){
        Factory factory(mode);CHECK(factory.history->publish(snap(1,10000000)));
        WorkerBackend backend(factory);Orchestrator o(backend);
        auto next=[&](){const auto until=std::chrono::steady_clock::now()+3s;
            while(std::chrono::steady_clock::now()<until){auto n=backend.poll();if(n)return n;std::this_thread::sleep_for(1ms);}return std::optional<Notification>{};};
        CHECK(o.manual_start(5,9,999)==Decision::accepted);
        auto n=next();CHECK(n && std::holds_alternative<Acquired>(*n));const auto a=std::get<Acquired>(*n);
        CHECK(a.capture==5 && a.ms==1000 && o.acquired(a.capture,a.ms));
        CHECK(o.manual_stop(5,1039));factory.r.permit=true;
        n=next();CHECK(n && std::holds_alternative<Completion>(*n));const auto c=std::get<Completion>(*n);
        CHECK(o.completed(c.capture,c.state,c.stats,c.quality,c.stop_boundary_reached));auto report=o.take(5);
        CHECK(report && report->terminal_stats_consistent && report->incomplete==(mode==2));
        const auto& r=factory.r;
        CHECK(r.lane!=std::this_thread::get_id() && r.encoder!=r.lane && r.encoder!=std::this_thread::get_id());
        CHECK(r.first.pixels==frame(0).pixels && r.second.pixels==frame(0).pixels && r.first.time==10000000 && r.second.time==10400000);
        if(mode==2){CHECK(c.reason==BackendEnd::source_error && !c.stop_boundary_reached && r.packets.empty() && r.aborts==1 && !r.finalizes && r.draws==2);}
        else {
            CHECK(c.reason==BackendEnd::stopped && c.stop_boundary_reached && r.packets.size()==2 && r.finalizes==1 && !r.aborts);
            CHECK(r.packets[0].pts==0 && r.packets[0].duration==400000 && r.packets[1].pts==400000 && r.packets[1].duration==fixture_tail);
            for(const auto& p:r.packets)CHECK((p.bytes==frame(0).pixels)==(mode==0));
            CHECK(r.draws==(mode==0?0u:2u));
        }
    }
    return true;
}
int main(){
    for(auto f:{history_alignment,validation_and_capacity,pixels_and_thread,stale_and_bitmap,clipping_disabled_failure,backend_composition})if(!f())return 1;
    std::cout<<"B-REC-03: 6 recorded-overlay scenario groups PASS\n";return 0;
}
