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
#include <filesystem>
#include <fstream>
#include <vector>
#include "src/modules/recorder/public/pixel_conversion.hpp"
#include "src/modules/recorder/public/quality.hpp"
#include "src/modules/recorder/public/timed_frames.hpp"
namespace {
namespace cap=winrt::Windows::Graphics::Capture;
namespace dx=winrt::Windows::Graphics::DirectX;
struct Apartment {
    Apartment(){winrt::init_apartment(winrt::apartment_type::multi_threaded);}
    ~Apartment(){winrt::uninit_apartment();}
};
LRESULT CALLBACK paint_window(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_PAINT){
        PAINTSTRUCT ps{};auto dc=BeginPaint(hwnd,&ps);
        RECT rect{};GetClientRect(hwnd,&rect);
        const auto color=GetWindowLongPtrW(hwnd,GWLP_USERDATA)?WHITE_BRUSH:BLACK_BRUSH;
        FillRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(color)));
        EndPaint(hwnd,&ps);return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
struct Window {
    HWND handle=nullptr;
    Window(){
        WNDCLASSW cls{};cls.lpfnWndProc=paint_window;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"SSC_T20_OwnWindow";
        if(!RegisterClassW(&cls))return;
        handle=CreateWindowExW(0,cls.lpszClassName,L"SSC synthetic capture test",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                               40,40,320,240,nullptr,nullptr,cls.hInstance,nullptr);
        if(handle)UpdateWindow(handle);
    }
    ~Window(){if(handle)DestroyWindow(handle);}
    void bright(){SetWindowLongPtrW(handle,GWLP_USERDATA,1);InvalidateRect(handle,nullptr,FALSE);UpdateWindow(handle);}
};
// Map only a centered 160x96 crop of the own window. No desktop capture or scaling.
std::vector<std::uint8_t> read_crop(ID3D11Device* device,const cap::Direct3D11CaptureFrame& frame) {
    constexpr UINT w=160,h=96;
    auto access=frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<ID3D11Texture2D> texture;
    winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D),texture.put_void()));
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    const auto content=frame.ContentSize();
    if(desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM || desc.SampleDesc.Count!=1 || content.Width<static_cast<int>(w) ||
       content.Height<static_cast<int>(h) || static_cast<UINT>(content.Width)>desc.Width ||
       static_cast<UINT>(content.Height)>desc.Height)
        throw winrt::hresult_error(E_FAIL,L"unexpected texture layout");
    const auto x=(static_cast<UINT>(content.Width)-w)/2,y=(static_cast<UINT>(content.Height)-h)/2;
    D3D11_BOX box{x,y,0,x+w,y+h,1};
    D3D11_TEXTURE2D_DESC crop{};crop.Width=w;crop.Height=h;crop.MipLevels=1;crop.ArraySize=1;
    crop.Format=desc.Format;crop.SampleDesc.Count=1;crop.Usage=D3D11_USAGE_STAGING;crop.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    winrt::com_ptr<ID3D11Texture2D> staging;
    winrt::check_hresult(device->CreateTexture2D(&crop,nullptr,staging.put()));
    winrt::com_ptr<ID3D11DeviceContext> context;device->GetImmediateContext(context.put());
    context->CopySubresourceRegion(staging.get(),0,0,0,0,texture.get(),0,&box);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    winrt::check_hresult(context->Map(staging.get(),0,D3D11_MAP_READ,0,&mapped));
    try{
        if(mapped.RowPitch<4*w || mapped.RowPitch>1024*1024)throw std::runtime_error("invalid mapped row pitch");
        auto data=supreme::recorder::bgra_to_nv12(static_cast<const std::uint8_t*>(mapped.pData),
                         static_cast<std::size_t>(mapped.RowPitch)*h,mapped.RowPitch,w,h);
        context->Unmap(staging.get(),0);return data;
    }catch(...){context->Unmap(staging.get(),0);throw;}
}
bool matches(const std::vector<std::uint8_t>& data,bool bright){
    constexpr std::size_t plane=160*96;
    if(data.size()!=plane+plane/2)return false;
    for(std::size_t i=0;i<plane;++i)if(bright?data[i]<220:data[i]>35)return false;
    for(std::size_t i=plane;i<data.size();++i)if(data[i]<120 || data[i]>136)return false;
    return true;
}
void pump() {
    MSG msg{};
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
}
int run(const wchar_t* output,bool timed=false) {
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
    std::vector<std::uint8_t> dark,light;
    std::vector<supreme::recorder::TimedFrame> sequence;
    unsigned skipped_pattern=0;
    constexpr std::size_t target=12;
    const auto deadline=GetTickCount64()+8000;
    while(GetTickCount64()<deadline && (timed?sequence.size()<target:output?light.empty():frames<3)){
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
            if(output){
                auto pixels=read_crop(device.get(),frame);
                if(timed){
                    const bool expected=sequence.size()>=target/2;
                    if(matches(pixels,expected)){
                        sequence.push_back({static_cast<std::uint64_t>(time),std::move(pixels)});
                        if(sequence.size()==target/2)window.bright();
                    }else ++skipped_pattern;
                }else if(dark.empty() && matches(pixels,false)){dark=std::move(pixels);window.bright();}
                else if(!dark.empty() && matches(pixels,true))light=std::move(pixels);
            }
            frame.Close();
        }
        SetWindowTextW(window.handle,(GetTickCount64()/100)%2?L"SSC synthetic capture A":L"SSC synthetic capture B");
        InvalidateRect(window.handle,nullptr,TRUE);
        Sleep(25);
    }
    session.Close();pool.Close();
    if(timed){
        if(sequence.size()!=target){
            std::cerr<<"timed own-window frame pattern incomplete, selected="<<sequence.size()<<" observed="<<frames<<'\n';
            return frames?1:77;
        }
        if(std::filesystem::exists(std::filesystem::symlink_status(output)))return 2;
        std::ofstream file(std::filesystem::path(output),std::ios::binary);
        supreme::recorder::write_timed_frames(file,sequence);
        file.close();if(!file)return 1;
        std::cout<<"TimedWGC "<<sequence.size()<<" "<<frames<<" "<<skipped_pattern<<" "<<sequence.front().time<<" "<<sequence.back().time<<"\n";
        return 0;
    }
    if(output){
        if(dark.empty() || light.empty()){
            std::cerr<<"own-window pixel pattern missing, frames="<<frames<<'\n';
            return frames?1:77;
        }
        if(std::filesystem::exists(std::filesystem::symlink_status(output)))return 2;
        std::ofstream file(std::filesystem::path(output),std::ios::binary);
        for(const auto* data:{&dark,&light})file.write(reinterpret_cast<const char*>(data->data()),static_cast<std::streamsize>(data->size()));
        file.close();if(!file)return 1;
        std::cout<<"WGC actual dark and bright 160x96 crops converted to NV12, all luma/chroma samples verified\n";
        return 0;
    }
    if(frames<3){
        std::cout<<"BLOCKED: only "<<frames<<" own-window frames received; interactive compositor test required\n";return 77;
    }
    std::cout<<"WGC own-window frame metadata: "<<frames<<" frames, "<<size.Width<<"x"<<size.Height
             <<", increasing QPC-correlated timestamps (coarse one-second sanity bound)\n";
    return 0;
}
}
int wmain(int argc,wchar_t** argv){
    const bool timed=argc==3 && std::wstring_view(argv[1])==L"--timed";
    if(argc>2 && !timed)return 2;
    try{return run(timed?argv[2]:argc==2?argv[1]:nullptr,timed);}
    catch(const winrt::hresult_error& e){
        const auto hr=e.code().value;
        std::cerr<<"WGC HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';
        if(hr==E_ACCESSDENIED || hr==E_NOTIMPL || hr==REGDB_E_CLASSNOTREG){std::cout<<"BLOCKED: capture platform/policy unavailable\n";return 77;}
        return 1;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
