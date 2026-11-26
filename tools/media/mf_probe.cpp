// Standalone Windows synthetic encode/decode probe. No game or capture access.
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <memory>
#include "src/modules/recorder/public/worker.hpp"
#include "src/modules/recorder/public/timed_frames.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using Microsoft::WRL::ComPtr;
namespace {
constexpr UINT32 width=160, height=96, fps=25;
UINT32 frames=12; // Set before constructing any worker; standalone probe only.
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
constexpr UINT32 sample_rate=48000, channels=2, block_align=4, pcm_frames=sample_rate/fps;
constexpr DWORD pcm_bytes=pcm_frames*block_align;
ComPtr<IMFMediaType> audio_type(bool compressed) {
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
void fill_audio(BYTE* data,UINT32 index) {
    for(UINT32 i=0;i<pcm_frames;++i) {
        const auto value=static_cast<UINT16>(((index*pcm_frames+i)/24u)%2u?8000:-8000);
        for(UINT32 ch=0;ch<channels;++ch) {
            const auto offset=i*block_align+ch*2u;
            data[offset]=static_cast<BYTE>(value & 0xffu);
            data[offset+1]=static_cast<BYTE>(value>>8);
        }
    }
}
void write_audio(IMFSinkWriter* writer,DWORD stream,UINT32 index) {
    ComPtr<IMFMediaBuffer> buffer;
    check(MFCreateMemoryBuffer(pcm_bytes,buffer.GetAddressOf()),"create PCM buffer");
    BYTE* data=nullptr;
    check(buffer->Lock(&data,nullptr,nullptr),"lock PCM");
    fill_audio(data,index);
    check(buffer->Unlock(),"unlock PCM");
    check(buffer->SetCurrentLength(pcm_bytes),"PCM length");
    ComPtr<IMFSample> sample;
    check(MFCreateSample(sample.GetAddressOf()),"create audio sample");
    check(sample->AddBuffer(buffer.Get()),"attach PCM");
    check(sample->SetSampleTime(static_cast<LONGLONG>(index)*period),"audio timestamp");
    check(sample->SetSampleDuration(period),"audio duration");
    check(writer->WriteSample(stream,sample.Get()),"write PCM");
}
ComPtr<IMFSinkWriter> open_writer(const wchar_t* path,bool with_audio,DWORD& stream,DWORD& audio_stream) {
    ComPtr<IMFSinkWriter> writer;
    check(MFCreateSinkWriterFromURL(path,nullptr,nullptr,writer.GetAddressOf()),"create MP4 sink writer");
    auto output=type(MFVideoFormat_H264);
    check(output->SetUINT32(MF_MT_AVG_BITRATE,500000),"bitrate");
    check(writer->AddStream(output.Get(),&stream),"add H264 stream");
    auto input=type(MFVideoFormat_NV12);
    check(input->SetUINT32(MF_MT_DEFAULT_STRIDE,width),"stride");
    check(writer->SetInputMediaType(stream,input.Get(),nullptr),"NV12 input");
    if(with_audio) {
        auto audio_out=audio_type(true),audio_in=audio_type(false);
        check(writer->AddStream(audio_out.Get(),&audio_stream),"add AAC stream");
        check(writer->SetInputMediaType(audio_stream,audio_in.Get(),nullptr),"PCM input");
    }
    check(writer->BeginWriting(),"begin writing");
    return writer;
}
void encode(const wchar_t* path,bool with_audio) {
    DWORD stream=0,audio_stream=0;
    auto writer=open_writer(path,with_audio,stream,audio_stream);
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
        if(with_audio) write_audio(writer.Get(),audio_stream,i);
    }
    check(writer->Finalize(),"finalize MP4");
}
// Real worker adapter for synthetic packets only. COM/MF initialized and released on the worker thread.
class AsyncSink final:public supreme::recorder::Sink {
    const wchar_t* path_;
    bool with_audio_;
    std::unique_ptr<Com> com_;
    std::unique_ptr<Foundation> mf_;
    ComPtr<IMFSinkWriter> writer_;
    DWORD video_=0,audio_=0;
    void release() noexcept {writer_.Reset();mf_.reset();com_.reset();}
public:
    explicit AsyncSink(const wchar_t* path,bool audio=true):path_(path),with_audio_(audio){}
    void begin() override {
        com_=std::make_unique<Com>();
        mf_=std::make_unique<Foundation>();
        writer_=open_writer(path_,with_audio_,video_,audio_);
    }
    void write(const supreme::recorder::Packet& packet) override {
        const bool video=packet.stream==supreme::recorder::Stream::video;
        if(!video && !with_audio_)throw std::runtime_error("audio stream disabled");
        const DWORD length=video?bytes:pcm_bytes;
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
void encode_async(const wchar_t* path,bool paced,const wchar_t* raw=nullptr) {
    std::vector<std::uint8_t> captured;
    if(raw){
        std::ifstream input(std::filesystem::path(raw),std::ios::binary);
        captured.resize(2u*bytes);
        if(!input.read(reinterpret_cast<char*>(captured.data()),static_cast<std::streamsize>(captured.size())) || input.peek()!=std::char_traits<char>::eof())
            throw std::runtime_error("invalid captured NV12 fixture");
    }
    using namespace supreme::recorder;
    AsyncSink sink(path);
    Worker worker(sink,32,32768);
    if(worker.wait_ready()!=State::running) throw std::runtime_error("worker start failed");
    const auto epoch=std::chrono::steady_clock::now();
    for(UINT32 i=0;i<frames;++i) {
        if(paced) std::this_thread::sleep_until(epoch+std::chrono::milliseconds(40)*i);
        const auto pts=static_cast<std::uint64_t>(i)*static_cast<std::uint64_t>(period);
        Packet video{Stream::video,pts,static_cast<std::uint64_t>(period),std::vector<std::uint8_t>(bytes)};
        if(raw)std::copy_n(captured.data()+(i<frames/2?0u:bytes),bytes,video.bytes.data());
        else{
            std::fill_n(video.bytes.data(),width*height,static_cast<BYTE>(i<frames/2?48:192));
            std::fill_n(video.bytes.data()+width*height,width*height/2,static_cast<BYTE>(128));
        }
        Packet audio{Stream::audio,pts,static_cast<std::uint64_t>(period),std::vector<std::uint8_t>(pcm_bytes)};
        fill_audio(audio.bytes.data(),i);
        if(worker.offer(std::move(video))!=Offer::accepted || worker.offer(std::move(audio))!=Offer::accepted)
            throw std::runtime_error("worker offer failed");
    }
    worker.stop();
    worker.join();
    const auto stats=worker.stats();
    if(worker.state()!=State::finalized || stats.accepted!=2u*frames || stats.written!=stats.accepted ||
       stats.dropped_full || stats.rejected_invalid || stats.discarded)
        throw std::runtime_error("worker finalization or counters");
    const auto quality=worker.quality();
    for(std::size_t index=0;index<quality.size();++index) {
        const auto& q=quality[index];const auto& c=q.written;
        if(q.accepted!=frames || c.packets!=frames || c.first!=0 || c.duration!=static_cast<std::uint64_t>(frames)*static_cast<std::uint64_t>(period) ||
           c.end!=c.duration || c.internal_gap()!=0 || q.dropped_full || q.discarded || q.rejected_invalid)
            throw std::runtime_error("stream quality mismatch");
        std::cout<<"StreamQuality "<<(index==0?"video":"audio")<<" "<<q.accepted<<" "<<c.packets<<" "
                 <<c.first<<" "<<c.end<<" "<<c.duration<<" "<<*c.internal_gap()<<" "
                 <<q.dropped_full<<" "<<q.dropped_duration<<" "<<q.rejected_invalid<<" "<<q.discarded<<"\n";
    }
    std::cout<<"QueueHighWater "<<worker.high_water()<<"\n";
    std::cout<<"Async worker finalized "<<stats.written<<" packets without observed queue drops\n";
}
void encode_timed(const wchar_t* path,const std::vector<supreme::recorder::TimedFrame>& captured) {
    using namespace supreme::recorder;
    const auto intervals=frame_intervals(captured);
    AsyncSink sink(path,false);
    Worker worker(sink,24,32768);
    if(worker.wait_ready()!=State::running)throw std::runtime_error("timed worker start");
    Coverage expected;
    for(std::size_t i=0;i<captured.size();++i){
        const auto interval=intervals[i];
        Packet video{Stream::video,interval.pts,interval.duration,captured[i].pixels};
        if(!expected.observe(interval.pts,interval.duration) || worker.offer(std::move(video))!=Offer::accepted)
            throw std::runtime_error("timed worker offer");
    }
    worker.stop();worker.join();
    const auto q=worker.quality()[0];
    const auto& c=q.written;
    if(worker.state()!=State::finalized || q.accepted!=captured.size() || c.packets!=expected.packets ||
       c.first!=expected.first || c.end!=expected.end || c.duration!=expected.duration ||
       c.internal_gap()!=expected.internal_gap() || q.dropped_full || q.discarded || q.rejected_invalid)
        throw std::runtime_error("timed worker quality");
    std::cout<<"TimedHandoff "<<c.packets<<" "<<c.first<<" "<<c.end<<" "<<c.duration<<" "<<*c.internal_gap()<<" "<<worker.high_water()<<"\n";
}
LONGLONG decode(const wchar_t* path,const std::vector<supreme::recorder::FrameInterval>* timeline=nullptr) {
    ComPtr<IMFSourceReader> reader;
    check(MFCreateSourceReaderFromURL(path,nullptr,reader.GetAddressOf()),"open finalized MP4");
    constexpr DWORD video=static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
    check(reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS),FALSE),"disable streams");
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
    for(unsigned calls=0;calls<frames+128 && !eos;++calls) {
        DWORD flags=0; LONGLONG timestamp=0;
        ComPtr<IMFSample> sample;
        check(reader->ReadSample(video,0,nullptr,&flags,&timestamp,sample.GetAddressOf()),"decode sample");
        if(flags & (MF_SOURCE_READERF_ERROR|MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED))
            throw std::runtime_error("unexpected decoder state change");
        eos=(flags & MF_SOURCE_READERF_ENDOFSTREAM)!=0;
        if(!sample) continue;
        if(count>=frames || timestamp<=previous) throw std::runtime_error("invalid sample sequence");
        if(timeline){
            const auto expected=static_cast<LONGLONG>((*timeline)[count].pts);
            // Exploratory mux timestamp tolerance (2 ms); do not claim exact preservation.
            constexpr LONGLONG tolerance=20000;
            if(timestamp<expected-tolerance || timestamp>expected+tolerance)
                throw std::runtime_error("decoded timestamp differs from captured timeline");
            std::cout<<"TimedPTS "<<count<<" "<<expected<<" "<<timestamp<<"\n";
        }
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
    return previous+(timeline?static_cast<LONGLONG>(timeline->back().duration):period);
}
void decode_audio(const wchar_t* path,LONGLONG video_end) {
    ComPtr<IMFSourceReader> reader;
    check(MFCreateSourceReaderFromURL(path,nullptr,reader.GetAddressOf()),"open audio from MP4");
    constexpr DWORD audio=static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
    check(reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS),FALSE),"disable streams for audio");
    check(reader->SetStreamSelection(audio,TRUE),"select audio");
    ComPtr<IMFMediaType> native;
    check(reader->GetNativeMediaType(audio,0,native.GetAddressOf()),"read encoded audio type");
    GUID codec{};
    check(native->GetGUID(MF_MT_SUBTYPE,&codec),"encoded audio subtype");
    if(codec!=MFAudioFormat_AAC) throw std::runtime_error("expected AAC");
    ComPtr<IMFMediaType> request;
    check(MFCreateMediaType(request.GetAddressOf()),"decode audio type");
    check(request->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio),"decode audio major");
    check(request->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_PCM),"decode audio PCM");
    check(reader->SetCurrentMediaType(audio,nullptr,request.Get()),"request PCM decode");
    ComPtr<IMFMediaType> actual;
    check(reader->GetCurrentMediaType(audio,actual.GetAddressOf()),"decoded PCM type");
    UINT32 rate=0,num=0,bits=0;
    check(actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,&rate),"decoded rate");
    check(actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS,&num),"decoded channels");
    check(actual->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,&bits),"decoded bits");
    if(rate!=sample_rate || num!=channels || bits!=16) throw std::runtime_error("unexpected PCM format");
    constexpr LONGLONG tolerance=1000000; // 100 ms exploratory bound, not a gameplay requirement.
    bool eos=false,have=false,signal=false;
    LONGLONG first=0,previous=0,end=0;
    UINT32 total_frames=0;
    for(unsigned calls=0;calls<frames*4+256 && !eos;++calls) {
        DWORD flags=0; LONGLONG timestamp=0;
        ComPtr<IMFSample> sample;
        check(reader->ReadSample(audio,0,nullptr,&flags,&timestamp,sample.GetAddressOf()),"read PCM");
        if(flags & (MF_SOURCE_READERF_ERROR|MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED))
            throw std::runtime_error("unexpected audio state change");
        eos=(flags & MF_SOURCE_READERF_ENDOFSTREAM)!=0;
        if(!sample) continue;
        if(timestamp < -tolerance || timestamp > video_end+tolerance || (have && timestamp<=previous))
            throw std::runtime_error("audio timestamp order or bounds");
        if(!have) first=timestamp;
        have=true;previous=timestamp;
        ComPtr<IMFMediaBuffer> buffer;
        check(sample->ConvertToContiguousBuffer(buffer.GetAddressOf()),"contiguous PCM");
        BYTE* data=nullptr;DWORD length=0;
        check(buffer->Lock(&data,nullptr,&length),"lock decoded PCM");
        const bool valid=length>0 && length<=1024u*1024u && length%block_align==0;
        if(valid) {
            for(DWORD i=0;i<length;i+=2) {
                const int raw=static_cast<int>(data[i])+256*static_cast<int>(data[i+1]);
                const int value=raw>=32768?raw-65536:raw;
                if(value>200 || value< -200) signal=true;
            }
        }
        check(buffer->Unlock(),"unlock decoded PCM");
        if(!valid) throw std::runtime_error("invalid PCM length");
        const UINT32 decoded=length/block_align;
        if(decoded>frames*pcm_frames+sample_rate || total_frames>frames*pcm_frames+sample_rate-decoded) throw std::runtime_error("excess decoded samples");
        total_frames+=decoded;
        end=timestamp+static_cast<LONGLONG>(decoded)*10000000/sample_rate;
    }
    const UINT32 input_frames=frames*pcm_frames;
    constexpr UINT32 padding=sample_rate/10;
    if(!eos || !have || !signal || total_frames<input_frames-padding || total_frames>input_frames+padding ||
       first < -tolerance || first > tolerance || end < video_end-tolerance || end > video_end+tolerance)
        throw std::runtime_error("audio coverage/signal or A/V endpoint mismatch");
    std::cout<<"Decoded AAC PCM frames="<<total_frames<<", first_100ns="<<first
             <<", audio_end_100ns="<<end<<", video_end_100ns="<<video_end<<", endpoint tolerance verified\n";
}
}
int wmain(int argc,wchar_t** argv) {
    const bool timed=argc==4 && std::wstring(argv[2])==L"--timed-video";
    const bool extended=argc==3 && std::wstring(argv[2])==L"--extended-av";
    const bool sustained=extended || (argc==3 && std::wstring(argv[2])==L"--sustained-av");
    const bool captured=argc==4 && std::wstring(argv[2])==L"--captured-av";
    const bool asynchronous=captured || sustained || (argc==3 && std::wstring(argv[2])==L"--async-av");
    if(sustained) frames=extended?750u:250u;
    const bool with_audio=asynchronous || (argc==3 && std::wstring(argv[2])==L"--av");
    if((argc!=2 && !with_audio && !timed) || GetFileAttributesW(argv[1])!=INVALID_FILE_ATTRIBUTES) return 2;
    try {
        Com com; Foundation mf;
        if(timed){
            std::ifstream input(std::filesystem::path(argv[3]),std::ios::binary);
            const auto captured=supreme::recorder::read_timed_frames(input);
            const auto intervals=supreme::recorder::frame_intervals(captured);
            frames=static_cast<UINT32>(captured.size());
            encode_timed(argv[1],captured);
            (void)decode(argv[1],&intervals);
            return 0;
        }
        if(asynchronous) encode_async(argv[1],sustained,captured?argv[3]:nullptr); else encode(argv[1],with_audio);
        const auto end=decode(argv[1]);
        if(with_audio) decode_audio(argv[1],end);
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n'; return 1;}
}
