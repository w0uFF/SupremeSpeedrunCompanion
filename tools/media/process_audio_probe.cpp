// Own-process audio activation and optional controlled-signal observation; no other process or fallback.
#define NOMINMAX
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <audioclientactivationparams.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <iostream>
#include <stdexcept>
#include <mmsystem.h>
#include <vector>
#include <string>
#include "src/modules/recorder/public/audio_observation.hpp"
#include "src/modules/recorder/public/quality.hpp"
using namespace Microsoft::WRL;
namespace {
struct Com {
    Com(){if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))throw std::runtime_error("COM initialization");}
    ~Com(){CoUninitialize();}
};
class Completion final:public RuntimeClass<RuntimeClassFlags<ClassicCom>,FtmBase,IActivateAudioInterfaceCompletionHandler> {
    HANDLE ready_=CreateEventW(nullptr,TRUE,FALSE,nullptr);
public:
    HRESULT result=E_PENDING;
    ComPtr<IAudioClient> client;
    ~Completion(){if(ready_)CloseHandle(ready_);}
    bool valid()const noexcept{return ready_!=nullptr;}
    DWORD wait() const noexcept{return WaitForSingleObject(ready_,5000);}
    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) override {
        HRESULT activation=E_FAIL;
        ComPtr<IUnknown> unknown;
        result=operation->GetActivateResult(&activation,unknown.GetAddressOf());
        if(SUCCEEDED(result))result=activation;
        if(SUCCEEDED(result))result=unknown?unknown.As(&client):E_POINTER;
        SetEvent(ready_);
        return S_OK;
    }
};
int report(HRESULT hr,const char* stage) {
    std::cerr<<stage<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';
    if(hr==E_NOTIMPL || hr==E_ACCESSDENIED || hr==REGDB_E_CLASSNOTREG ||
       hr==AUDCLNT_E_SERVICE_NOT_RUNNING || hr==AUDCLNT_E_DEVICE_INVALIDATED ||
       hr==HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)){
        std::cout<<"BLOCKED: process audio platform/service unavailable\n";return 77;
    }
    return 1;
}
// Own low-amplitude 1 kHz square wave; no child or unrelated app.
struct Playback {
    std::vector<std::int16_t> pcm;
    HWAVEOUT handle=nullptr;
    WAVEHDR header{};
    bool prepared=false;
    Playback():pcm(48000u*2u){
        for(std::size_t i=0;i<pcm.size()/2;++i){
            const auto v=static_cast<std::int16_t>((i/24u)%2u?1800:-1800);
            pcm[2*i]=pcm[2*i+1]=v;
        }
    }
    MMRESULT open(WAVEFORMATEX& format){
        return waveOutOpen(&handle,WAVE_MAPPER,&format,0,0,CALLBACK_NULL);
    }
    MMRESULT play(){
        header.lpData=reinterpret_cast<LPSTR>(pcm.data());
        header.dwBufferLength=static_cast<DWORD>(pcm.size()*sizeof(std::int16_t));
        auto rc=waveOutPrepareHeader(handle,&header,sizeof(header));
        if(rc!=MMSYSERR_NOERROR)return rc;
        prepared=true;
        return waveOutWrite(handle,&header,sizeof(header));
    }
    ~Playback(){
        if(handle){
            waveOutReset(handle);
            if(prepared)waveOutUnprepareHeader(handle,&header,sizeof(header));
            waveOutClose(handle);
        }
    }
};
struct CaptureStart {
    IAudioClient* client;
    bool started=false;
    ~CaptureStart(){if(started)client->Stop();}
};
int sample_signal(IAudioClient* client,IAudioCaptureClient* capture,WAVEFORMATEX& format){
    Playback playback;
    const auto opened=playback.open(format);
    if(opened!=MMSYSERR_NOERROR){
        std::cout<<"BLOCKED: own-signal playback endpoint unavailable, MMRESULT="<<opened<<'\n';return 77;
    }
    CaptureStart guard{client};
    LARGE_INTEGER freq{},epoch{},now{};
    if(!QueryPerformanceFrequency(&freq)||!QueryPerformanceCounter(&epoch)||freq.QuadPart<=0||epoch.QuadPart<0)return 1;
    const auto origin=supreme::recorder::qpc_to_100ns(0,static_cast<std::uint64_t>(epoch.QuadPart),static_cast<std::uint64_t>(freq.QuadPart));
    if(!origin)return 1;
    auto hr=client->Start();
    if(FAILED(hr))return report(hr,"start own-process capture");
    guard.started=true;
    const auto played=playback.play();
    if(played!=MMSYSERR_NOERROR){std::cerr<<"own-signal playback failed "<<played<<'\n';return 1;}
    supreme::recorder::AudioObservation observations;
    std::uint64_t positive=0,negative=0,valid_clock=0,clock_outliers=0,first_qpc=0,last_qpc=0;
    const auto deadline=GetTickCount64()+3500;
    while(GetTickCount64()<deadline){
        UINT32 available=0;
        hr=capture->GetNextPacketSize(&available);
        if(FAILED(hr))return report(hr,"get next own-process audio packet");
        if(!available){Sleep(5);continue;}
        BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;UINT64 position=0,qpc=0;
        hr=capture->GetBuffer(&data,&frames,&flags,&position,&qpc);
        if(FAILED(hr))return report(hr,"get own-process audio packet");
        if(!frames){continue;}
        const bool silent=(flags & AUDCLNT_BUFFERFLAGS_SILENT)!=0;
        const bool discontinuity=(flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)!=0;
        const bool timestamp_error=(flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)!=0;
        const bool shape=frames<=48000 && (silent || data!=nullptr);
        if(shape && !silent){
            for(UINT32 i=0;i<frames;++i){
                const auto offset=i*4u;
                const int raw=static_cast<int>(data[offset])+256*static_cast<int>(data[offset+1]);
                const int value=raw>=32768?raw-65536:raw;
                if(value>200)++positive;
                if(value< -200)++negative;
            }
        }
        hr=capture->ReleaseBuffer(frames);
        if(FAILED(hr))return report(hr,"release own-process audio packet");
        if(!shape || !observations.observe(position,frames,silent,discontinuity,timestamp_error))return 1;
        if(!timestamp_error){
            if(!QueryPerformanceCounter(&now)||now.QuadPart<0)return 1;
            const auto current=supreme::recorder::qpc_to_100ns(0,static_cast<std::uint64_t>(now.QuadPart),static_cast<std::uint64_t>(freq.QuadPart));
            if(!current)return 1;
            // Coarse QPC-domain sanity only, not A/V synchronization.
            if(qpc>*current+10000000 || (qpc<*origin && *origin-qpc>10000000) || (valid_clock && qpc<last_qpc))++clock_outliers;
            if(!valid_clock)first_qpc=qpc;
            last_qpc=qpc;++valid_clock;
        }
        if(positive>=2000 && negative>=2000 && valid_clock>=2)break;
    }
    std::cout<<"Own-process PCM packets="<<observations.packets<<", frames="<<observations.frames
             <<", silent_frames="<<observations.silent<<", positive="<<positive<<", negative="<<negative
             <<", discontinuities="<<observations.discontinuities<<", timestamp_errors="<<observations.timestamp_errors
             <<", position_gaps="<<observations.gaps<<", position_regressions="<<observations.regressions
             <<", valid_qpc="<<valid_clock<<", qpc_outliers="<<clock_outliers
             <<", first_qpc="<<first_qpc<<", last_qpc="<<last_qpc<<'\n';
    if(positive<2000 || negative<2000){
        std::cout<<"BLOCKED: own signal not observed in hosted audio environment\n";return 77;
    }
    if(valid_clock<2 || clock_outliers)return 1;
    std::cout<<"Own-process controlled PCM signal observed with QPC timestamp sanity check; no audio stored\n";
    return 0;
}
int run(bool sample){
    Com com;
    auto callback=Make<Completion>();
    if(!callback || !callback->valid())throw std::runtime_error("activation completion allocation");
    AUDIOCLIENT_ACTIVATION_PARAMS parameters{};
    parameters.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    parameters.ProcessLoopbackParams.TargetProcessId=GetCurrentProcessId();
    parameters.ProcessLoopbackParams.ProcessLoopbackMode=PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT activation{};
    activation.vt=VT_BLOB;
    activation.blob.cbSize=static_cast<ULONG>(sizeof(parameters));
    activation.blob.pBlobData=reinterpret_cast<BYTE*>(&parameters);
    ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
    auto hr=ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,__uuidof(IAudioClient),
                                        &activation,callback.Get(),operation.GetAddressOf());
    if(FAILED(hr))return report(hr,"activate own-process loopback");
    if(callback->wait()!=WAIT_OBJECT_0){
        std::cerr<<"activation timeout; no capture initialized\n";
        // Process exit prevents a late callback from using destroyed activation parameters.
        ExitProcess(1);
    }
    if(FAILED(callback->result))return report(callback->result,"activation completion");
    WAVEFORMATEX format{};
    format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=48000;
    format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=192000;
    hr=callback->client->Initialize(AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,10000000,0,&format,nullptr);
    if(FAILED(hr))return report(hr,"initialize process loopback");
    ComPtr<IAudioCaptureClient> capture;
    hr=callback->client->GetService(IID_PPV_ARGS(capture.GetAddressOf()));
    if(FAILED(hr))return report(hr,"get process capture service");
    if(sample)return sample_signal(callback->client.Get(),capture.Get(),format);
    std::cout<<"Own-process loopback activated and capture service obtained; 48 kHz stereo PCM; no audio packets read\n";
    return 0;
}
}
int main(int argc,char** argv){
    if(argc>2 || (argc==2 && std::string(argv[1])!="--sample"))return 2;
    try{return run(argc==2);}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
