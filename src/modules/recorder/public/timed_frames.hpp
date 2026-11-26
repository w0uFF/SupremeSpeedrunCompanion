#pragma once
#include <algorithm>
#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace supreme::recorder {
// Bounded private test handoff; not a media container or public archive format.
// SSCTIME1 + records of little-endian absolute 100ns QPC-correlated time + fixed NV12 bytes.
inline constexpr std::size_t fixture_bytes=160u*96u*3u/2u;
inline constexpr std::size_t fixture_max_frames=24;
inline constexpr std::uint64_t fixture_max_span=80000000;
inline constexpr std::uint64_t fixture_tail=400000, fixture_max_hold=1000000;
struct TimedFrame {std::uint64_t time;std::vector<std::uint8_t> pixels;};
struct FrameInterval {std::uint64_t pts,duration;};
inline std::vector<FrameInterval> frame_intervals(const std::vector<TimedFrame>& frames){
    if(frames.size()<2 || frames.size()>fixture_max_frames)throw std::invalid_argument("timed frame count");
    const auto origin=frames.front().time;
    std::vector<FrameInterval> out;out.reserve(frames.size());
    for(std::size_t i=0;i<frames.size();++i){
        const auto now=frames[i].time;
        if(frames[i].pixels.size()!=fixture_bytes || now<origin || now-origin>fixture_max_span ||
           (i && now<=frames[i-1].time))throw std::invalid_argument("timed frame bounds/order");
        if(i+1<frames.size() && frames[i+1].time<=now)throw std::invalid_argument("timed frame order");
        const auto duration=i+1<frames.size()?std::min(frames[i+1].time-now,fixture_max_hold):fixture_tail;
        out.push_back({now-origin,duration});
    }
    return out;
}
inline void write_timed_frames(std::ostream& out,const std::vector<TimedFrame>& frames){
    (void)frame_intervals(frames);
    out.write("SSCTIME1",8);
    for(const auto& frame:frames){
        for(unsigned i=0;i<8;++i)out.put(static_cast<char>((frame.time>>(8*i))&255u));
        out.write(reinterpret_cast<const char*>(frame.pixels.data()),static_cast<std::streamsize>(fixture_bytes));
    }
    if(!out)throw std::runtime_error("timed frame write");
}
inline std::vector<TimedFrame> read_timed_frames(std::istream& in){
    char magic[8]{};
    if(!in.read(magic,8) || std::string_view(magic,8)!="SSCTIME1")throw std::invalid_argument("timed frame header");
    std::vector<TimedFrame> result;
    while(in.peek()!=std::char_traits<char>::eof()){
        if(result.size()==fixture_max_frames)throw std::invalid_argument("timed frame limit");
        std::uint64_t time=0;
        for(unsigned i=0;i<8;++i){
            const auto c=in.get();if(c==std::char_traits<char>::eof())throw std::invalid_argument("truncated timestamp");
            time|=static_cast<std::uint64_t>(static_cast<unsigned char>(c))<<(8*i);
        }
        TimedFrame frame{time,std::vector<std::uint8_t>(fixture_bytes)};
        if(!in.read(reinterpret_cast<char*>(frame.pixels.data()),static_cast<std::streamsize>(fixture_bytes)))
            throw std::invalid_argument("truncated pixels");
        result.push_back(std::move(frame));
    }
    if(in.bad())throw std::runtime_error("timed frame read");
    (void)frame_intervals(result);return result;
}
}
