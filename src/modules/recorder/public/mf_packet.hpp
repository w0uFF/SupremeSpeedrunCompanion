#pragma once
#include "src/modules/recorder/public/worker.hpp"
#include <string>

namespace supreme::recorder::mf {
// Existing PCM fixture profile; no native audio source is implied.
inline constexpr std::uint32_t pcm_rate=48000, pcm_channels=2, pcm_align=4;
inline constexpr std::uint32_t pcm_samples_per_packet=1920;
inline constexpr std::uint32_t pcm_packet_bytes=pcm_samples_per_packet*pcm_align;

// A copied destination, not an exclusive reservation. Only a verified OutputLease
// may supply production paths; the fixture supplies its existing private path.
class Destination {
    const std::wstring path_;
public:
    explicit Destination(std::wstring path):path_(std::move(path)){
        if(path_.empty() || path_.find(L'\0')!=std::wstring::npos)
            throw std::invalid_argument("media destination");
    }
    const wchar_t* c_str() const noexcept{return path_.c_str();}
};

// Check the exact NV12 or fixture PCM payload before any platform API call.
// The Worker already enforces per-stream monotonicity and queue bounds.
inline std::uint32_t packet_length(const Packet& packet,std::uint32_t video_bytes,bool with_audio){
    if(packet.stream==Stream::audio && !with_audio)throw std::runtime_error("audio stream disabled");
    const auto length=packet.stream==Stream::video?video_bytes:pcm_packet_bytes;
    constexpr auto max_timestamp=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if((packet.stream!=Stream::video && packet.stream!=Stream::audio) || !length ||
       packet.bytes.size()!=length || !packet.duration || packet.pts>max_timestamp ||
       packet.duration>max_timestamp-packet.pts)
        throw std::runtime_error("invalid synthetic sink packet");
    return length;
}
}
