// Standalone Windows synthetic encode/decode probe. No game or capture access.
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include "tools/media/fixture_performance.hpp"
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
#include "tools/media/mf_fixture_sink.hpp"
#include "src/modules/recorder/public/frame_lookahead.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using Microsoft::WRL::ComPtr;
namespace {
using namespace ssc::media_fixture;
UINT32 frames=12;
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
std::uint64_t cpu_100ns() {
    FILETIME creation{},exit_time{},kernel{},user{};
    if(!GetProcessTimes(GetCurrentProcess(),&creation,&exit_time,&kernel,&user))
        throw std::runtime_error("process CPU time unavailable");
    const auto units=[](const FILETIME& f){
        return (static_cast<std::uint64_t>(f.dwHighDateTime)<<32u)|f.dwLowDateTime;
    };
    return units(kernel)+units(user);
}
void encode_hd(const wchar_t* path) {
    using namespace supreme::recorder;
    using Clock=std::chrono::steady_clock;
    const auto us=[](Clock::time_point start,Clock::time_point end){
        return std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
    };
    constexpr auto spec=hd_spec;
    constexpr std::size_t slots=8;
    const auto start=Clock::now();
    const auto cpu_start=cpu_100ns();
    AsyncSink sink(path,false,spec);
    Worker worker(sink,slots,spec.frame_bytes);
    if(worker.wait_ready()!=State::running)throw std::runtime_error("HD worker start");
    const auto epoch=Clock::now();
    std::vector<std::int64_t> latencies;
    latencies.reserve(frames);
    for(UINT32 i=0;i<frames;++i){
        std::this_thread::sleep_until(epoch+std::chrono::milliseconds(40)*i);
        const auto begin=Clock::now();
        const auto pts=static_cast<std::uint64_t>(i)*static_cast<std::uint64_t>(period);
        Packet frame{Stream::video,pts,static_cast<std::uint64_t>(period),std::vector<std::uint8_t>(spec.frame_bytes,128)};
        // Moving checker bands; first two 64x32 tiles retain independently checked luma.
        for(UINT32 y=0;y<spec.frame_height;++y){
            for(UINT32 x=0;x<spec.frame_width;x+=64u){
                const auto parity=(x/64u+y/32u+(y<32u?0u:i/5u))%2u;
                const bool bright=(i>=frames/2u)!=(parity!=0u);
                std::fill_n(frame.bytes.data()+y*spec.frame_width+x,64u,static_cast<BYTE>(bright?192:48));
            }
        }
        if(worker.offer(std::move(frame))!=Offer::accepted)throw std::runtime_error("HD worker offer");
        latencies.push_back(us(begin,Clock::now()));
    }
    const auto drain=Clock::now();
    worker.stop();worker.join();
    const auto end=Clock::now();
    const auto cpu_end=cpu_100ns();
    const auto stats=worker.stats();
    const auto q=worker.quality()[0];
    const auto expected=static_cast<std::uint64_t>(frames)*static_cast<std::uint64_t>(period);
    if(worker.state()!=State::finalized || stats.accepted!=frames || stats.written!=frames ||
       stats.dropped_full || stats.discarded || stats.rejected_invalid || q.written.first!=0 ||
       q.written.end!=expected || q.written.duration!=expected || q.written.internal_gap()!=0 ||
       cpu_end<cpu_start)throw std::runtime_error("HD worker quality");
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb=static_cast<DWORD>(sizeof(memory));
    if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),memory.cb))
        throw std::runtime_error("process memory counters unavailable");
    const auto size=std::filesystem::file_size(std::filesystem::path(path));
    if(!size || size>16u*1024u*1024u)throw std::runtime_error("HD output size");
    const auto summary=summarize_latency(std::move(latencies));
    std::cout<<"HDPerformance "<<summary.count<<" "<<us(start,end)<<" "<<cpu_end-cpu_start<<" "
             <<summary.median_us<<" "<<summary.p95_us<<" "<<summary.p99_us<<" "<<summary.max_us<<" "
             <<us(drain,end)<<" "<<memory.PeakWorkingSetSize<<" "<<memory.WorkingSetSize<<" "
             <<memory.PrivateUsage<<" "<<size<<" "<<worker.high_water()<<" "<<slots*spec.frame_bytes<<"\n";
}
LONGLONG decode(const wchar_t* path,const std::vector<supreme::recorder::FrameInterval>* timeline=nullptr,
                VideoSpec spec=low_spec,bool texture=false) {
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
    if(w!=spec.frame_width || h!=spec.frame_height) throw std::runtime_error("unexpected dimensions");
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
        const int luma=length>=spec.frame_bytes?data[0]:-1;
        const int alternate=length>=spec.frame_bytes?data[64]:-1;
        check(buffer->Unlock(),"unlock decoded frame");
        if(luma<0 || (count<frames/2?luma>=100:luma<=140))
            throw std::runtime_error("decoded luma pattern mismatch");
        if(texture && (alternate<0 || (count<frames/2?alternate<=140:alternate>=100)))
            throw std::runtime_error("decoded HD alternate tile mismatch");
        ++count;
    }
    if(!eos || count!=frames) throw std::runtime_error("frame count or EOS mismatch");
    std::cout<<"Decoded "<<count<<" H264 frames, "<<spec.frame_width<<'x'<<spec.frame_height<<", monotonic timestamps and luma pattern verified\n";
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
    const bool hd=argc==3 && std::wstring(argv[2])==L"--hd720";
    const bool live_hd=argc==4 && std::wstring(argv[2])==L"--decode-live-hd720";
    const bool live_decode=live_hd || (argc==4 && std::wstring(argv[2])==L"--decode-live");
    const bool timed=argc==4 && std::wstring(argv[2])==L"--timed-video";
    const bool extended=argc==3 && std::wstring(argv[2])==L"--extended-av";
    const bool sustained=extended || (argc==3 && std::wstring(argv[2])==L"--sustained-av");
    const bool captured=argc==4 && std::wstring(argv[2])==L"--captured-av";
    const bool asynchronous=captured || sustained || (argc==3 && std::wstring(argv[2])==L"--async-av");
    if(sustained) frames=extended?750u:250u;
    const bool with_audio=asynchronous || (argc==3 && std::wstring(argv[2])==L"--av");
    if(argc<2 || (argc!=2 && !with_audio && !timed && !live_decode && !hd))return 2;
    if((GetFileAttributesW(argv[1])!=INVALID_FILE_ATTRIBUTES)!=live_decode)return 2;
    try {
        Com com; Foundation mf;
        if(hd){
            frames=250;
            encode_hd(argv[1]);
            std::vector<supreme::recorder::FrameInterval> timeline;
            timeline.reserve(frames);
            for(UINT32 i=0;i<frames;++i)
                timeline.push_back({static_cast<std::uint64_t>(i)*static_cast<std::uint64_t>(period),
                                    static_cast<std::uint64_t>(period)});
            (void)decode(argv[1],&timeline,hd_spec,true);
            return 0;
        }
        if(live_decode){
            std::ifstream input(std::filesystem::path(argv[3]),std::ios::binary);
            std::vector<supreme::recorder::FrameInterval> timeline;
            std::uint64_t pts=0,duration=0;
            supreme::recorder::Coverage coverage;
            while(input>>pts>>duration){
                if(timeline.size()==supreme::recorder::FrameLookahead::max_frames || pts>supreme::recorder::FrameLookahead::max_span || duration>supreme::recorder::fixture_max_hold || !coverage.observe(pts,duration))throw std::runtime_error("live decode timeline");
                timeline.push_back({pts,duration});
            }
            if(!input.eof() || (timeline.size()!=60 && timeline.size()!=240 && timeline.size()!=600 && timeline.size()!=1200) || timeline.front().pts!=0 || timeline.back().duration!=supreme::recorder::fixture_tail)throw std::runtime_error("live decode bounds");
            if(live_hd && timeline.size()!=240 && timeline.size()!=600 && timeline.size()!=1200)throw std::runtime_error("HD capture frame count");
            frames=static_cast<UINT32>(timeline.size());
            (void)decode(argv[1],&timeline,live_hd?hd_spec:low_spec);return 0;
        }
        if(timed){
            std::ifstream input(std::filesystem::path(argv[3]),std::ios::binary);
            const auto timed_frames=supreme::recorder::read_timed_frames(input);
            const auto intervals=supreme::recorder::frame_intervals(timed_frames);
            frames=static_cast<UINT32>(timed_frames.size());
            encode_timed(argv[1],timed_frames);
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
