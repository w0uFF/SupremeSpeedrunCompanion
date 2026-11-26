#pragma once
// Independent, bounded resource observations for test-only scopes.
#include <windows.h>
#include <psapi.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include "tools/media/mf_fixture_sink.hpp"

namespace ssc::isolation_fixture {
struct Outcome {
    const char* state="none";
    std::uint64_t accepted=0,written=0,discarded=0;
    bool exists=false;
    std::uintmax_t bytes=0;
};
inline void sample(unsigned cycle,unsigned stage){
    PROCESS_MEMORY_COUNTERS_EX m{};
    m.cb=static_cast<DWORD>(sizeof(m));
    DWORD handles=0;
    if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m),m.cb) ||
       !GetProcessHandleCount(GetCurrentProcess(),&handles))throw std::runtime_error("isolation resource sample");
    SetLastError(ERROR_SUCCESS);
    const auto gdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    if(!gdi && GetLastError()!=ERROR_SUCCESS)throw std::runtime_error("isolation GDI sample");
    SetLastError(ERROR_SUCCESS);
    const auto user=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
    if(!user && GetLastError()!=ERROR_SUCCESS)throw std::runtime_error("isolation USER sample");
    std::cout<<"IsolationSample "<<cycle<<" "<<stage<<" "<<GetCurrentProcessId()<<" "<<m.PeakWorkingSetSize
             <<" "<<m.WorkingSetSize<<" "<<m.PrivateUsage<<" "<<handles<<" "<<gdi<<" "<<user<<std::endl;
}
inline Outcome worker_scope(const std::filesystem::path& path,bool write){
    using namespace supreme::recorder;
    using namespace ssc::media_fixture;
    if(std::filesystem::exists(std::filesystem::symlink_status(path)))throw std::runtime_error("isolation output exists");
    AsyncSink sink(path.c_str(),false);
    Worker worker(sink,8,bytes);
    if(worker.wait_ready()!=State::running)throw std::runtime_error("isolation worker startup");
    const unsigned count=write?60u:0u;
    for(unsigned i=0;i<count;++i){
        std::vector<std::uint8_t> pixels(bytes,128);
        std::fill_n(pixels.begin(),width*height,static_cast<std::uint8_t>(i<30?16:235));
        const auto time=static_cast<std::uint64_t>(period);
        if(worker.offer({Stream::video,i*time,time,std::move(pixels)})!=Offer::accepted)
            throw std::runtime_error("isolation worker offer");
        const auto deadline=GetTickCount64()+5000;
        while(worker.stats().written<i+1 && worker.state()==State::running && GetTickCount64()<deadline)Sleep(5);
        if(worker.stats().written!=i+1)throw std::runtime_error("isolation worker write deadline");
    }
    if(write)worker.stop();else worker.cancel();
    worker.join();
    const auto stats=worker.stats();
    if(worker.state()!=(write?State::finalized:State::cancelled) || stats.accepted!=count || stats.written!=count ||
       stats.discarded || stats.dropped_full || stats.rejected_invalid || worker.high_water()!=(write?1u:0u) ||
       sink.injected_failure() || sink.injected_finalize_failure())throw std::runtime_error("isolation worker outcome");
    const bool exists=std::filesystem::exists(std::filesystem::symlink_status(path));
    const auto size=exists?std::filesystem::file_size(path):0;
    if(size>16u*1024u*1024u || (write && !size))throw std::runtime_error("isolation artifact bound");
    return {write?"finalized":"cancelled",stats.accepted,stats.written,stats.discarded,exists,size};
}
inline void print_outcome(unsigned cycle,const Outcome& r){
    std::cout<<"IsolationOutcome "<<cycle<<" "<<r.state<<" "<<r.accepted<<" "<<r.written<<" "<<r.discarded
             <<" "<<(r.exists?1:0)<<" "<<r.bytes<<std::endl;
}
}
