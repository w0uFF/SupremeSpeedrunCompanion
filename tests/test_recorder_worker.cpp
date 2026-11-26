#include "src/modules/recorder/public/frame_lookahead.hpp"
#include "src/modules/recorder/public/worker.hpp"
#include <future>
#include <chrono>
#include <iostream>
using namespace supreme::recorder;
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
struct GateSink final:Sink {
    std::promise<void> entered, release;
    std::shared_future<void> permit=release.get_future().share();
    bool fail=false, fail_begin=false, fail_finalize=false, finalized=false, aborted=false;
    std::vector<std::uint64_t> seen;
    void begin() override {if(fail_begin)throw std::runtime_error("begin fixture");}
    void write(const Packet& p) override {
        if(seen.empty()){entered.set_value();permit.wait();}
        if(fail)throw std::runtime_error("fixture");
        seen.push_back(p.pts);
    }
    void finalize() override {if(fail_finalize)throw std::runtime_error("finalize fixture");finalized=true;}
    void abort() noexcept override {aborted=true;}
};
struct FinalizeGateSink final:Sink {
    std::promise<void> entered, release;
    std::shared_future<void> permit=release.get_future().share();
    unsigned writes=0, finalizes=0, aborts=0;
    bool fail=false;
    void begin() override {}
    void write(const Packet&) override {++writes;}
    void finalize() override {++finalizes;entered.set_value();permit.wait();if(fail)throw std::runtime_error("late finalize fixture");}
    void abort() noexcept override {++aborts;}
};
Packet video(std::uint64_t pts){return {Stream::video,pts,10,{1,2}};}
int main(){
    {
        GateSink s; Worker w(s,1,16); CHECK(w.wait_ready()==State::running);
        auto first=w.offer(video(0)); if(first!=Offer::accepted){s.release.set_value();return 1;}
        s.entered.get_future().wait();
        const auto second=w.offer(video(10)), full=w.offer(video(20)), stale=w.offer(video(19));
        w.stop();const auto closed=w.offer(video(30));s.release.set_value();w.join();
        CHECK(second==Offer::accepted && full==Offer::full && stale==Offer::invalid && closed==Offer::closed);
        CHECK(w.state()==State::finalized && s.finalized && !s.aborted);
        const auto n=w.stats();CHECK(n.accepted==2 && n.written==2 && n.dropped_full==1 && n.rejected_invalid==1 && n.discarded==0);
        CHECK(s.seen==std::vector<std::uint64_t>({0,10}));
        const auto q=w.quality();
        CHECK(q[0].accepted==2 && q[0].written.packets==2 && q[0].written.duration==20);
        CHECK(q[0].written.internal_gap()==0 && q[0].dropped_full==1 && q[0].dropped_duration==10);
        CHECK(q[0].rejected_invalid==1 && !q[1].written.internal_gap() && w.high_water()==1);
    }
    for(bool failure:{false,true}){
        GateSink s;s.fail=failure;Worker w(s,2,16);CHECK(w.wait_ready()==State::running);
        auto first=w.offer(video(0));if(first!=Offer::accepted){s.release.set_value();return 1;}
        s.entered.get_future().wait();const auto second=w.offer({Stream::audio,0,10,{3}});
        if(failure)w.stop();else w.cancel();s.release.set_value();w.join();
        CHECK(second==Offer::accepted && s.aborted && !s.finalized);
        CHECK(w.state()==(failure?State::failed:State::cancelled));
        const auto n=w.stats();CHECK(n.accepted==2 && n.discarded==(failure?2u:1u) && n.written==(failure?0u:1u));
        const auto q=w.quality();CHECK(q[0].discarded==(failure?1u:0u) && q[1].discarded==1);
    }
    {
        GateSink s;s.fail_begin=true;Worker w(s,1,16);w.join();
        CHECK(w.state()==State::failed && s.aborted && !s.finalized);
        CHECK(w.offer(video(0))==Offer::closed && w.stats().accepted==0);
    }
    {
        GateSink s;s.fail_finalize=true;Worker w(s,1,16);CHECK(w.wait_ready()==State::running);
        const auto first=w.offer(video(0));if(first!=Offer::accepted){s.release.set_value();return 1;}
        s.entered.get_future().wait();w.stop();s.release.set_value();w.join();
        CHECK(w.state()==State::failed && s.aborted && !s.finalized);
        CHECK(w.stats().written==1 && w.stats().discarded==0 && w.offer(video(10))==Offer::closed);
    }
    {
        GateSink s;Worker w(s,1,32768);FrameLookahead look;
        CHECK(w.wait_ready()==State::running);
        auto frame=[](std::uint64_t t){return TimedFrame{t,std::vector<std::uint8_t>(fixture_bytes,16)};};
        CHECK(!look.push(frame(0)));
        auto packet=look.push(frame(400000));
        const auto first=w.offer(std::move(*packet));
        if(first!=Offer::accepted){s.release.set_value();return 1;}
        s.entered.get_future().wait();
        const auto second=w.offer(std::move(*look.push(frame(800000))));
        const auto full=w.offer(std::move(*look.push(frame(1200000))));
        w.cancel();s.release.set_value();w.join();
        CHECK(second==Offer::accepted && full==Offer::full);
        CHECK(w.state()==State::cancelled && s.aborted && !s.finalized);
        const auto n=w.stats();const auto q=w.quality()[0];
        CHECK(n.accepted==2 && n.written==1 && n.dropped_full==1 && n.discarded==1);
        CHECK(q.dropped_duration==400000 && q.discarded==1 && w.high_water()==1);
        CHECK(w.offer(look.finish())==Offer::closed);
    }
    // Once the sink has entered finalization, cancellation cannot retract it.
    // Release the gate before assertions, including on timeout, to avoid deadlock.
    for(bool fail:{false,true}){
        FinalizeGateSink s;s.fail=fail;Worker w(s,2,16);
        const auto ready=w.wait_ready();const auto offered=w.offer(video(0));w.stop();
        const auto entered=s.entered.get_future().wait_for(std::chrono::seconds(5));
        const auto before=w.state();w.cancel();w.stop();const auto after=w.state();
        const auto closed=w.offer(video(10));s.release.set_value();w.join();
        CHECK(ready==State::running && offered==Offer::accepted && entered==std::future_status::ready);
        CHECK(before==State::finalizing && after==State::finalizing && closed==Offer::closed);
        const auto terminal=w.state();w.cancel();w.stop();w.join();
        CHECK(terminal==(fail?State::failed:State::finalized) && w.state()==terminal);
        CHECK(s.writes==1 && s.finalizes==1 && s.aborts==(fail?1u:0u));
        const auto n=w.stats();CHECK(n.accepted==1 && n.written==1 && !n.discarded && !n.dropped_full && !n.rejected_invalid);
    }
    try {GateSink s;Worker w(s,128,16u*1024u*1024u);return 1;}catch(const std::invalid_argument&){}
    return 0;
}
