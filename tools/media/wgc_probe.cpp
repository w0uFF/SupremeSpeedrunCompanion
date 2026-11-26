// Isolated feasibility probe: only a window created by this executable.
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <iostream>
#include <chrono>
#include "tools/media/fixture_performance.hpp"
#include <filesystem>
#include <array>
#include <fstream>
#include <vector>
#include "src/modules/recorder/public/pixel_conversion.hpp"
#include "src/modules/recorder/public/quality.hpp"
#include "src/modules/recorder/public/timed_frames.hpp"
#include "src/modules/recorder/public/frame_lookahead.hpp"
#include "tools/media/mf_fixture_sink.hpp"
#include "tools/media/fixture_isolation.hpp"
#include <string_view>
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
    ATOM registered=0;
    explicit Window(bool hd=false){
        WNDCLASSW cls{};cls.lpfnWndProc=paint_window;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"SSC_T20_OwnWindow";
        registered=RegisterClassW(&cls);
        if(!registered)return;
        const DWORD style=hd?static_cast<DWORD>(WS_POPUP|WS_VISIBLE):static_cast<DWORD>(WS_OVERLAPPEDWINDOW|WS_VISIBLE);
        handle=CreateWindowExW(0,cls.lpszClassName,L"SSC synthetic capture test",style,
                               hd?0:40,hd?0:40,hd?1280:320,hd?720:240,nullptr,nullptr,cls.hInstance,nullptr);
        if(handle)UpdateWindow(handle);
    }
    ~Window(){if(handle)DestroyWindow(handle);if(registered)UnregisterClassW(L"SSC_T20_OwnWindow",GetModuleHandleW(nullptr));}
    void bright(){SetWindowLongPtrW(handle,GWLP_USERDATA,1);InvalidateRect(handle,nullptr,FALSE);UpdateWindow(handle);}
};
// Map only a bounded centered crop of the own window. No desktop capture or scaling.
std::vector<std::uint8_t> read_crop(ID3D11Device* device,const cap::Direct3D11CaptureFrame& frame,ssc::media_fixture::VideoSpec spec=ssc::media_fixture::low_spec) {
    const UINT w=spec.frame_width,h=spec.frame_height;
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
bool matches(const std::vector<std::uint8_t>& data,bool bright,ssc::media_fixture::VideoSpec spec=ssc::media_fixture::low_spec){
    const std::size_t plane=static_cast<std::size_t>(spec.frame_width)*spec.frame_height;
    if(data.size()!=plane+plane/2)return false;
    for(std::size_t i=0;i<plane;++i)if(bright?data[i]<220:data[i]>35)return false;
    for(std::size_t i=plane;i<data.size();++i)if(data[i]<120 || data[i]>136)return false;
    return true;
}
// Bounded spatial diagnostics for a rejected bright frame, not acceptance criteria.
void diagnose_bright(const std::vector<std::uint8_t>& data,ssc::media_fixture::VideoSpec spec){
    const auto w=static_cast<std::size_t>(spec.frame_width),h=static_cast<std::size_t>(spec.frame_height);
    if(data.size()!=w*h*3/2)return;
    std::size_t bright=0,neutral_errors=0;
    std::size_t min_x=w,max_x=0;
    for(std::size_t x=0;x<w;++x)for(std::size_t y=0;y<h;++y){
        if(data[y*w+x]>=220)++bright;
        else{min_x=std::min(min_x,x);max_x=std::max(max_x,x);}
    }
    for(std::size_t i=w*h;i<data.size();++i)if(data[i]<120 || data[i]>136)++neutral_errors;
    std::cout<<"HDBrightMismatch "<<bright<<" "<<w*h<<" "<<min_x<<" "<<max_x<<" "<<neutral_errors;
    for(auto x:{std::size_t{0},w/4,w/2,w*3/4,w-1})
        std::cout<<" "<<static_cast<unsigned>(data[(h/2)*w+x]);
    std::cout<<std::endl;
}
void pump() {
    MSG msg{};
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
}
using FixtureClock=std::chrono::steady_clock;
std::int64_t elapsed_us(FixtureClock::time_point start,FixtureClock::time_point end){
    return std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
}
std::uint64_t process_cpu_100ns(){
    FILETIME creation{},exit_time{},kernel{},user{};
    if(!GetProcessTimes(GetCurrentProcess(),&creation,&exit_time,&kernel,&user))
        throw std::runtime_error("fixture process CPU time unavailable");
    const auto units=[](const FILETIME& value){
        return (static_cast<std::uint64_t>(value.dwHighDateTime)<<32u)|value.dwLowDateTime;
    };
    return units(kernel)+units(user);
}
struct CaptureDevice {
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<IDXGIDevice> dxgi;
    winrt::com_ptr<::IInspectable> inspectable;
    dx::Direct3D11::IDirect3DDevice runtime{nullptr};
};
CaptureDevice make_capture_device(){
    CaptureDevice result;
    auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                             nullptr,0,D3D11_SDK_VERSION,result.device.put(),nullptr,nullptr);
    if(FAILED(hr)){
        result.device=nullptr;
        winrt::check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                                nullptr,0,D3D11_SDK_VERSION,result.device.put(),nullptr,nullptr));
        std::cout<<"D3D11 WARP fallback\n";
    }
    result.dxgi=result.device.as<IDXGIDevice>();
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(result.dxgi.get(),result.inspectable.put()));
    result.runtime=result.inspectable.as<dx::Direct3D11::IDirect3DDevice>();
    return result;
}
struct CloseCapture {
    cap::GraphicsCaptureSession& session;
    cap::Direct3D11CaptureFramePool& pool;
    bool session_closed=false,pool_closed=false;
    void close(){
        if(!session_closed){session.Close();session_closed=true;}
        if(!pool_closed){pool.Close();pool_closed=true;}
    }
    ~CloseCapture(){if(!session_closed)try{session.Close();}catch(...){}if(!pool_closed)try{pool.Close();}catch(...){}}
};
void isolate_capture_scope(std::wstring_view mode){
    if(mode==L"device"){
        auto device=make_capture_device();
        if(!device.runtime)throw std::runtime_error("isolated runtime device missing");
        return;
    }
    if(mode==L"support-only"){
        if(!cap::GraphicsCaptureSession::IsSupported())throw std::runtime_error("isolated WGC unavailable");
        return;
    }
    Window window;
    if(!window.handle)throw std::runtime_error("isolated own-window creation");
    pump();
    if(mode==L"window")return;
    if((mode!=L"pool" && mode!=L"item-only" && mode!=L"framepool-only") || !cap::GraphicsCaptureSession::IsSupported())throw std::runtime_error("isolated WGC unavailable");
    cap::GraphicsCaptureItem item{nullptr};
    auto factory=winrt::get_activation_factory<cap::GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
    winrt::check_hresult(factory->CreateForWindow(window.handle,winrt::guid_of<cap::GraphicsCaptureItem>(),winrt::put_abi(item)));
    const auto size=item.Size();
    if(size.Width<=0 || size.Height<=0 || size.Width>1024 || size.Height>1024)throw std::runtime_error("isolated item bounds");
    if(mode==L"item-only")return;
    auto device=make_capture_device();
    auto pool=cap::Direct3D11CaptureFramePool::CreateFreeThreaded(device.runtime,dx::DirectXPixelFormat::B8G8R8A8UIntNormalized,2,size);
    if(mode==L"framepool-only"){pool.Close();return;}
    auto session=pool.CreateCaptureSession(item);
    CloseCapture close{session,pool};
    close.close(); // Never start capture: this is setup/teardown isolation.
}
int isolate_retention(std::wstring_view mode,const std::filesystem::path& directory,bool extended=false){
    using namespace ssc::isolation_fixture;
    const bool allowed=extended?(mode==L"support-only" || mode==L"item-only" || mode==L"framepool-only" || mode==L"pool"):
        (mode==L"window" || mode==L"device" || mode==L"pool" || mode==L"worker-idle" || mode==L"worker-write");
    if(!std::filesystem::is_directory(directory) || std::filesystem::is_symlink(directory) || !allowed)return 2;
    const unsigned cycles=extended?6u:3u;
    const auto label=winrt::to_string(winrt::hstring(mode));
    std::cout<<"IsolationBegin "<<label<<std::endl;
    for(unsigned cycle=1;cycle<=cycles;++cycle){
        sample(cycle,0);
        Outcome result;
        if(mode==L"worker-idle" || mode==L"worker-write")
            result=worker_scope(directory/(L"worker-"+std::to_wstring(cycle)+L".mp4"),mode==L"worker-write");
        else isolate_capture_scope(mode);
        print_outcome(cycle,result);
        sample(cycle,1);
        Sleep(1000);pump();sample(cycle,2);
        Sleep(2000);pump();sample(cycle,3);
    }
    std::cout<<"IsolationEnd "<<label<<" "<<cycles<<std::endl;
    return 0;
}
enum class FixtureStop { none, user_cancel, simulated_device_loss, sink_write_failure, sink_finalize_failure };
// Own-process, bounded stage diagnostics; no system-wide handle inspection.
void stage_resources(unsigned stage){
    PROCESS_MEMORY_COUNTERS_EX m{};
    m.cb=static_cast<DWORD>(sizeof(m));
    DWORD handles=0;
    if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m),m.cb)
       || !GetProcessHandleCount(GetCurrentProcess(),&handles))
        throw std::runtime_error("stage resource sample unavailable");
    std::cout<<"HDStageResources "<<stage<<" "<<GetCurrentProcessId()<<" "<<m.PeakWorkingSetSize
             <<" "<<m.WorkingSetSize<<" "<<m.PrivateUsage<<" "<<handles<<std::endl;
}
int run(const wchar_t* output,bool timed=false,bool live=false,bool resize=false,bool long_fixture=false,bool extended=false,bool simulated_loss=false,bool marathon=false,bool hd=false,bool hd_extended=false,bool hd_sustained=false,FixtureStop stop=FixtureStop::none,bool retention=false) {
    if(stop!=FixtureStop::none && (!hd || !live || !hd_extended || hd_sustained))return 2;
    HDESK desktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
    if(!desktop){std::cout<<"BLOCKED: input desktop unavailable, Win32="<<GetLastError()<<'\n';return 77;}
    CloseDesktop(desktop);
    // The calling thread owns one apartment for its entire probe lifetime.
    if(hd)std::cout<<"HDStage support-check"<<std::endl;
    if(!cap::GraphicsCaptureSession::IsSupported()){
        std::cout<<"BLOCKED: Windows Graphics Capture unsupported\n";return 77;
    }
    const auto spec=hd?ssc::media_fixture::hd_spec:ssc::media_fixture::low_spec;
    if(hd)std::cout<<"HDStage create-window"<<std::endl;
    if(retention)stage_resources(0);
    Window window(hd);
    if(!window.handle){std::cerr<<"test window creation failed "<<GetLastError()<<'\n';return 1;}
    if(retention)stage_resources(1);
    if(hd)std::cout<<"HDStage pump-window"<<std::endl;
    pump();
    if(hd)std::cout<<"HDStage activation-factory"<<std::endl;
    cap::GraphicsCaptureItem item{nullptr};
    auto interop=winrt::get_activation_factory<cap::GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
    if(hd)std::cout<<"HDStage capture-item"<<std::endl;
    winrt::check_hresult(interop->CreateForWindow(window.handle,winrt::guid_of<cap::GraphicsCaptureItem>(),winrt::put_abi(item)));
    if(hd)std::cout<<"HDStage capture-item-size"<<std::endl;
    const auto size=item.Size();
    if(hd){
        RECT client{}, outer{};
        if(!GetClientRect(window.handle,&client) || !GetWindowRect(window.handle,&outer))
            throw std::runtime_error("HD own-window geometry unavailable");
        std::cout<<"HDWindowGeometry "<<client.right-client.left<<" "<<client.bottom-client.top<<" "
                 <<outer.right-outer.left<<" "<<outer.bottom-outer.top<<" "<<size.Width<<" "<<size.Height
                 <<" "<<GetSystemMetrics(SM_CXSCREEN)<<" "<<GetSystemMetrics(SM_CYSCREEN)<<std::endl;
    }
    if(size.Width<=0 || size.Height<=0 || size.Width>(hd?1536:1024) || size.Height>1024 ||
       (hd && (size.Width<1280 || size.Height<720)))
        throw winrt::hresult_error(E_FAIL,L"unexpected capture item size");
    if(hd)std::cout<<"HDStage create-device"<<std::endl;
    auto capture_device=make_capture_device();
    auto& device=capture_device.device;
    auto& runtime_device=capture_device.runtime;
    if(retention)stage_resources(2);
    if(hd)std::cout<<"HDStage create-pool"<<std::endl;
    auto pool=cap::Direct3D11CaptureFramePool::CreateFreeThreaded(runtime_device,dx::DirectXPixelFormat::B8G8R8A8UIntNormalized,2,size);
    if(hd)std::cout<<"HDStage create-session"<<std::endl;
    auto session=pool.CreateCaptureSession(item);
    if(retention)stage_resources(3);
    LARGE_INTEGER frequency{},start{},now{};
    if(!QueryPerformanceFrequency(&frequency)||!QueryPerformanceCounter(&start)||frequency.QuadPart<=0||start.QuadPart<0)
        throw winrt::hresult_error(E_FAIL,L"QPC unavailable");
    auto begin=supreme::recorder::qpc_to_100ns(0,static_cast<std::uint64_t>(start.QuadPart),
                                              static_cast<std::uint64_t>(frequency.QuadPart));
    if(!begin)throw winrt::hresult_error(E_FAIL,L"QPC conversion unavailable");
    // Worker starts before acquisition; sink is owned longer than Worker.
    std::unique_ptr<ssc::media_fixture::AsyncSink> sink;
    std::unique_ptr<supreme::recorder::Worker> worker;
    supreme::recorder::FrameLookahead lookahead(spec.frame_bytes);
    supreme::recorder::Coverage expected_coverage;
    if(live){
        if(hd)std::cout<<"HDStage start-worker"<<std::endl;
        if(std::filesystem::exists(std::filesystem::symlink_status(output)))return 2;
        sink=std::make_unique<ssc::media_fixture::AsyncSink>(output,false,spec,stop==FixtureStop::sink_write_failure?119u:0u,stop==FixtureStop::sink_finalize_failure);
        worker=std::make_unique<supreme::recorder::Worker>(*sink,8,hd?spec.frame_bytes:32768u);
        if(worker->wait_ready()!=supreme::recorder::State::running)throw std::runtime_error("live worker startup");
    }
    if(retention)stage_resources(4);
    auto offer=[&](supreme::recorder::Packet packet){
        if(!expected_coverage.observe(packet.pts,packet.duration))throw std::runtime_error("live handoff interval");
        const auto result=worker->offer(std::move(packet));
        if(result!=supreme::recorder::Offer::accepted){
            std::cerr<<"LiveOfferFailure "<<static_cast<int>(result)<<" dropped="<<worker->stats().dropped_full<<'\n';
            throw std::runtime_error("live worker rejected packet");
        }
    };
    CloseCapture close_capture{session,pool};
    const auto stop_hd=[&](const char* reason,int status){
        // Only the explicit bounded fixture paths enter here; no terminal packet
        // or sink finalization. The pending lookahead frame remains unoffered.
        close_capture.close();
        if(retention)stage_resources(5);
        const bool failure=stop==FixtureStop::sink_write_failure;
        if(failure)worker->stop();else worker->cancel();
        worker->join();
        if(retention)stage_resources(6);
        const auto stats=worker->stats();
        const auto q=worker->quality()[0];
        const auto selected=lookahead.count();
        const auto high=worker->high_water();
        const auto expected_state=failure?supreme::recorder::State::failed:supreme::recorder::State::cancelled;
        if(worker->state()!=expected_state || sink->injected_failure()!=failure ||
           (failure && (stats.written!=118 || stats.discarded!=1)) || selected!=120 ||
           stats.accepted!=selected-1 || !stats.written ||
           stats.written+stats.discarded!=stats.accepted || stats.dropped_full || stats.rejected_invalid ||
           q.accepted!=stats.accepted || q.written.packets!=stats.written ||
           q.discarded!=stats.discarded || q.dropped_full || q.rejected_invalid || !high || high>8)
            throw std::runtime_error("HD cancellation accounting");
        const auto path=std::filesystem::path(output);
        const bool exists=std::filesystem::exists(std::filesystem::symlink_status(path));
        const auto bytes=exists?std::filesystem::file_size(path):0;
        if(bytes>16u*1024u*1024u)throw std::runtime_error("HD cancelled output bound");
        std::cout<<"HDFaultOutcome "<<reason<<(failure?" failed ":" cancelled ")<<selected<<" "<<stats.accepted<<" "
                 <<stats.written<<" "<<stats.discarded<<" "<<selected-stats.accepted<<" "
                 <<high<<" "<<stats.dropped_full<<" "<<stats.rejected_invalid<<std::endl;
        std::cout<<"HDFaultArtifact "<<(exists?1:0)<<" "<<bytes<<std::endl;
        return status;
    };
    const auto fixture_wall_begin=FixtureClock::now();
    const auto cpu_begin=(marathon || hd)?process_cpu_100ns():std::uint64_t{0};
    if(hd)std::cout<<"HDStage start-capture"<<std::endl;
    session.StartCapture();
    unsigned frames=0;std::int64_t last=-1;
    std::vector<std::uint8_t> dark,light;
    std::vector<supreme::recorder::TimedFrame> sequence;
    unsigned skipped_pattern=0;
    const std::size_t target=stop==FixtureStop::sink_finalize_failure?120u:hd_sustained?1200u:hd_extended?600u:hd?240u:marathon?1200u:extended?600u:long_fixture?240u:live?60u:12u;
    std::vector<std::int64_t> processing_us;
    if(marathon || hd)processing_us.reserve(target);
    std::uint64_t written_before_stop=0;
    bool paused=false;
    ULONGLONG bright_at=0;
    unsigned bright_diagnostics=0;
    const auto deadline=GetTickCount64()+(hd_extended?80000u:hd?40000u:marathon?80000u:extended?40000u:long_fixture?20000u:live?10000u:8000u);
    while(GetTickCount64()<deadline && (live?lookahead.count()<target:timed?sequence.size()<target:output?light.empty():frames<3)){
        pump();
        if(auto frame=pool.TryGetNextFrame()){
            const auto time=frame.SystemRelativeTime().count();
            const auto content=frame.ContentSize();
            if(!QueryPerformanceCounter(&now)||now.QuadPart<0)
                throw winrt::hresult_error(E_FAIL,L"QPC unavailable");
            const auto current=supreme::recorder::qpc_to_100ns(0,static_cast<std::uint64_t>(now.QuadPart),
                                                              static_cast<std::uint64_t>(frequency.QuadPart));
            if(content.Width!=size.Width || content.Height!=size.Height){
                if(live){worker->cancel();worker->join();std::cerr<<"LiveResizeRejected "<<content.Width<<" "<<content.Height<<"\n";return 1;}
                throw winrt::hresult_error(E_FAIL,L"frame dimensions mismatch");
            }
            const bool hd_injected=stop==FixtureStop::simulated_device_loss && lookahead.count()>=120;
            const bool injected=hd_injected || (simulated_loss && lookahead.count()>=5);
            const auto device_status=injected ? DXGI_ERROR_DEVICE_REMOVED : device->GetDeviceRemovedReason();
            if(FAILED(device_status) && live){
                if(hd_injected){frame.Close();return stop_hd("simulated-device-loss",4);}
                worker->cancel();worker->join();
                std::cerr<<(injected?"LiveSimulatedDeviceLoss ":"LiveDeviceFailure ")
                         <<static_cast<int>(worker->state())<<"\n";
                if(worker->state()!=supreme::recorder::State::cancelled)throw std::runtime_error("device failure cancellation");
                return 1;
            }
            winrt::check_hresult(device_status);
            if(!current || time<=last || time<0 ||
               static_cast<std::uint64_t>(time)>*current+10000000 ||
               (static_cast<std::uint64_t>(time)<*begin && *begin-static_cast<std::uint64_t>(time)>10000000))
                throw winrt::hresult_error(E_FAIL,L"frame dimensions or timestamp mismatch");
            last=time;++frames;
            if(output){
                const auto processing_begin=FixtureClock::now();
                auto pixels=read_crop(device.get(),frame,spec);
                if(live){
                    const auto count=lookahead.count();
                    if(matches(pixels,count>=target/2,spec)){
                        if(auto packet=lookahead.push({static_cast<std::uint64_t>(time),std::move(pixels)}))offer(std::move(*packet));
                        if(marathon || hd)processing_us.push_back(elapsed_us(processing_begin,FixtureClock::now()));
                        std::cout<<"LiveFrame "<<count<<" "<<time<<"\n";
                        if(lookahead.count()==target/2){window.bright();bright_at=GetTickCount64();}
                        if(resize && lookahead.count()==5 && !SetWindowPos(window.handle,nullptr,0,0,400,300,SWP_NOMOVE|SWP_NOZORDER))throw std::runtime_error("test resize failed");
                        written_before_stop=worker->stats().written;
                        if(hd_extended && (lookahead.count()==1 || lookahead.count()%120u==0)){
                            PROCESS_MEMORY_COUNTERS_EX memory{};
                            memory.cb=static_cast<DWORD>(sizeof(memory));
                            if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),memory.cb))
                                throw std::runtime_error("HD capture resource sample unavailable");
                            std::cout<<"LiveResourceSample "<<lookahead.count()<<" "
                                     <<elapsed_us(fixture_wall_begin,FixtureClock::now())<<" "
                                     <<memory.PeakWorkingSetSize<<" "<<memory.WorkingSetSize<<" "
                                     <<memory.PrivateUsage<<" "<<worker->high_water()<<"\n";
                        }
                    }else{
                        ++skipped_pattern;
                        if(hd && bright_at && count>=target/2 && bright_diagnostics<3 &&
                           GetTickCount64()-bright_at>=1000ull*(bright_diagnostics+1u)){
                            diagnose_bright(pixels,spec);++bright_diagnostics;
                        }
                    }
                }else if(timed){
                    const bool expected=sequence.size()>=target/2;
                    if(matches(pixels,expected)){
                        sequence.push_back({static_cast<std::uint64_t>(time),std::move(pixels)});
                        if(sequence.size()==target/2)window.bright();
                    }else ++skipped_pattern;
                }else if(dark.empty() && matches(pixels,false)){dark=std::move(pixels);window.bright();}
                else if(!dark.empty() && matches(pixels,true))light=std::move(pixels);
            }
            frame.Close();
            if(stop==FixtureStop::sink_write_failure && lookahead.count()==120)
                return stop_hd("sink-write-failure",5);
            if(stop==FixtureStop::user_cancel && lookahead.count()==120)
                return stop_hd("user-cancel",3);
            // Controlled fixture-only pause; not a production capture technique.
            if((long_fixture || extended || marathon || hd) && !paused && lookahead.count()>=100){
                paused=true;
                Sleep(350);
            }
        }
        SetWindowTextW(window.handle,(GetTickCount64()/100)%2?L"SSC synthetic capture A":L"SSC synthetic capture B");
        InvalidateRect(window.handle,nullptr,TRUE);
        Sleep(25);
    }
    if(hd)std::cout<<"HDStage close-capture"<<std::endl;
    close_capture.close();
    if(retention)stage_resources(5);
    if(live){
        if(resize || lookahead.count()!=target || !written_before_stop){
            std::cerr<<"LiveIncomplete selected="<<lookahead.count()<<" observed="<<frames<<'\n';return frames?1:77;
        }
        const auto drain_begin=FixtureClock::now();
        offer(lookahead.finish());worker->stop();worker->join();
        if(retention)stage_resources(6);
        const auto fixture_wall_end=FixtureClock::now();
        const auto q=worker->quality()[0];const auto& c=q.written;
        const bool finalize_failure=stop==FixtureStop::sink_finalize_failure;
        const auto required_state=finalize_failure?supreme::recorder::State::failed:supreme::recorder::State::finalized;
        if(worker->state()!=required_state || sink->injected_finalize_failure()!=finalize_failure ||
           c.packets!=target || q.accepted!=target ||
           c.first!=expected_coverage.first || c.end!=expected_coverage.end || c.duration!=expected_coverage.duration ||
           q.dropped_full || q.discarded || q.rejected_invalid)throw std::runtime_error("live worker quality");
        if(finalize_failure){
            const auto stats=worker->stats();
            const auto high=worker->high_water();
            if(target!=120 || stats.accepted!=target || stats.written!=target || stats.discarded ||
               stats.dropped_full || stats.rejected_invalid || !high || high>8 || sink->injected_failure())
                throw std::runtime_error("HD finalization failure accounting");
            const auto path=std::filesystem::path(output);
            const bool exists=std::filesystem::exists(std::filesystem::symlink_status(path));
            const auto bytes=exists?std::filesystem::file_size(path):0;
            if(bytes>16u*1024u*1024u)throw std::runtime_error("HD failed finalization output bound");
            std::cout<<"HDFaultOutcome sink-finalize-failure failed "<<target<<" "<<stats.accepted<<" "<<stats.written
                     <<" "<<stats.discarded<<" 0 "<<high<<" "<<stats.dropped_full<<" "<<stats.rejected_invalid<<std::endl;
            std::cout<<"HDFaultArtifact "<<(exists?1:0)<<" "<<bytes<<std::endl;
            return 6;
        }
        std::cout<<"LiveCapture "<<target<<" "<<frames<<" "<<skipped_pattern<<" "<<written_before_stop<<"\n";
        if(marathon || hd){
            const auto cpu_end=process_cpu_100ns();
            if(cpu_end<cpu_begin)throw std::runtime_error("fixture CPU clock regression");
            const auto summary=ssc::media_fixture::summarize_latency(std::move(processing_us));
            if(summary.count!=target)throw std::runtime_error("fixture processing sample count");
            std::cout<<"LivePerformance "<<summary.count<<" "<<elapsed_us(fixture_wall_begin,fixture_wall_end)
                     <<" "<<cpu_end-cpu_begin<<" "<<summary.median_us<<" "<<summary.p95_us<<" "
                     <<summary.p99_us<<" "<<summary.max_us<<" "<<elapsed_us(drain_begin,fixture_wall_end)<<"\n";
        }
        if(hd){
            PROCESS_MEMORY_COUNTERS_EX memory{};
            memory.cb=static_cast<DWORD>(sizeof(memory));
            if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),memory.cb))
                throw std::runtime_error("HD capture process memory unavailable");
            const auto file_bytes=std::filesystem::file_size(std::filesystem::path(output));
            if(!file_bytes || file_bytes>16u*1024u*1024u)throw std::runtime_error("HD capture output bounds");
            std::cout<<"LiveResources "<<size.Width<<" "<<size.Height<<" "<<spec.frame_width<<" "<<spec.frame_height<<" "
                     <<memory.PeakWorkingSetSize<<" "<<memory.WorkingSetSize<<" "<<memory.PrivateUsage<<" "
                     <<file_bytes<<" "<<8u*spec.frame_bytes<<"\n";
        }
        std::cout<<"LiveHandoff "<<c.packets<<" "<<c.first<<" "<<c.end<<" "<<c.duration<<" "<<*c.internal_gap()<<" "<<worker->high_water()<<" "<<q.dropped_full<<" "<<q.discarded<<"\n";
        return 0;
    }
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
void lifecycle_sample(unsigned phase,bool gui=false){
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb=static_cast<DWORD>(sizeof(memory));
    DWORD handles=0;
    if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),memory.cb)
       || !GetProcessHandleCount(GetCurrentProcess(),&handles))
        throw std::runtime_error("lifecycle resource sample unavailable");
    if(gui){
        SetLastError(ERROR_SUCCESS);
        const DWORD gdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        if(!gdi && GetLastError()!=ERROR_SUCCESS)throw std::runtime_error("GDI sample unavailable");
        SetLastError(ERROR_SUCCESS);
        const DWORD user=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
        if(!user && GetLastError()!=ERROR_SUCCESS)throw std::runtime_error("USER sample unavailable");
        std::cout<<"HDFaultGuiSample "<<phase<<" "<<GetCurrentProcessId()<<" "<<gdi<<" "<<user<<std::endl;
    }
    std::cout<<"HDLifecycleSample "<<phase<<" "<<GetCurrentProcessId()<<" "
             <<memory.PeakWorkingSetSize<<" "<<memory.WorkingSetSize<<" "<<memory.PrivateUsage<<" "<<handles<<std::endl;
}
int lifecycle(const std::filesystem::path& directory,bool faults=false,bool retention=false,bool finalize_failure=false){
    if(!std::filesystem::is_directory(directory) || std::filesystem::is_symlink(directory))return 2;
    struct Segment {bool sustained;FixtureStop stop;};
    const std::vector<Segment> segments=retention?
        std::vector<Segment>{{false,FixtureStop::none},{false,finalize_failure?FixtureStop::sink_finalize_failure:FixtureStop::sink_write_failure},{false,FixtureStop::none}}:
        faults?
        std::vector<Segment>{{false,FixtureStop::none},{false,FixtureStop::user_cancel},
                             {true,FixtureStop::none},{false,FixtureStop::simulated_device_loss},{false,FixtureStop::none}}:
        std::vector<Segment>{{false,FixtureStop::none},{true,FixtureStop::none},{false,FixtureStop::none}};
    std::vector<std::filesystem::path> files;
    for(std::size_t i=0;i<segments.size();++i){
        files.push_back(directory/(L"segment-"+std::to_wstring(i+1)+L".mp4"));
        if(std::filesystem::exists(std::filesystem::symlink_status(files.back())))return 2;
    }
    lifecycle_sample(0,faults||retention);
    for(unsigned i=0;i<files.size();++i){
        const auto segment=segments[i];
        const auto count=segment.stop!=FixtureStop::none?120:segment.sustained?1200:600;
        std::cout<<"HDLifecycleBegin "<<i+1<<" "<<count<<std::endl;
        const int status=run(files[i].c_str(),false,true,false,false,false,false,false,true,true,segment.sustained,segment.stop,retention);
        const int expected=segment.stop==FixtureStop::user_cancel?3:segment.stop==FixtureStop::simulated_device_loss?4:segment.stop==FixtureStop::sink_write_failure?5:segment.stop==FixtureStop::sink_finalize_failure?6:0;
        if(status!=expected){std::cerr<<"HDLifecycleIncomplete "<<i+1<<" "<<status<<'\n';return 1;}
        if(retention){
            stage_resources(7);
            Sleep(1000);pump();stage_resources(8);
            Sleep(2000);pump();stage_resources(9);
        }
        lifecycle_sample(i+1,faults||retention);
        std::cout<<"HDLifecycleEnd "<<i+1<<std::endl;
    }
    return 0;
}
}
int wmain(int argc,wchar_t** argv){
    const bool faults=argc==3 && std::wstring_view(argv[1])==L"--live-hd720-fault-lifecycle";
    const bool refinement=argc==4 && std::wstring_view(argv[1])==L"--isolate-wgc-refinement";
    if(refinement || (argc==4 && std::wstring_view(argv[1])==L"--isolate-retention")){
        try{Apartment apartment;return isolate_retention(argv[2],std::filesystem::path(argv[3]),refinement);}
        catch(const winrt::hresult_error& e){std::cerr<<"Isolation HRESULT "<<winrt::to_string(e.message())<<'\n';return 1;}
        catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    const bool finalize_failure=argc==3 && std::wstring_view(argv[1])==L"--live-hd720-finalize-failure-lifecycle";
    const bool retention=finalize_failure || (argc==3 && std::wstring_view(argv[1])==L"--live-hd720-write-failure-lifecycle");
    if(retention || faults || (argc==3 && std::wstring_view(argv[1])==L"--live-hd720-lifecycle")){
        try{Apartment apartment;return lifecycle(std::filesystem::path(argv[2]),faults,retention,finalize_failure);}
        catch(const winrt::hresult_error& e){std::cerr<<"HD lifecycle HRESULT "<<winrt::to_string(e.message())<<'\n';return 1;}
        catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    const bool timed=argc==3 && std::wstring_view(argv[1])==L"--timed";
    const bool resize=argc==3 && std::wstring_view(argv[1])==L"--live-resize";
    const bool long_fixture=argc==3 && std::wstring_view(argv[1])==L"--live-long";
    const bool extended=argc==3 && std::wstring_view(argv[1])==L"--live-extended";
    const bool simulated_loss=argc==3 && std::wstring_view(argv[1])==L"--live-simulated-loss";
    const bool marathon=argc==3 && std::wstring_view(argv[1])==L"--live-marathon";
    const bool hd_extended=argc==3 && std::wstring_view(argv[1])==L"--live-hd720-extended";
    const bool hd=hd_extended || (argc==3 && std::wstring_view(argv[1])==L"--live-hd720");
    const bool live=hd || resize || long_fixture || extended || simulated_loss || marathon || (argc==3 && std::wstring_view(argv[1])==L"--live");
    if(argc>2 && !timed && !live)return 2;
    try{Apartment apartment;return run((timed||live)?argv[2]:argc==2?argv[1]:nullptr,timed,live,resize,long_fixture,extended,simulated_loss,marathon,hd,hd_extended);}
    catch(const winrt::hresult_error& e){
        const auto hr=e.code().value;
        std::cerr<<"WGC HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<" "<<winrt::to_string(e.message())<<'\n';
        if(hr==E_ACCESSDENIED || hr==E_NOTIMPL || hr==REGDB_E_CLASSNOTREG){std::cout<<"BLOCKED: capture platform/policy unavailable\n";return 77;}
        return 1;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
