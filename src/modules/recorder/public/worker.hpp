#pragma once
#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
namespace supreme::recorder {
enum class Stream { video, audio };
struct Packet { Stream stream; std::uint64_t pts, duration; std::vector<std::uint8_t> bytes; }; // 100 ns units, common origin
class Sink {
public:
    virtual ~Sink()=default;
    virtual void begin()=0;
    virtual void write(const Packet&)=0;
    virtual void finalize()=0;
    virtual void abort() noexcept=0;
};
enum class State { starting, running, draining, cancelling, finalizing, finalized, cancelled, failed };
enum class Offer { accepted, full, invalid, closed };
struct Stats { std::uint64_t accepted=0, written=0, dropped_full=0, rejected_invalid=0, discarded=0; };
// One owner controls lifecycle; producers may offer concurrently. Sink is worker-thread only.
class Worker {
    Sink& sink_;
    const std::size_t slots_, max_packet_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<Packet> queue_;
    std::array<std::uint64_t,2> end_{};
    State state_=State::starting;
    Stats stats_;
    std::thread thread_;
    void run() noexcept {
        try {
            sink_.begin();
            std::unique_lock<std::mutex> lock(mutex_);
            if(state_==State::starting) state_=State::running;
            changed_.notify_all();
            for(;;) {
                changed_.wait(lock,[this]{return state_!=State::running || !queue_.empty();});
                if(state_==State::cancelling) {
                    lock.unlock(); sink_.abort(); lock.lock(); state_=State::cancelled; break;
                }
                if(queue_.empty() && state_==State::draining) {
                    state_=State::finalizing;
                    lock.unlock(); sink_.finalize(); lock.lock(); state_=State::finalized; break;
                }
                Packet packet=std::move(queue_.front()); queue_.pop_front();
                lock.unlock(); sink_.write(packet); lock.lock(); ++stats_.written;
            }
            changed_.notify_all();
        } catch (...) {
            sink_.abort();
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.discarded=stats_.accepted-stats_.written;
            queue_.clear(); state_=State::failed; changed_.notify_all();
        }
    }
public:
    Worker(Sink& sink,std::size_t slots,std::size_t max_packet):sink_(sink),slots_(slots),max_packet_(max_packet) {
        if(!slots || slots>128 || !max_packet || max_packet>16u*1024u*1024u || slots>64u*1024u*1024u/max_packet)
            throw std::invalid_argument("queue bounds");
        thread_=std::thread([this]{run();});
    }
    Worker(const Worker&)=delete;
    Worker& operator=(const Worker&)=delete;
    ~Worker(){cancel(); join();}
    Offer offer(Packet&& packet) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(state_!=State::running) return Offer::closed;
        if((packet.stream!=Stream::video && packet.stream!=Stream::audio) || packet.bytes.empty() ||
           packet.bytes.size()>max_packet_ || packet.bytes.capacity()>max_packet_ || !packet.duration ||
           packet.pts>std::numeric_limits<std::uint64_t>::max()-packet.duration) {
            ++stats_.rejected_invalid; return Offer::invalid;
        }
        const auto i=packet.stream==Stream::video?0u:1u;
        if(packet.pts<end_[i]) {++stats_.rejected_invalid;return Offer::invalid;}
        if(queue_.size()==slots_) {end_[i]=packet.pts+packet.duration;++stats_.dropped_full;return Offer::full;}
        const auto end=packet.pts+packet.duration;
        queue_.push_back(std::move(packet)); end_[i]=end; ++stats_.accepted; changed_.notify_one(); return Offer::accepted;
    }
    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if(state_==State::starting || state_==State::running) {state_=State::draining;changed_.notify_all();}
    }
    void cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        if(state_==State::starting || state_==State::running || state_==State::draining) {
            stats_.discarded+=static_cast<std::uint64_t>(queue_.size());queue_.clear();state_=State::cancelling;changed_.notify_all();
        }
    }
    State wait_ready() {std::unique_lock<std::mutex> lock(mutex_);changed_.wait(lock,[this]{return state_!=State::starting;});return state_;}
    void join(){if(thread_.joinable())thread_.join();}
    State state() const {std::lock_guard<std::mutex> lock(mutex_);return state_;}
    Stats stats() const {std::lock_guard<std::mutex> lock(mutex_);return stats_;}
};
}
