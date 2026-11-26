// Standalone Windows synthetic encode/decode probe. No game or capture access.
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
using Microsoft::WRL::ComPtr;
namespace {
constexpr UINT32 width=160, height=96, fps=25, frames=12;
constexpr DWORD bytes=width*height*3/2;
constexpr LONGLONG period=10000000/fps;
void check(HRESULT hr, const char* step) {
    if(FAILED(hr)) {
        std::cerr<<step<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';
        throw std::runtime_error(step);
    }
}
struct Com { Com(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM");} ~Com(){CoUninitialize();} };
struct Foundation { Foundation(){check(MFStartup(MF_VERSION),"MFStartup");} ~Foundation(){MFShutdown();} };
ComPtr<IMFMediaType> type(const GUID& subtype) {
    ComPtr<IMFMediaType> t;
    check(MFCreateMediaType(t.GetAddressOf()),"create media type");
    check(t->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video),"major type");
    check(t->SetGUID(MF_MT_SUBTYPE,subtype),"subtype");
    check(MFSetAttributeSize(t.Get(),MF_MT_FRAME_SIZE,width,height),"frame size");
    check(MFSetAttributeRatio(t.Get(),MF_MT_FRAME_RATE,fps,1),"frame rate");
    check(MFSetAttributeRatio(t.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1),"pixel aspect");
    check(t->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive),"interlace");
    return t;
}
void encode(const wchar_t* path) {
    ComPtr<IMFSinkWriter> writer;
    check(MFCreateSinkWriterFromURL(path,nullptr,nullptr,writer.GetAddressOf()),"create MP4 sink writer");
    auto output=type(MFVideoFormat_H264);
    check(output->SetUINT32(MF_MT_AVG_BITRATE,500000),"bitrate");
    DWORD stream=0;
    check(writer->AddStream(output.Get(),&stream),"add H264 stream");
    auto input=type(MFVideoFormat_NV12);
    check(input->SetUINT32(MF_MT_DEFAULT_STRIDE,width),"stride");
    check(writer->SetInputMediaType(stream,input.Get(),nullptr),"NV12 input");
    check(writer->BeginWriting(),"begin writing");
    for(UINT32 i=0;i<frames;++i) {
        ComPtr<IMFMediaBuffer> buffer;
        check(MFCreateMemoryBuffer(bytes,buffer.GetAddressOf()),"create frame buffer");
        BYTE* data=nullptr;
        check(buffer->Lock(&data,nullptr,nullptr),"lock frame buffer");
        std::fill_n(data,width*height,static_cast<BYTE>(i<frames/2?48:192));
        std::fill_n(data+width*height,width*height/2,static_cast<BYTE>(128));
        check(buffer->Unlock(),"unlock frame buffer");
        check(buffer->SetCurrentLength(bytes),"set frame length");
        ComPtr<IMFSample> sample;
        check(MFCreateSample(sample.GetAddressOf()),"create sample");
        check(sample->AddBuffer(buffer.Get()),"add sample buffer");
        check(sample->SetSampleTime(static_cast<LONGLONG>(i)*period),"sample time");
        check(sample->SetSampleDuration(period),"sample duration");
        check(writer->WriteSample(stream,sample.Get()),"write sample");
    }
    check(writer->Finalize(),"finalize MP4");
}
void decode(const wchar_t* path) {
    ComPtr<IMFSourceReader> reader;
    check(MFCreateSourceReaderFromURL(path,nullptr,reader.GetAddressOf()),"open finalized MP4");
    constexpr DWORD video=MF_SOURCE_READER_FIRST_VIDEO_STREAM;
    check(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE),"disable streams");
    check(reader->SetStreamSelection(video,TRUE),"select video");
    ComPtr<IMFMediaType> native;
    check(reader->GetNativeMediaType(video,0,native.GetAddressOf()),"read encoded format");
    GUID codec{};
    check(native->GetGUID(MF_MT_SUBTYPE,&codec),"encoded subtype");
    if(codec!=MFVideoFormat_H264) throw std::runtime_error("unexpected encoded format");
    ComPtr<IMFMediaType> request;
    check(MFCreateMediaType(request.GetAddressOf()),"create decode type");
    check(request->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video),"decode major");
    check(request->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_NV12),"decode subtype");
    check(reader->SetCurrentMediaType(video,nullptr,request.Get()),"request NV12 decode");
    ComPtr<IMFMediaType> actual;
    check(reader->GetCurrentMediaType(video,actual.GetAddressOf()),"get decode type");
    UINT32 w=0,h=0;
    check(MFGetAttributeSize(actual.Get(),MF_MT_FRAME_SIZE,&w,&h),"decoded size");
    if(w!=width || h!=height) throw std::runtime_error("unexpected dimensions");
    UINT32 count=0;
    LONGLONG previous=-1;
    bool eos=false;
    for(unsigned calls=0;calls<128 && !eos;++calls) {
        DWORD flags=0; LONGLONG timestamp=0;
        ComPtr<IMFSample> sample;
        check(reader->ReadSample(video,0,nullptr,&flags,&timestamp,sample.GetAddressOf()),"decode sample");
        if(flags & (MF_SOURCE_READERF_ERROR|MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED))
            throw std::runtime_error("unexpected decoder state change");
        eos=(flags & MF_SOURCE_READERF_ENDOFSTREAM)!=0;
        if(!sample) continue;
        if(count>=frames || timestamp<=previous) throw std::runtime_error("invalid sample sequence");
        previous=timestamp;
        ComPtr<IMFMediaBuffer> buffer;
        check(sample->ConvertToContiguousBuffer(buffer.GetAddressOf()),"contiguous decoded buffer");
        BYTE* data=nullptr; DWORD length=0;
        check(buffer->Lock(&data,nullptr,&length),"lock decoded frame");
        // Sample only the first luma byte; tolerates lossy coding and row padding.
        const int luma=length>=bytes?data[0]:-1;
        check(buffer->Unlock(),"unlock decoded frame");
        if(luma<0 || (count<frames/2?luma>=100:luma<=140))
            throw std::runtime_error("decoded luma pattern mismatch");
        ++count;
    }
    if(!eos || count!=frames) throw std::runtime_error("frame count or EOS mismatch");
    std::cout<<"Decoded "<<count<<" H264 frames, "<<width<<'x'<<height<<", monotonic timestamps and luma pattern verified\n";
}
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=2 || GetFileAttributesW(argv[1])!=INVALID_FILE_ATTRIBUTES) return 2;
    try { Com com; Foundation mf; encode(argv[1]); decode(argv[1]); return 0; }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n'; return 1;}
}
