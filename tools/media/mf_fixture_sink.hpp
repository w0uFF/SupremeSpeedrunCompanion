#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <memory>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "src/modules/recorder/public/worker.hpp"
namespace ssc::media_fixture {
using Microsoft::WRL::ComPtr;
constexpr UINT32 width=160, height=96, fps=25;
constexpr DWORD bytes=width*height*3/2;
constexpr LONGLONG period=10000000/fps;
struct VideoSpec { UINT32 frame_width, frame_height; DWORD frame_bytes, bitrate; };
inline constexpr VideoSpec low_spec{width,height,bytes,500000};
inline constexpr VideoSpec hd_spec{1280,720,1280u*720u*3u/2u,5000000};
inline void check(HRESULT hr, const char* step) {
    if(FAILED(hr)) {
        std::cerr<<step<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';
        throw std::runtime_error(step);
    }
}
struct Com { Com(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM");} ~Com(){CoUninitialize();} };
struct Foundation { Foundation(){check(MFStartup(MF_VERSION),"MFStartup");} ~Foundation(){MFShutdown();} };
inline ComPtr<IMFMediaType> type(const GUID& subtype,VideoSpec spec=low_spec) {
    ComPtr<IMFMediaType> t;
    check(MFCreateMediaType(t.GetAddressOf()),"create media type");
    check(t->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video),"major type");
    check(t->SetGUID(MF_MT_SUBTYPE,subtype),"subtype");
    check(MFSetAttributeSize(t.Get(),MF_MT_FRAME_SIZE,spec.frame_width,spec.frame_height),"frame size");
    check(MFSetAttributeRatio(t.Get(),MF_MT_FRAME_RATE,fps,1),"frame rate");
    check(MFSetAttributeRatio(t.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1),"pixel aspect");
    check(t->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive),"interlace");
    return t;
}
constexpr UINT32 sample_rate=48000, channels=2, block_align=4, pcm_frames=sample_rate/fps;
constexpr DWORD pcm_bytes=pcm_frames*block_align;
inline ComPtr<IMFMediaType> audio_type(bool compressed) {
    ComPtr<IMFMediaType> t;
    check(MFCreateMediaType(t.GetAddressOf()),"create audio media type");
    check(t->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio),"audio major");
    check(t->SetGUID(MF_MT_SUBTYPE,compressed?MFAudioFormat_AAC:MFAudioFormat_PCM),"audio subtype");
    check(t->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,channels),"audio channels");
    check(t->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,sample_rate),"audio rate");
    check(t->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16),"audio bits");
    check(t->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,compressed?16000u:sample_rate*block_align),"audio byte rate");
    if(!compressed) check(t->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,block_align),"audio alignment");
    return t;
}
inline ComPtr<IMFSinkWriter> open_writer(const wchar_t* path,bool with_audio,DWORD& stream,DWORD& audio_stream,VideoSpec spec=low_spec) {
    ComPtr<IMFSinkWriter> writer;
    check(MFCreateSinkWriterFromURL(path,nullptr,nullptr,writer.GetAddressOf()),"create MP4 sink writer");
    auto output=type(MFVideoFormat_H264,spec);
    check(output->SetUINT32(MF_MT_AVG_BITRATE,spec.bitrate),"bitrate");
    check(writer->AddStream(output.Get(),&stream),"add H264 stream");
    auto input=type(MFVideoFormat_NV12,spec);
    check(input->SetUINT32(MF_MT_DEFAULT_STRIDE,spec.frame_width),"stride");
    check(writer->SetInputMediaType(stream,input.Get(),nullptr),"NV12 input");
    if(with_audio) {
        auto audio_out=audio_type(true),audio_in=audio_type(false);
        check(writer->AddStream(audio_out.Get(),&audio_stream),"add AAC stream");
        check(writer->SetInputMediaType(audio_stream,audio_in.Get(),nullptr),"PCM input");
    }
    check(writer->BeginWriting(),"begin writing");
    return writer;
}
class AsyncSink final:public supreme::recorder::Sink {
    const wchar_t* path_;
    bool with_audio_;
    const VideoSpec spec_;
    std::unique_ptr<Com> com_;
    std::unique_ptr<Foundation> mf_;
    ComPtr<IMFSinkWriter> writer_;
    DWORD video_=0,audio_=0;
    const unsigned fail_write_;
    unsigned writes_=0;
    bool injected_=false;
    void release() noexcept {writer_.Reset();mf_.reset();com_.reset();}
public:
    explicit AsyncSink(const wchar_t* path,bool audio=true,VideoSpec spec=low_spec,unsigned fail_write=0):path_(path),with_audio_(audio),spec_(spec),fail_write_(fail_write){}
    // Test-only observation; caller may read only after Worker::join().
    bool injected_failure() const noexcept{return injected_;}
    void begin() override {
        com_=std::make_unique<Com>();
        mf_=std::make_unique<Foundation>();
        writer_=open_writer(path_,with_audio_,video_,audio_,spec_);
    }
    void write(const supreme::recorder::Packet& packet) override {
        if(fail_write_ && ++writes_==fail_write_){injected_=true;throw std::runtime_error("injected fixture sink write failure");}
        const bool video=packet.stream==supreme::recorder::Stream::video;
        if(!video && !with_audio_)throw std::runtime_error("audio stream disabled");
        const DWORD length=video?spec_.frame_bytes:pcm_bytes;
        constexpr auto max_timestamp=static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max());
        if(packet.bytes.size()!=length || packet.pts>max_timestamp ||
           packet.duration>max_timestamp-packet.pts)
            throw std::runtime_error("invalid synthetic sink packet");
        ComPtr<IMFMediaBuffer> buffer;
        check(MFCreateMemoryBuffer(length,buffer.GetAddressOf()),"worker buffer");
        BYTE* data=nullptr;
        check(buffer->Lock(&data,nullptr,nullptr),"worker buffer lock");
        std::memcpy(data,packet.bytes.data(),length);
        check(buffer->Unlock(),"worker buffer unlock");
        check(buffer->SetCurrentLength(length),"worker buffer length");
        ComPtr<IMFSample> sample;
        check(MFCreateSample(sample.GetAddressOf()),"worker sample");
        check(sample->AddBuffer(buffer.Get()),"worker attach");
        check(sample->SetSampleTime(static_cast<LONGLONG>(packet.pts)),"worker sample time");
        check(sample->SetSampleDuration(static_cast<LONGLONG>(packet.duration)),"worker sample duration");
        check(writer_->WriteSample(video?video_:audio_,sample.Get()),"worker write");
    }
    void finalize() override {check(writer_->Finalize(),"worker finalize");release();}
    void abort() noexcept override {release();}
};
}
