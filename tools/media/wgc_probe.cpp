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
#include <fstream>
#include <vector>
#include "src/modules/recorder/public/pixel_conversion.hpp"
#include "src/modules/recorder/public/quality.hpp"
#include "src/modules/recorder/public/timed_frames.hpp"
#include "src/modules/recorder/public/frame_lookahead.hpp"
#include "tools/media/mf_fixture_sink.hpp"
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
    explicit Window(bool hd=false){
        WNDCLASSW cls{};cls.lpfnWndProc=paint_window;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"SSC_T20_OwnWindow";
        if(!RegisterClassW(&cls))return;
        const DWORD style=hd?static_cast<DWORD>(WS_POPUP|WS_VISIBLE):static_cast<DWORD>(WS_OVERLAPPEDWINDOW|WS_VISIBLE);
        handle=CreateWindowExW(0,cls.lpszClassName,L"SSC synthetic capture test",style,
                               hd?0:40,hd?0:40,hd?1280:320,hd?720:240,nullptr,nullptr,cls.hInstance,nullptr);
        if(handle)UpdateWindow(handle);
    }
    ~Window(){if(handle)DestroyWindow(handle);}
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
int run(const wchar_t* output,bool timed=false,bool live=false,bool resize=false,bool long_fixture=false,bool extended=false,bool simulated_loss=false,bool marathon=false,bool hd=false) {
    HDESK desktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
    if(!desktop){std::cout<<"BLOCKED: input desktop unavailable, Win32="<<GetLastError()<<'\n';return 77;}
    CloseDesktop(desktop);
    Apartment apartment;
    if(!cap::GraphicsCaptureSession::IsSupported()){
        std::cout<<"BLOCKED: Windows Graphics Capture unsupported\n";return 77;
    }
    const auto spec=hd?ssc::media_fixture::hd_spec:ssc::media_fixture::low_spec;
    Window window(hd);
    if(!window.handle){std::cerr<<"test window creation failed "<<GetLastError()<<'\n';return 1;}
    pump();
    cap::GraphicsCaptureItem item{nullptr};
    auto interop=winrt::get_activation_factory<cap::GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
    winrt::check_hresult(interop->CreateForWindow(window.handle,winrt::guid_of<cap::GraphicsCaptureItem>(),winrt::put_abi(item)));
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
    // Worker starts before acquisition; sink is owned longer than Worker.
    std::unique_ptr<ssc::media_fixture::AsyncSink> sink;
    std::unique_ptr<supreme::recorder::Worker> worker;
    supreme::recorder::FrameLookahead lookahead(spec.frame_bytes);
    supreme::recorder::Coverage expected_coverage;
    if(live){
        if(std::filesystem::exists(std::filesystem::symlink_status(output)))return 2;
        sink=std::make_unique<ssc::media_fixture::AsyncSink>(output,false,spec);
        worker=std::make_unique<supreme::recorder::Worker>(*sink,8,hd?spec.frame_bytes:32768u);
        if(worker->wait_ready()!=supreme::recorder::State::running)throw std::runtime_error("live worker startup");
    }
    auto offer=[&](supreme::recorder::Packet packet){
        if(!expected_coverage.observe(packet.pts,packet.duration))throw std::runtime_error("live handoff interval");
        const auto result=worker->offer(std::move(packet));
        if(result!=supreme::recorder::Offer::accepted){
            std::cerr<<"LiveOfferFailure "<<static_cast<int>(result)<<" dropped="<<worker->stats().dropped_full<<'\n';
            throw std::runtime_error("live worker rejected packet");
        }
    };
    struct CloseCapture {
        cap::GraphicsCaptureSession& session;
        cap::Direct3D11CaptureFramePool& pool;
        ~CloseCapture(){try{session.Close();}catch(...){}try{pool.Close();}catch(...){}}
    } close_capture{session,pool};
    const auto fixture_wall_begin=FixtureClock::now();
    const auto cpu_begin=(marathon || hd)?process_cpu_100ns():std::uint64_t{0};
    session.StartCapture();
    unsigned frames=0;std::int64_t last=-1;
    std::vector<std::uint8_t> dark,light;
    std::vector<supreme::recorder::TimedFrame> sequence;
    unsigned skipped_pattern=0;
    const std::size_t target=hd?240u:marathon?1200u:extended?600u:long_fixture?240u:live?60u:12u;
    std::vector<std::int64_t> processing_us;
    if(marathon || hd)processing_us.reserve(target);
    std::uint64_t written_before_stop=0;
    bool paused=false;
    const auto deadline=GetTickCount64()+(hd?40000u:marathon?80000u:extended?40000u:long_fixture?20000u:live?10000u:8000u);
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
            const bool injected=simulated_loss && lookahead.count()>=5;
            const auto device_status=injected ? DXGI_ERROR_DEVICE_REMOVED : device->GetDeviceRemovedReason();
            if(FAILED(device_status) && live){
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
                        if(lookahead.count()==target/2)window.bright();
                        if(resize && lookahead.count()==5 && !SetWindowPos(window.handle,nullptr,0,0,400,300,SWP_NOMOVE|SWP_NOZORDER))throw std::runtime_error("test resize failed");
                        written_before_stop=worker->stats().written;
                    }else ++skipped_pattern;
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
    session.Close();pool.Close();
    if(live){
        if(resize || lookahead.count()!=target || !written_before_stop){
            std::cerr<<"LiveIncomplete selected="<<lookahead.count()<<" observed="<<frames<<'\n';return frames?1:77;
        }
        const auto drain_begin=FixtureClock::now();
        offer(lookahead.finish());worker->stop();worker->join();
        const auto fixture_wall_end=FixtureClock::now();
        const auto q=worker->quality()[0];const auto& c=q.written;
        if(worker->state()!=supreme::recorder::State::finalized || c.packets!=target || q.accepted!=target ||
           c.first!=expected_coverage.first || c.end!=expected_coverage.end || c.duration!=expected_coverage.duration ||
           q.dropped_full || q.discarded || q.rejected_invalid)throw std::runtime_error("live worker quality");
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
}
int wmain(int argc,wchar_t** argv){
    const bool timed=argc==3 && std::wstring_view(argv[1])==L"--timed";
    const bool resize=argc==3 && std::wstring_view(argv[1])==L"--live-resize";
    const bool long_fixture=argc==3 && std::wstring_view(argv[1])==L"--live-long";
    const bool extended=argc==3 && std::wstring_view(argv[1])==L"--live-extended";
    const bool simulated_loss=argc==3 && std::wstring_view(argv[1])==L"--live-simulated-loss";
    const bool marathon=argc==3 && std::wstring_view(argv[1])==L"--live-marathon";
    const bool hd=argc==3 && std::wstring_view(argv[1])==L"--live-hd720";
    const bool live=hd || resize || long_fixture || extended || simulated_loss || marathon || (argc==3 && std::wstring_view(argv[1])==L"--live");
    if(argc>2 && !timed && !live)return 2;
    try{return run((timed||live)?argv[2]:argc==2?argv[1]:nullptr,timed,live,resize,long_fixture,extended,simulated_loss,marathon,hd);}
    catch(const winrt::hresult_error& e){
        const auto hr=e.code().value;
        std::cerr<<"WGC HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<" "<<winrt::to_string(e.message())<<'\n';
        if(hr==E_ACCESSDENIED || hr==E_NOTIMPL || hr==REGDB_E_CLASSNOTREG){std::cout<<"BLOCKED: capture platform/policy unavailable\n";return 77;}
        return 1;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
