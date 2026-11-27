#pragma once
#include "src/modules/recorder/adapters/windows/mf_writer.hpp"

namespace ssc::media_fixture {
namespace mf=supreme::recorder::mf;
using Microsoft::WRL::ComPtr;
using mf::VideoSpec;
using mf::check;
using mf::Com;
using mf::Foundation;
using mf::audio_type;
constexpr UINT32 width=160, height=96, fps=25;
constexpr DWORD bytes=width*height*3/2;
constexpr LONGLONG period=10000000/fps;
inline constexpr VideoSpec low_spec{width,height,bytes,500000};
inline constexpr VideoSpec hd_spec{1280,720,1280u*720u*3u/2u,5000000};
constexpr UINT32 sample_rate=mf::sample_rate, channels=mf::channels, block_align=mf::block_align, pcm_frames=sample_rate/fps;
constexpr DWORD pcm_bytes=pcm_frames*block_align;
static_assert(pcm_bytes==mf::pcm_packet_bytes,"fixture PCM packet compatibility");
inline ComPtr<IMFMediaType> type(const GUID& subtype,VideoSpec spec=low_spec) {
    return mf::type(subtype,spec,fps);
}
inline ComPtr<IMFSinkWriter> open_writer(const wchar_t* path,bool with_audio,DWORD& stream,DWORD& audio_stream,VideoSpec spec=low_spec) {
    return mf::open_writer(path,with_audio,stream,audio_stream,spec,fps);
}
class AsyncSink final:public supreme::recorder::Sink {
    mf::Writer writer_;
    const unsigned fail_write_;
    unsigned writes_=0;
    bool injected_=false;
    const bool fail_finalize_;
    bool injected_finalize_=false;
public:
    explicit AsyncSink(const wchar_t* path,bool audio=true,VideoSpec spec=low_spec,unsigned fail_write=0,bool fail_finalize=false):
        writer_(path?path:L"",audio,spec,fps),fail_write_(fail_write),fail_finalize_(fail_finalize){}
    // Test-only observation; caller may read only after Worker::join().
    bool injected_failure() const noexcept{return injected_;}
    bool injected_finalize_failure() const noexcept{return injected_finalize_;}
    void begin() override {writer_.begin();}
    void write(const supreme::recorder::Packet& packet) override {
        if(fail_write_ && ++writes_==fail_write_){injected_=true;throw std::runtime_error("injected fixture sink write failure");}
        writer_.write(packet);
    }
    void finalize() override {
        if(fail_finalize_){injected_finalize_=true;throw std::runtime_error("injected fixture sink finalization failure");}
        writer_.finalize();
    }
    void abort() noexcept override {writer_.abort();}
};
}
