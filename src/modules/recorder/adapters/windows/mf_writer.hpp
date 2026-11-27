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
#include "src/modules/recorder/public/mf_packet.hpp"
#include "src/modules/recorder/public/owned_video_sink.hpp"
namespace supreme::recorder::mf {
using Microsoft::WRL::ComPtr;
struct VideoSpec { UINT32 frame_width, frame_height; DWORD frame_bytes, bitrate; };
inline void check(HRESULT hr, const char* step) {
    if(FAILED(hr)) {
        std::cerr<<step<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';
        throw std::runtime_error(step);
    }
}
struct Com {
    Com(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM");}
    Com(const Com&)=delete;
    Com& operator=(const Com&)=delete;
    ~Com(){CoUninitialize();}
};
struct Foundation {
    Foundation(){check(MFStartup(MF_VERSION),"MFStartup");}
    Foundation(const Foundation&)=delete;
    Foundation& operator=(const Foundation&)=delete;
    ~Foundation(){MFShutdown();}
};
inline ComPtr<IMFMediaType> type(const GUID& subtype,VideoSpec spec,UINT32 nominal_fps) {
    ComPtr<IMFMediaType> t;
    check(MFCreateMediaType(t.GetAddressOf()),"create media type");
    check(t->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video),"major type");
    check(t->SetGUID(MF_MT_SUBTYPE,subtype),"subtype");
    check(MFSetAttributeSize(t.Get(),MF_MT_FRAME_SIZE,spec.frame_width,spec.frame_height),"frame size");
    check(MFSetAttributeRatio(t.Get(),MF_MT_FRAME_RATE,nominal_fps,1),"frame rate");
    check(MFSetAttributeRatio(t.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1),"pixel aspect");
    check(t->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive),"interlace");
    return t;
}
constexpr UINT32 sample_rate=pcm_rate, channels=pcm_channels, block_align=pcm_align;
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
inline ComPtr<IMFSinkWriter> open_writer(const wchar_t* path,bool with_audio,DWORD& stream,DWORD& audio_stream,VideoSpec spec,UINT32 nominal_fps) {
    ComPtr<IMFSinkWriter> writer;
    check(MFCreateSinkWriterFromURL(path,nullptr,nullptr,writer.GetAddressOf()),"create MP4 sink writer");
    auto output=type(MFVideoFormat_H264,spec,nominal_fps);
    check(output->SetUINT32(MF_MT_AVG_BITRATE,spec.bitrate),"bitrate");
    check(writer->AddStream(output.Get(),&stream),"add H264 stream");
    auto input=type(MFVideoFormat_NV12,spec,nominal_fps);
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
// Worker-thread lifecycle only. Construct with an already-reserved private
// destination; this class owns the path, not the reservation or archive identity.
class Writer final:public Sink {
    const Destination destination_;
    const bool with_audio_;
    const VideoSpec spec_;
    const UINT32 nominal_fps_;
    std::unique_ptr<Com> com_;
    std::unique_ptr<Foundation> mf_;
    ComPtr<IMFSinkWriter> writer_;
    DWORD video_=0,audio_=0;
    void release() noexcept {writer_.Reset();mf_.reset();com_.reset();}
public:
    Writer(std::wstring path,bool audio,VideoSpec spec,UINT32 nominal_fps):
        destination_(std::move(path)),with_audio_(audio),spec_(spec),nominal_fps_(nominal_fps){}
    void begin() override {
        com_=std::make_unique<Com>();
        mf_=std::make_unique<Foundation>();
        writer_=open_writer(destination_.c_str(),with_audio_,video_,audio_,spec_,nominal_fps_);
    }
    void write(const Packet& packet) override {
        const bool video=packet.stream==Stream::video;
        const DWORD length=static_cast<DWORD>(packet_length(packet,static_cast<std::uint32_t>(spec_.frame_bytes),with_audio_));
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
    void finalize() override {
        check(writer_->Finalize(),"worker finalize");release();
    }
    void abort() noexcept override {release();}
};
// Intended for OutputLease::make_writer: the lease owns output identity and
// exclusive reservation, the writer owns a copy of the resolved path.
// Production audio stays disabled; the fixed PCM profile is fixture-only.
inline std::unique_ptr<Sink> make_video_writer(std::wstring path,const VideoFormat& format) {
    if(!format.valid())throw std::invalid_argument("video output configuration");
    const VideoSpec spec{format.width,format.height,static_cast<DWORD>(format.frame_bytes()),static_cast<DWORD>(format.bitrate)};
    return std::make_unique<Writer>(std::move(path),false,spec,format.nominal_fps);
}
}
