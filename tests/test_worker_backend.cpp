#include "src/modules/recorder/public/worker_backend.hpp"
#include <atomic>
#include <iostream>
using namespace supreme;
using namespace supreme::recorder;
using namespace supreme::recorder::application;
using namespace std::chrono_literals;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
struct Script {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<Read> reads;
    bool fail_open=false,fail_begin=false,fail_write=false,fail_finalize=false;
    bool gate_begin=false,gate_write=false,gate_finalize=false;
    bool release_begin=false,release_write=false,release_finalize=false;
    std::atomic<bool> began{false},writing{false},finalizing{false};
    std::atomic<unsigned> selected{0},idle_after{0},source_destroyed{0},sink_destroyed{0},aborts{0};
    std::vector<FrameInterval> packets; // read only after terminal publication
    void frame(Id ms,std::size_t bytes=2){
        std::lock_guard<std::mutex> lock(mutex);reads.push_back({ReadKind::frame,{ms*10000,std::vector<std::uint8_t>(bytes,7)}});cv.notify_all();
    }
    void signal(ReadKind kind){std::lock_guard<std::mutex> lock(mutex);reads.push_back({kind,{}});cv.notify_all();}
    void release(bool Script::*flag){std::lock_guard<std::mutex> lock(mutex);this->*flag=true;cv.notify_all();}
    void wait_gate(bool Script::*flag){
        std::unique_lock<std::mutex> lock(mutex);
        if(!cv.wait_for(lock,5s,[&]{return this->*flag;}))throw std::runtime_error("test gate timeout");
    }
};
struct Source final:FrameSource {
    Script& s;
    explicit Source(Script& script):s(script){}
    ~Source() override {++s.source_destroyed;}
    Read read(std::chrono::milliseconds budget) override {
        std::unique_lock<std::mutex> lock(s.mutex);
        if(!s.cv.wait_for(lock,budget,[&]{return !s.reads.empty();})){s.idle_after=s.selected.load();return {ReadKind::idle,{}};}
        auto r=std::move(s.reads.front());s.reads.pop_front();if(r.kind==ReadKind::frame)++s.selected;return r;
    }
};
struct TestSink final:Sink {
    Script& s;
    explicit TestSink(Script& script):s(script){}
    ~TestSink() override {++s.sink_destroyed;}
    void begin() override {s.began=true;if(s.gate_begin)s.wait_gate(&Script::release_begin);if(s.fail_begin)throw std::runtime_error("begin");}
    void write(const Packet& p) override {
        s.writing=true;if(s.gate_write)s.wait_gate(&Script::release_write);
        if(s.fail_write)throw std::runtime_error("write");
        s.packets.push_back({p.pts,p.duration});
    }
    void finalize() override {s.finalizing=true;if(s.gate_finalize)s.wait_gate(&Script::release_finalize);if(s.fail_finalize)throw std::runtime_error("finalize");}
    void abort() noexcept override {++s.aborts;}
};
struct Factory final:MediaFactory {
    std::array<Script,8> scripts;
    MediaSession open(const Request& r) override {
        auto& s=scripts.at(static_cast<std::size_t>(r.capture));
        if(s.fail_open || r.target!=9)throw std::runtime_error("unavailable target");
        return {std::make_unique<Source>(s),std::make_unique<TestSink>(s),2};
    }
};
template<class P> bool wait_for(P pred){
    auto deadline=std::chrono::steady_clock::now()+3s;
    while(std::chrono::steady_clock::now()<deadline){if(pred())return true;std::this_thread::sleep_for(1ms);}
    return pred();
}
std::optional<Notification> next(WorkerBackend& b){
    std::optional<Notification> n;wait_for([&]{n=b.poll();return n.has_value();});return n;
}
std::optional<Completion> done(WorkerBackend& b){
    for(unsigned i=0;i<2;++i){auto n=next(b);if(!n)return {};if(auto* c=std::get_if<Completion>(&*n))return *c;}return {};
}
bool acquired(WorkerBackend& b,Orchestrator& o,Id id,Id ms){
    auto n=next(b);if(!n)return false;auto* a=std::get_if<Acquired>(&*n);
    return a && a->capture==id && a->ms==ms && o.acquired(id,ms);
}
Request request(Id id,Id ms=100){return {id,9,Mode::manual,{},ms};}
session::Event race(Id seq,Id ms,session::EventKind kind){
    return {10,seq,1,ms,kind,categories::Track::alpine_easy,kind==session::EventKind::finish?std::optional<Id>{123}:std::nullopt};
}
bool automatic_and_overlap(){
    Factory f;auto& a=f.scripts[1];auto& b=f.scripts[2];a.gate_finalize=true;
    WorkerBackend backend(f);Orchestrator o(backend);
    CHECK(o.prepare(1,9,{10,1},100)==Decision::accepted);a.frame(101);
    CHECK(acquired(backend,o,1,101));
    CHECK(o.event(1,race(1,110,session::EventKind::start)));
    CHECK(o.event(1,race(2,150,session::EventKind::finish)) && !a.finalizing);
    CHECK(o.result_visible(1,{10,1},160));a.frame(160);
    CHECK(wait_for([&]{return a.idle_after==2;}) && !a.finalizing && !backend.poll());
    a.frame(161);CHECK(wait_for([&]{return a.finalizing.load();}) && a.source_destroyed==1 && a.sink_destroyed==0);
    CHECK(o.manual_start(2,9,162)==Decision::accepted);b.frame(163);CHECK(acquired(backend,o,2,163));
    CHECK(o.manual_stop(2,164));b.frame(165);
    auto second=done(backend);CHECK(second && second->capture==2 && second->stop_boundary_reached && second->stats.written==2);
    CHECK(second->worker_state==State::finalized && b.source_destroyed==1 && b.sink_destroyed==1);
    CHECK(o.completed(2,second->state,second->stats,second->quality,second->stop_boundary_reached));
    auto manual=o.take(2);CHECK(manual && !manual->incomplete && manual->terminal_stats_consistent);
    CHECK(!backend.poll());a.release(&Script::release_finalize);
    auto first=done(backend);CHECK(first && first->capture==1 && first->reason==BackendEnd::stopped && first->worker_state==State::finalized);
    CHECK(first->stop_boundary_reached && first->stats.accepted==3 && first->stats.written==3 && first->quality[0].written.duration==1000000);
    CHECK(a.packets.size()==3 && a.packets[0].pts==0 && a.packets[0].duration==590000 && a.packets[1].duration==10000 && a.packets[2].pts==600000 && a.packets[2].duration==fixture_tail);
    CHECK(a.source_destroyed==1 && a.sink_destroyed==1 && a.aborts==0);
    CHECK(o.completed(1,first->state,first->stats,first->quality,first->stop_boundary_reached));auto report=o.take(1);
    CHECK(report && !report->incomplete && report->terminal_stats_consistent && report->attempts[0].result_ms==160 && !backend.poll());return true;
}
bool start_and_source_failures(){
    for(unsigned mode=0;mode<7;++mode){
        Factory f;auto& s=f.scripts[1];s.fail_open=mode==0;s.fail_begin=mode==1;
        WorkerBackend b(f);CHECK(b.start(request(1)));
        if(mode==2)s.signal(ReadKind::error);
        if(mode==3)s.signal(ReadKind::end);
        if(mode==4)s.frame(99);
        if(mode==5){s.frame(101);s.frame(101);}
        if(mode==6){s.frame(101);s.frame(90102);}
        auto c=done(b);CHECK(c && c->capture==1 && c->state==State::failed && !c->stop_boundary_reached && !b.poll());
        const std::array<BackendEnd,7> reasons{BackendEnd::startup_error,BackendEnd::worker_failed,BackendEnd::source_error,BackendEnd::source_ended,BackendEnd::invalid_frame,BackendEnd::invalid_frame,BackendEnd::invalid_frame};
        CHECK(c->reason==reasons[mode]);
        if(mode==0)CHECK(!c->worker_state && s.source_destroyed==0 && s.sink_destroyed==0);
        else {CHECK(s.source_destroyed==1 && s.sink_destroyed==1 && s.aborts==1);CHECK(c->worker_state==(mode==1?State::failed:State::cancelled));}
    }
    return true;
}
bool timeout_and_sink_failures(){
    {
        Factory f;auto& s=f.scripts[1];s.gate_begin=true;
        BackendLimits limits;limits.ready=100ms;
        WorkerBackend b(f,limits);CHECK(b.start(request(1)));
        CHECK(wait_for([&]{return s.began.load();}));
        CHECK(wait_for([&]{return s.source_destroyed==1;}) && !b.poll());
        s.release(&Script::release_begin);auto c=done(b);
        CHECK(c && c->reason==BackendEnd::timeout && c->worker_state==State::cancelled && c->state==State::failed);
        CHECK(s.selected==0 && s.sink_destroyed==1 && s.aborts==1);
    }
    for(unsigned mode=0;mode<4;++mode){
        Factory f;auto& s=f.scripts[1];s.fail_write=mode==2;s.fail_finalize=mode==3;
        BackendLimits limits;limits.idle=mode==0?30ms:1000ms;limits.stop=mode==1?30ms:1000ms;
        WorkerBackend b(f,limits);CHECK(b.start(request(1)));
        if(mode!=0){s.frame(101);CHECK(next(b).has_value());b.stop(1,110);if(mode>=2)s.frame(111);}
        auto c=done(b);CHECK(c && c->state==State::failed && !c->stop_boundary_reached && s.source_destroyed==1 && s.sink_destroyed==1);
        if(mode<2)CHECK(c->reason==BackendEnd::timeout && c->worker_state==State::cancelled);
        else CHECK(c->worker_state==State::failed && (c->reason==BackendEnd::worker_failed || c->reason==BackendEnd::handoff_rejected));
        CHECK(c->stats.accepted==c->stats.written+c->stats.discarded && s.aborts==1);
    }
    return true;
}
bool cancel_start_write_finalize(){
    for(unsigned mode=0;mode<3;++mode){
        Factory f;auto& s=f.scripts[1];s.gate_begin=mode==0;s.gate_write=mode==1;s.gate_finalize=mode==2;
        WorkerBackend b(f);CHECK(b.start(request(1)));
        if(mode==0)CHECK(wait_for([&]{return s.began.load();}));
        else {s.frame(101);CHECK(next(b).has_value());if(mode==2)b.stop(1,110);s.frame(111);
            CHECK(wait_for([&]{return mode==1?s.writing.load():s.finalizing.load();}));}
        b.cancel(1);b.cancel(1);b.stop(1,112);
        if(mode==0)s.release(&Script::release_begin);
        if(mode==1){CHECK(wait_for([&]{return s.source_destroyed==1;}));s.release(&Script::release_write);}
        if(mode==2)s.release(&Script::release_finalize);
        auto c=done(b);CHECK(c && c->reason==BackendEnd::cancelled && !c->stop_boundary_reached && !b.poll());
        CHECK(c->worker_state==(mode==2?State::finalized:State::cancelled) && c->state==*c->worker_state);
        CHECK(s.source_destroyed==1 && s.sink_destroyed==1 && s.aborts==(mode==2?0u:1u));
    }
    return true;
}
bool overflow(){
    Factory f;auto& s=f.scripts[1];s.gate_write=true;BackendLimits limits;limits.slots=1;limits.max_packet=2;
    WorkerBackend b(f,limits);CHECK(b.start(request(1)));s.frame(101);CHECK(next(b).has_value());s.frame(102);
    CHECK(wait_for([&]{return s.writing.load();}));s.frame(103);s.frame(104);
    CHECK(wait_for([&]{return s.source_destroyed==1;}));s.release(&Script::release_write);
    auto c=done(b);CHECK(c && c->reason==BackendEnd::handoff_rejected && c->state==State::failed && c->worker_state==State::cancelled);
    CHECK(c->stats.accepted==2 && c->stats.dropped_full==1 && c->stats.written+c->stats.discarded==2 && c->high_water==1);
    CHECK(c->quality[0].dropped_full==1 && c->quality[0].dropped_duration==10000 && !c->stop_boundary_reached);return true;
}
bool capacity_shutdown_and_clock(){
    CHECK(session_time_100ns({100,1000},{125,1000})==250000);
    CHECK(!session_time_100ns({100,1000},{99,1000}) && !session_time_100ns({100,1000},{125,1001}));
    Factory f;WorkerBackend b(f);
    CHECK(b.start(request(1)) && b.start(request(2)) && !b.start(request(3)) && !b.start(request(1)));
    CHECK(wait_for([&]{return f.scripts[1].began && f.scripts[2].began;}));
    b.cancel(99);b.stop(99,100);b.cancel(1);
    CHECK(wait_for([&]{return f.scripts[1].sink_destroyed==1;}));
    // Withhold owner polling: cleanup must not free the terminal mailbox early.
    CHECK(!b.start(request(3)));
    auto c=done(b);CHECK(c && c->capture==1 && c->reason==BackendEnd::cancelled && !b.poll());
    CHECK(!b.start(request(1)) && b.start(request(3)));
    b.shutdown();CHECK(!b.start(request(4)));b.join();
    unsigned mask=0;while(auto n=b.poll())if(auto* p=std::get_if<Completion>(&*n)){CHECK(p->reason==BackendEnd::cancelled && (p->capture==2 || p->capture==3));const auto bit=1u<<p->capture;CHECK(!(mask&bit));mask|=bit;}
    CHECK(mask==((1u<<2)|(1u<<3)) && !b.poll());
    try {BackendLimits bad;bad.slots=128;WorkerBackend invalid(f,bad);return false;}catch(const std::invalid_argument&){}
    return true;
}
bool terminal_coherence(){
    struct Port final:Backend {
        bool start(const Request&) noexcept override{return true;}
        void stop(Id,Id) noexcept override{}
        void cancel(Id) noexcept override{}
    } port;
    Orchestrator o(port);
    for(Id mode=0;mode<7;++mode){
        const Id id=mode+1;
        CHECK(o.manual_start(id,9,1)==Decision::accepted && o.acquired(id,2) && o.manual_stop(id,3));
        Stats stats{1,1,0,0,0};std::array<StreamQuality,2> q{};
        q[0].accepted=1;q[0].written.observe(0,10);
        if(mode==1){stats.accepted=2;stats.written=2;}
        if(mode==2)q[0].accepted=2;
        if(mode==3)q[0].written.duration=11;
        if(mode==4)stats.rejected_invalid=1; // unknown stream invalid is aggregate only
        if(mode==5)q[1].rejected_invalid=1;
        if(mode==6)q[0].dropped_full=1;
        CHECK(o.completed(id,State::finalized,stats,q,true));
        auto r=o.take(id);CHECK(r && r->terminal_stats_consistent==(mode==0 || mode==4));
        CHECK(r->incomplete==(mode!=0) && r->terminal==State::finalized);
    }
    return true;
}
int main(){
    for(auto f:{automatic_and_overlap,start_and_source_failures,timeout_and_sink_failures,cancel_start_write_finalize,overflow,capacity_shutdown_and_clock,terminal_coherence})if(!f())return 1;
    std::cout<<"B-REC-02: 7 Worker-backed scenario groups PASS\n";return 0;
}
