#pragma once
#include "src/modules/recorder/public/worker.hpp"
#include <memory>
#include <string>

namespace supreme::recorder {
struct VideoFormat {
    std::uint32_t width,height,nominal_fps,bitrate;
    constexpr bool valid() const noexcept {
        return width>=2 && width<=4096 && height>=2 && height<=4096 && !(width%2) && !(height%2) &&
               std::uint64_t(width)*height*3/2<=16u*1024u*1024u &&
               nominal_fps>=1 && nominal_fps<=120 && bitrate>=1 && bitrate<=100000000;
    }
    constexpr std::size_t frame_bytes() const noexcept {
        return valid()?static_cast<std::size_t>(std::uint64_t(width)*height*3/2):0;
    }
};
// Instance capture identity plus opaque, independently allocated output token.
// Token is not a path or proof of archive provenance.
struct OutputIdentity {
    std::uint64_t capture;
    std::string token;
    std::uint64_t byte_budget;
    bool valid() const noexcept {
        if(!capture || token.size()!=32 || !byte_budget || byte_budget>64ull*1024*1024*1024)return false;
        for(char c:token)if(!((c>='0' && c<='9') || (c>='a' && c<='f')))return false;
        return true;
    }
};
inline bool operator==(const OutputIdentity& a,const OutputIdentity& b) noexcept {
    return a.capture==b.capture && a.token==b.token && a.byte_budget==b.byte_budget;
}
// Worker-thread only. Provider owns exclusive reservation and path safety;
// writer owns codec calls. Reservation must outlive writer and retain failures.
class OutputLease {
public:
    virtual ~OutputLease()=default;
    virtual const OutputIdentity& identity() const noexcept=0;
    virtual std::unique_ptr<Sink> make_writer(const VideoFormat&)=0;
    virtual std::uint64_t size_bytes()=0; // observed bytes, not a hard write quota
    virtual void seal()=0; // mark media finalized, not an eligible/archived run
    virtual void incomplete() noexcept=0; // preserve, never silently delete
};
class OutputProvider {
public:
    virtual ~OutputProvider()=default;
    // Concurrent worker calls. Reject collisions; reserve atomically and exclusively.
    // Throwing reserve must clean up or preserve any partial reservation itself.
    virtual std::unique_ptr<OutputLease> reserve(const OutputIdentity&)=0;
};
// Reusable decorator over an injected existing codec Sink. No OS or MF dependency.
// All calls (including abort after partial begin) are made by the existing Worker.
class OwnedVideoSink final:public Sink {
    enum class Phase { fresh, opening, running, terminal };
    OutputProvider& provider_;
    const OutputIdentity id_;
    const VideoFormat format_;
    std::unique_ptr<OutputLease> lease_;
    std::unique_ptr<Sink> writer_;
    Phase phase_=Phase::fresh;
    std::uint64_t end_=0;
    bool written_=false;
    void check_size(){if(lease_->size_bytes()>id_.byte_budget)throw std::runtime_error("video byte budget exceeded");}
public:
    OwnedVideoSink(OutputProvider& provider,OutputIdentity id,VideoFormat format):provider_(provider),id_(std::move(id)),format_(format){
        if(!id_.valid() || !format_.valid())throw std::invalid_argument("video output configuration");
    }
    const OutputIdentity& identity() const noexcept{return id_;}
    void begin() override {
        if(phase_!=Phase::fresh)throw std::logic_error("video begin state");
        phase_=Phase::opening;
        lease_=provider_.reserve(id_);
        if(!lease_)throw std::runtime_error("video reservation unavailable");
        if(!(lease_->identity()==id_)){lease_.reset();throw std::runtime_error("video reservation mismatch");}
        writer_=lease_->make_writer(format_);
        if(!writer_)throw std::runtime_error("video writer unavailable");
        writer_->begin();check_size();phase_=Phase::running;
    }
    void write(const Packet& packet) override {
        constexpr auto max_pts=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        if(phase_!=Phase::running || packet.stream!=Stream::video || packet.bytes.size()!=format_.frame_bytes() ||
           !packet.duration || packet.pts<end_ || packet.pts>max_pts || packet.duration>max_pts-packet.pts)
            throw std::invalid_argument("video packet");
        writer_->write(packet);check_size();end_=packet.pts+packet.duration;written_=true;
    }
    void finalize() override {
        if(phase_!=Phase::running || !written_)throw std::logic_error("video finalize state");
        writer_->finalize();writer_.reset(); // codec finished; never abort it after success
        check_size();lease_->seal();lease_.reset();phase_=Phase::terminal;
    }
    void abort() noexcept override {
        if(phase_==Phase::terminal)return;
        if(writer_){writer_->abort();writer_.reset();}
        if(lease_){lease_->incomplete();lease_.reset();}
        phase_=Phase::terminal;
    }
};
}
