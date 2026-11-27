#include "src/modules/recorder/public/frame_lookahead.hpp"
#include "src/modules/recorder/public/owned_video_sink.hpp"
#include "src/modules/recorder/public/worker_backend.hpp"
#include <atomic>
#include <iostream>
using namespace supreme::recorder;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
template<class F> bool throws(F f){try{f();return false;}catch(const std::exception&){return true;}}
TimedFrame frame(std::uint64_t t){return {t,{1,2}};}
bool timeline(){
    auto look=std::make_unique<FrameLookahead>(2,LookaheadPolicy::extended());
    CHECK(!look->push(frame(100)));
    for(std::uint64_t i=1;i<=90001;++i){
        auto p=look->push(frame(100+i*400000));
        CHECK(p && p->pts==(i-1)*400000 && p->duration==400000 && p->bytes.size()==2);
    }
    auto p=look->finish();CHECK(look->count()==90002 && p.pts==90001ull*400000 && p.duration==fixture_tail);
    CHECK(throws([&]{look->finish();}) && throws([&]{look->push(frame(0));}));
    auto fixture=std::make_unique<FrameLookahead>(2);
    CHECK(!fixture->push(frame(0)));CHECK(throws([&]{fixture->push(frame(FrameLookahead::max_span+1));}));
    for(std::size_t i=1;i<FrameLookahead::max_frames;++i)CHECK(fixture->push(frame(i)).has_value());
    CHECK(throws([&]{fixture->push(frame(FrameLookahead::max_frames));}));
    CHECK(fixture->finish().pts==FrameLookahead::max_frames-1);
    return true;
}
bool limits(){
    CHECK(throws([]{FrameLookahead bad(2,{1,10,10,1});}));
    CHECK(throws([]{FrameLookahead bad(2,{2,0,10,1});}));
    CHECK(throws([]{FrameLookahead bad(2,{2,10,10,11});}));
    CHECK(throws([]{FrameLookahead bad(2,{LookaheadPolicy::ceiling_frames+1,10,10,1});}));
    CHECK(throws([]{FrameLookahead bad(2,{2,LookaheadPolicy::ceiling_span+1,10,1});}));
    auto look=std::make_unique<FrameLookahead>(2,LookaheadPolicy{3,200,10,4});
    CHECK(!look->push(frame(1000)) && throws([&]{look->finish();}));
    CHECK(throws([&]{look->push(frame(1000));}));
    auto p=look->push(frame(1100));CHECK(p && p->pts==0 && p->duration==10);
    CHECK(throws([&]{look->push(frame(1201));}));
    p=look->push(frame(1200));CHECK(p && p->pts==100 && p->duration==10);
    CHECK(throws([&]{look->push(frame(1201));}));
    auto end=look->finish();CHECK(end.pts==200 && end.duration==4);return true;
}
struct Record {
    std::uint64_t bytes=0;
    unsigned reserve=0,begin=0,write=0,finalize=0,abort=0,seal=0,incomplete=0,writer_destroy=0,lease_destroy=0;
    bool wrong_thread=false,wrong_order=false;
    std::thread::id owner;
    std::vector<FrameInterval> packets;
    void thread(){if(owner!=std::this_thread::get_id())wrong_thread=true;}
};
enum class Fault { none, collision, mismatch, make, begin, write, finalize, seal, budget, final_budget, size };
struct Writer final:Sink {
    Record& r;Fault fault;
    Writer(Record& record,Fault f):r(record),fault(f){}
    ~Writer() override {r.thread();++r.writer_destroy;}
    void begin() override {r.thread();++r.begin;r.bytes=1;if(fault==Fault::begin)throw std::runtime_error("begin");}
    void write(const Packet& p) override {r.thread();++r.write;if(fault==Fault::write)throw std::runtime_error("write");r.packets.push_back({p.pts,p.duration});r.bytes+=fault==Fault::budget?100:2;}
    void finalize() override {r.thread();++r.finalize;r.bytes+=fault==Fault::final_budget?100:1;if(fault==Fault::finalize)throw std::runtime_error("finalize");}
    void abort() noexcept override {r.thread();++r.abort;}
};
struct Lease final:OutputLease {
    Record& r;Fault fault;OutputIdentity id;
    Lease(Record& record,Fault f,OutputIdentity i):r(record),fault(f),id(std::move(i)){}
    ~Lease() override {r.thread();++r.lease_destroy;}
    const OutputIdentity& identity() const noexcept override{return id;}
    std::unique_ptr<Sink> make_writer(const VideoFormat& v) override {r.thread();if(v.frame_bytes()!=6)throw std::runtime_error("format");if(fault==Fault::make)throw std::runtime_error("make");return std::make_unique<Writer>(r,fault);}
    std::uint64_t size_bytes() override {r.thread();if(fault==Fault::size)throw std::runtime_error("size");return r.bytes;}
    void seal() override {r.thread();if(r.writer_destroy!=1)r.wrong_order=true;if(fault==Fault::seal)throw std::runtime_error("seal");++r.seal;}
    void incomplete() noexcept override {r.thread();if(r.begin && r.writer_destroy!=1)r.wrong_order=true;++r.incomplete;}
};
struct Provider final:OutputProvider {
    Record r;Fault fault=Fault::none;
    std::unique_ptr<OutputLease> reserve(const OutputIdentity& id) override {
        r.owner=std::this_thread::get_id();++r.reserve;
        if(fault==Fault::collision)throw std::runtime_error("collision");
        auto copy=id;if(fault==Fault::mismatch)++copy.capture;
        return std::make_unique<Lease>(r,fault,std::move(copy));
    }
};
OutputIdentity identity(){return {1,"0123456789abcdef0123456789abcdef",10};}
constexpr VideoFormat format{2,2,25,500000};
Packet packet(std::uint64_t pts=0,std::uint64_t duration=10){return {Stream::video,pts,duration,{1,2,3,4,5,6}};}
bool success(){
    Provider p;auto id=identity();OwnedVideoSink sink(p,id,format);id.token.clear();
    CHECK(sink.identity()==identity());
    Worker w(sink,4,16);CHECK(w.wait_ready()==State::running);
    CHECK(w.offer(packet())==Offer::accepted && w.offer(packet(50,7))==Offer::accepted);
    w.stop();w.join();CHECK(w.state()==State::finalized && w.stats().written==2);
    CHECK(p.r.reserve==1 && p.r.begin==1 && p.r.write==2 && p.r.finalize==1 && p.r.seal==1 && !p.r.incomplete && !p.r.abort);
    CHECK(p.r.writer_destroy==1 && p.r.lease_destroy==1 && !p.r.wrong_thread && !p.r.wrong_order);
    CHECK(p.r.packets.size()==2 && p.r.packets[1].pts==50 && p.r.packets[1].duration==7);
    CHECK(w.quality()[0].written.internal_gap()==40);return true;
}
bool failures(){
    for(auto fault:{Fault::collision,Fault::mismatch,Fault::make,Fault::begin,Fault::write,Fault::finalize,Fault::seal,Fault::budget,Fault::final_budget,Fault::size}){
        Provider p;p.fault=fault;OwnedVideoSink sink(p,identity(),format);Worker w(sink,4,16);
        if(w.wait_ready()==State::running){CHECK(w.offer(packet())==Offer::accepted);w.stop();}
        w.join();CHECK(w.state()==State::failed && !p.r.seal && !p.r.wrong_thread && !p.r.wrong_order);
        CHECK(w.stats().accepted==w.stats().written+w.stats().discarded);
        CHECK(p.r.lease_destroy==(fault==Fault::collision?0u:1u));
        CHECK(p.r.incomplete==((fault==Fault::collision || fault==Fault::mismatch)?0u:1u));
        const bool made=fault!=Fault::collision && fault!=Fault::mismatch && fault!=Fault::make;
        CHECK(p.r.writer_destroy==(made?1u:0u));
        CHECK(p.r.abort==(made && fault!=Fault::seal && fault!=Fault::final_budget?1u:0u));
    }
    return true;
}
bool cancel_and_invalid(){
    for(unsigned mode=0;mode<5;++mode){
        Provider p;OwnedVideoSink sink(p,identity(),format);Worker w(sink,4,16);CHECK(w.wait_ready()==State::running);
        if(mode==0)w.cancel();
        else if(mode==1)w.stop(); // empty output is incomplete
        else {auto bad=packet();if(mode==2)bad.stream=Stream::audio;if(mode==3)bad.bytes.pop_back();if(mode==4)bad.pts=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());CHECK(w.offer(std::move(bad))==Offer::accepted);w.stop();}
        w.join();CHECK(w.state()==(mode==0?State::cancelled:State::failed));
        CHECK(p.r.incomplete==1 && p.r.abort==1 && !p.r.seal && !p.r.write && !p.r.wrong_thread && !p.r.wrong_order);
    }
    Provider p;auto id=identity();id.token="../escape";CHECK(throws([&]{OwnedVideoSink invalid(p,id,format);}));
    CHECK(throws([&]{OwnedVideoSink invalid(p,identity(),{3,2,25,1});}));
    CHECK(!p.r.reserve);return true;
}
bool backend_policy(){
    using namespace supreme::recorder::application;
    using namespace std::chrono_literals;
    struct Source final:FrameSource {
        std::atomic<bool>& permit;unsigned count=0;
        explicit Source(std::atomic<bool>& p):permit(p){}
        Read read(std::chrono::milliseconds budget) override {
            if(!count){++count;return {ReadKind::frame,{1000ull*10000,{1,2,3,4,5,6}}};}
            if(count==1 && permit){++count;return {ReadKind::frame,{92000ull*10000,{1,2,3,4,5,6}}};}
            std::this_thread::sleep_for(budget);return {ReadKind::idle,{}};
        }
    };
    struct Factory final:MediaFactory {
        Provider provider;std::atomic<bool> permit{false};
        MediaSession open(const Request& r) override {
            auto id=identity();id.capture=r.capture;
            return {std::make_unique<Source>(permit),std::make_unique<OwnedVideoSink>(provider,id,format),format.frame_bytes()};
        }
    };
    for(bool extended:{false,true}){
        Factory factory;BackendLimits l;if(extended)l.timeline=LookaheadPolicy::extended();
        WorkerBackend b(factory,l);Orchestrator o(b);
        auto next=[&](){std::optional<Notification> n;const auto deadline=std::chrono::steady_clock::now()+3s;
            while(std::chrono::steady_clock::now()<deadline){n=b.poll();if(n)return n;std::this_thread::sleep_for(1ms);}return n;};
        CHECK(o.manual_start(1,9,900)==Decision::accepted);
        auto n=next();CHECK(n && std::holds_alternative<Acquired>(*n));
        const auto a=std::get<Acquired>(*n);CHECK(a.capture==1 && a.ms==1000 && o.acquired(a.capture,a.ms));
        CHECK(o.manual_stop(1,91999));factory.permit=true;
        n=next();CHECK(n && std::holds_alternative<Completion>(*n));const auto c=std::get<Completion>(*n);
        CHECK(c.capture==1 && c.stop_boundary_reached==extended);
        CHECK(c.reason==(extended?BackendEnd::stopped:BackendEnd::invalid_frame));
        CHECK(o.completed(c.capture,c.state,c.stats,c.quality,c.stop_boundary_reached));
        auto report=o.take(1);CHECK(report && report->incomplete==!extended && report->terminal_stats_consistent);
        CHECK(factory.provider.r.seal==(extended?1u:0u) && factory.provider.r.incomplete==(extended?0u:1u));
        CHECK(!factory.provider.r.wrong_thread && !factory.provider.r.wrong_order);
        if(extended)CHECK(c.stats.written==2 && c.quality[0].written.internal_gap()==909000000);
        else CHECK(c.stats.written==0 && c.worker_state==State::cancelled);
    }
    Factory factory;BackendLimits invalid;invalid.timeline.frame_limit=0;
    CHECK(throws([&]{WorkerBackend b(factory,invalid);}));return true;
}
int main(){
    for(auto f:{timeline,limits,success,failures,cancel_and_invalid,backend_policy})if(!f())return 1;
    std::cout<<"B-REC-02b: 6 media primitive scenario groups PASS\n";return 0;
}
