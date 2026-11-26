// Isolated feasibility probe: only a window created by this executable.
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <iostream>
#include "src/modules/recorder/public/quality.hpp"
namespace {
namespace cap=winrt::Windows::Graphics::Capture;
namespace dx=winrt::Windows::Graphics::DirectX;
struct Apartment {
    Apartment(){winrt::init_apartment(winrt::apartment_type::multi_threaded);}
    ~Apartment(){winrt::uninit_apartment();}
};
struct Window {
    HWND handle=CreateWindowExW(0,L"STATIC",L"SSC synthetic capture test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                               40,40,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    ~Window(){if(handle)DestroyWindow(handle);}
};
void pump() {
    MSG msg{};
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
}
int run() {
    HDESK desktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
    if(!desktop){std::cout<<"BLOCKED: input desktop unavailable, Win32="<<GetLastError()<<'\n';return 77;}
    CloseDesktop(desktop);
    Apartment apartment;
    if(!cap::GraphicsCaptureSession::IsSupported()){
        std::cout<<"BLOCKED: Windows Graphics Capture unsupported\n";return 77;
    }
    Window window;
    if(!window.handle){std::cerr<<"test window creation failed "<<GetLastError()<<'\n';return 1;}
    pump();
    cap::GraphicsCaptureItem item{nullptr};
    auto interop=winrt::get_activation_factory<cap::GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
    winrt::check_hresult(interop->CreateForWindow(window.handle,winrt::guid_of<cap::GraphicsCaptureItem>(),winrt::put_abi(item)));
    const auto size=item.Size();
    if(size.Width<=0 || size.Height<=0 || size.Width>1024 || size.Height>1024)
        throw winrt::hresult_error(E_FAIL,L"unexpected capture item size");
    winrt::com_ptr<ID3D11Device> device;
    auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                            nullptr,0,D3D11_SDK_VERSION,device.put(),nullptr,nullptr);
    if(FAILED(hr)){
        device=nullptr;
        winrt::check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                                nullptr,0,D3D11_SDK_VERSION,device.put(),nullptr,nullptr));
        std::cout<<"D3D11 WARP fallback\n";
    }
    auto dxgi=device.as<IDXGIDevice>();
    winrt::com_ptr<::IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(),inspectable.put()));
    auto runtime_device=inspectable.as<dx::Direct3D11::IDirect3DDevice>();
    auto pool=cap::Direct3D11CaptureFramePool::CreateFreeThreaded(runtime_device,dx::DirectXPixelFormat::B8G8R8A8UIntNormalized,2,size);
    auto session=pool.CreateCaptureSession(item);
    LARGE_INTEGER frequency{},start{},now{};
    if(!QueryPerformanceFrequency(&frequency)||!QueryPerformanceCounter(&start)||frequency.QuadPart<=0||start.QuadPart<0)
        throw winrt::hresult_error(E_FAIL,L"QPC unavailable");
    auto begin=supreme::recorder::qpc_to_100ns(0,static_cast<std::uint64_t>(start.QuadPart),
                                              static_cast<std::uint64_t>(frequency.QuadPart));
    if(!begin)throw winrt::hresult_error(E_FAIL,L"QPC conversion unavailable");
    session.StartCapture();
    unsigned frames=0;std::int64_t last=-1;
    const auto deadline=GetTickCount64()+5000;
    while(GetTickCount64()<deadline && frames<3){
        pump();
        if(auto frame=pool.TryGetNextFrame()){
            const auto time=frame.SystemRelativeTime().count();
            const auto content=frame.ContentSize();
            if(!QueryPerformanceCounter(&now)||now.QuadPart<0)
                throw winrt::hresult_error(E_FAIL,L"QPC unavailable");
            const auto current=supreme::recorder::qpc_to_100ns(0,static_cast<std::uint64_t>(now.QuadPart),
                                                              static_cast<std::uint64_t>(frequency.QuadPart));
            if(!current || time<=last || time<0 || content.Width!=size.Width || content.Height!=size.Height ||
               static_cast<std::uint64_t>(time)>*current+10000000 ||
               (static_cast<std::uint64_t>(time)<*begin && *begin-static_cast<std::uint64_t>(time)>10000000))
                throw winrt::hresult_error(E_FAIL,L"frame dimensions or timestamp mismatch");
            last=time;++frames;
            frame.Close();
        }
        SetWindowTextW(window.handle,(GetTickCount64()/100)%2?L"SSC synthetic capture A":L"SSC synthetic capture B");
        InvalidateRect(window.handle,nullptr,TRUE);
        Sleep(25);
    }
    session.Close();pool.Close();
    if(frames<3){
        std::cout<<"BLOCKED: only "<<frames<<" own-window frames received; interactive compositor test required\n";return 77;
    }
    std::cout<<"WGC own-window frame metadata: "<<frames<<" frames, "<<size.Width<<"x"<<size.Height
             <<", increasing QPC-correlated timestamps (coarse one-second sanity bound)\n";
    return 0;
}
}
int main(){
    try{return run();}
    catch(const winrt::hresult_error& e){
        const auto hr=e.code().value;
        std::cerr<<"WGC HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';
        if(hr==E_ACCESSDENIED || hr==E_NOTIMPL || hr==REGDB_E_CLASSNOTREG){std::cout<<"BLOCKED: capture platform/policy unavailable\n";return 77;}
        return 1;
    }
}
