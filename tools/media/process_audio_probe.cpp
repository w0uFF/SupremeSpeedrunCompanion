// Standalone activation feasibility only; no recording, no other process, no endpoint-wide fallback.
#define NOMINMAX
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <audioclientactivationparams.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <iostream>
#include <stdexcept>
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
int run(){
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
    std::cout<<"Own-process loopback activated and capture service obtained; 48 kHz stereo PCM; no audio packets read\n";
    return 0;
}
}
int main(){
    try{return run();}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
