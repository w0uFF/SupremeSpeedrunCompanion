#pragma once
#include "contracts/observation.hpp"
#include "src/modules/recorder/public/frame_lookahead.hpp"
#include "src/modules/recorder/public/orchestrator.hpp"
#include <chrono>
#include <memory>
#include <variant>

namespace supreme::recorder::application {
// Source timestamps and Session milliseconds must use the same supplied epoch.
inline std::optional<Id> session_time_100ns(ClockSample epoch,ClockSample sample) noexcept {
    if(epoch.frequency_hz!=sample.frequency_hz)return {};
    return qpc_to_100ns(epoch.ticks,sample.ticks,epoch.frequency_hz);
}
enum class ReadKind { frame, idle, end, error };
struct Read { ReadKind kind; TimedFrame frame{}; };
class FrameSource {
public:
    virtual ~FrameSource()=default;
    // Called on capture lane only; return within the supplied polling budget.
    virtual Read read(std::chrono::milliseconds budget)=0;
};
struct MediaSession {
    std::unique_ptr<FrameSource> source;
    std::unique_ptr<Sink> sink;
    std::size_t frame_bytes=0;
};
class MediaFactory {
public:
    virtual ~MediaFactory()=default;
    // Concurrent lane calls; validate target/epoch and reserve unique output here.
    virtual MediaSession open(const Request&)=0;
};
enum class BackendEnd { stopped, cancelled, startup_error, source_error, source_ended,
                        invalid_frame, timeout, handoff_rejected, worker_failed, internal_error };
struct Acquired { Id capture, ms; };
struct Completion {
    Id capture;
    BackendEnd reason=BackendEnd::startup_error;
    State state=State::failed; // application disposition
    std::optional<State> worker_state;
    Stats stats{};
    std::array<StreamQuality,2> quality{};
    std::size_t high_water=0;
    bool stop_boundary_reached=false;
};
using Notification=std::variant<Acquired,Completion>;
struct BackendLimits {
    std::size_t slots=8, max_packet=2u*1024u*1024u;
    std::chrono::milliseconds ready{5000}, idle{5000}, stop{5000};
    LookaheadPolicy timeline{}; // explicit extended opt-in, legacy default
};
// Offline video adapter. Fixed mailboxes; Worker owns encoding, lanes own sources.
// Construct/join/destroy outside game/UI callbacks; owner methods never join.
class WorkerBackend final:public Backend {
    using Clock=std::chrono::steady_clock;
    using Ms=std::chrono::milliseconds;
    struct Control {
        bool cancel=false;
        std::optional<Id> through;
        Clock::time_point stop_deadline{};
    };
    struct Lane {
        std::optional<Request> request;
        Clock::time_point ready_deadline{};
        Control command;
        std::optional<Acquired> acquired;
        std::optional<Completion> done;
        std::thread thread;
    };
    MediaFactory& factory_;
    const BackendLimits limits_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::array<Lane,2> lanes_{};
    Id highest_=0;
    bool closing_=false;
    static bool terminal(State s) noexcept {
        return s==State::finalized || s==State::cancelled || s==State::failed;
    }
    Control control(Lane& l){std::lock_guard<std::mutex> lock(mutex_);return l.command;}
    void pause(){std::unique_lock<std::mutex> lock(mutex_);changed_.wait_for(lock,Ms(2));}
    void notify_acquired(Lane& l,Acquired a){std::lock_guard<std::mutex> lock(mutex_);l.acquired=a;}
    BackendEnd capture(Lane& l,const Request& r,MediaSession& media,Worker& worker){
        while(worker.state()==State::starting){
            if(control(l).cancel)return BackendEnd::cancelled;
            if(Clock::now()>=l.ready_deadline)return BackendEnd::timeout;
            pause();
        }
        if(worker.state()!=State::running)return BackendEnd::worker_failed;
        if(Clock::now()>=l.ready_deadline)return BackendEnd::timeout;
        // One lane-owned allocation; timeline policy is immutable for this backend.
        auto look=std::make_unique<FrameLookahead>(media.frame_bytes,limits_.timeline);
        auto idle_deadline=Clock::now()+limits_.idle;
        Id last_ms=0;
        for(;;){
            auto c=control(l);
            if(c.cancel)return BackendEnd::cancelled;
            if(worker.state()!=State::running)return BackendEnd::worker_failed;
            const auto now=Clock::now();
            if(now>=idle_deadline || (c.through && now>=c.stop_deadline))return BackendEnd::timeout;
            // Require a frame in a strictly later millisecond than the stop event.
            // Equal truncated milliseconds alone cannot establish ordering.
            if(c.through && look->count()>=2 && last_ms>*c.through){
                return worker.offer(look->finish())==Offer::accepted?BackendEnd::stopped:BackendEnd::handoff_rejected;
            }
            Read next{ReadKind::error,{}};
            try {next=media.source->read(Ms(5));}catch(...){return BackendEnd::source_error;}
            if(control(l).cancel)return BackendEnd::cancelled;
            if(next.kind==ReadKind::idle){pause();continue;}
            if(next.kind==ReadKind::end)return BackendEnd::source_ended;
            if(next.kind!=ReadKind::frame)return BackendEnd::source_error;
            if(Clock::now()>=idle_deadline)return BackendEnd::timeout;
            const auto ms=next.frame.time/10000;
            if(ms<r.requested_ms || next.frame.pixels.capacity()>limits_.max_packet)return BackendEnd::invalid_frame;
            try {
                auto packet=look->push(std::move(next.frame));
                if(packet && worker.offer(std::move(*packet))!=Offer::accepted)return BackendEnd::handoff_rejected;
            }catch(const std::invalid_argument&){return BackendEnd::invalid_frame;}
            last_ms=ms;idle_deadline=Clock::now()+limits_.idle;
            if(look->count()==1)notify_acquired(l,{r.capture,ms});
        }
    }
    Completion execute(Lane& l,const Request& r) noexcept {
        Completion out{};out.capture=r.capture;
        MediaSession media;
        std::unique_ptr<Worker> worker;
        try {
            if(control(l).cancel)out.reason=BackendEnd::cancelled;
            else {
                media=factory_.open(r);
                if(control(l).cancel)out.reason=BackendEnd::cancelled;
                else if(Clock::now()>=l.ready_deadline)out.reason=BackendEnd::timeout;
                else if(media.source && media.sink && media.frame_bytes && media.frame_bytes<=limits_.max_packet){
                    worker=std::make_unique<Worker>(*media.sink,limits_.slots,limits_.max_packet);
                    try {out.reason=capture(l,r,media,*worker);}
                    catch(...){out.reason=BackendEnd::internal_error;}
                }
            }
        }catch(...){out.reason=BackendEnd::startup_error;}
        // Release capture resources before draining; sink must outlive Worker.
        media.source.reset();
        if(worker){
            if(out.reason==BackendEnd::stopped && !control(l).cancel)worker->stop();
            else {if(out.reason==BackendEnd::stopped)out.reason=BackendEnd::cancelled;worker->cancel();}
            while(!terminal(worker->state())){
                if(control(l).cancel){if(out.reason==BackendEnd::stopped)out.reason=BackendEnd::cancelled;worker->cancel();}
                pause();
            }
            worker->join();
            out.worker_state=worker->state();out.stats=worker->stats();out.quality=worker->quality();out.high_water=worker->high_water();
            if(*out.worker_state==State::failed && out.reason==BackendEnd::stopped)out.reason=BackendEnd::worker_failed;
            worker.reset();
        }
        media.sink.reset();
        if(control(l).cancel && out.reason==BackendEnd::stopped)out.reason=BackendEnd::cancelled;
        if(out.reason==BackendEnd::stopped && out.worker_state==State::finalized){
            out.state=State::finalized;out.stop_boundary_reached=true;
        }else if(out.reason==BackendEnd::cancelled){
            out.state=out.worker_state.value_or(State::cancelled);
        }else if(out.reason==BackendEnd::stopped){out.reason=BackendEnd::worker_failed;}
        return out;
    }
    void loop(Lane& l){
        for(;;){
            Request r{};
            {
                std::unique_lock<std::mutex> lock(mutex_);
                changed_.wait(lock,[&]{return closing_ || (l.request && !l.done);});
                if(closing_ && (!l.request || l.done))return;
                r=*l.request;
            }
            auto result=execute(l,r);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                // Linearize cancellation against publishing the terminal mailbox.
                if(l.command.cancel && result.reason==BackendEnd::stopped){
                    result.reason=BackendEnd::cancelled;result.stop_boundary_reached=false;
                }
                l.done=result;
            }
        }
    }
public:
    explicit WorkerBackend(MediaFactory& factory,BackendLimits limits={}):factory_(factory),limits_(limits){
        const auto valid=[](Ms d){return d.count()>0 && d<=Ms(60000);};
        if(!limits.slots || limits.slots>128 || !limits.max_packet || limits.max_packet>16u*1024u*1024u ||
           limits.slots>32u*1024u*1024u/limits.max_packet || !valid(limits.ready) || !valid(limits.idle) || !valid(limits.stop) || !limits.timeline.valid())
            throw std::invalid_argument("backend limits");
        try {for(auto& l:lanes_)l.thread=std::thread([this,p=&l]{loop(*p);});}
        catch(...){join();throw;}
    }
    ~WorkerBackend() override {join();}
    bool start(const Request& r) noexcept override {
        std::lock_guard<std::mutex> lock(mutex_);
        if(closing_ || !r.capture || r.capture<=highest_ || !r.target)return false;
        for(auto& l:lanes_)if(!l.request){
            l.request=r;l.command={};l.ready_deadline=Clock::now()+limits_.ready;highest_=r.capture;
            changed_.notify_all();return true;
        }
        return false;
    }
    void stop(Id capture,Id through_ms) noexcept override {
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto& l:lanes_)if(l.request && l.request->capture==capture && !l.done && !l.command.cancel && !l.command.through && through_ms>=l.request->requested_ms){
            l.command.through=through_ms;l.command.stop_deadline=Clock::now()+limits_.stop;changed_.notify_all();
        }
    }
    void cancel(Id capture) noexcept override {
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto& l:lanes_)if(l.request && l.request->capture==capture && !l.done)l.command.cancel=true;
        changed_.notify_all();
    }
    std::optional<Notification> poll(){
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto& l:lanes_){
            if(l.acquired){Notification n=*l.acquired;l.acquired.reset();return n;}
            if(l.done){Notification n=*l.done;l.done.reset();l.request.reset();return n;}
        }
        return {};
    }
    void shutdown() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);closing_=true;
        for(auto& l:lanes_)if(l.request && !l.done)l.command.cancel=true;
        changed_.notify_all();
    }
    // Blocking lifecycle boundary for composition root only. Terminal mailboxes remain readable.
    void join(){shutdown();for(auto& l:lanes_)if(l.thread.joinable())l.thread.join();}
};
}
