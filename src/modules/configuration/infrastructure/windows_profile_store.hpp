#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include "src/modules/configuration/public/profile_codec.hpp"

namespace supreme::configuration::windows {
namespace fs=std::filesystem;
[[noreturn]] inline void io_error(const char* action){
    throw std::runtime_error(std::string(action)+" (Win32 "+std::to_string(GetLastError())+")");
}
class Handle {
    HANDLE value_=INVALID_HANDLE_VALUE;
public:
    explicit Handle(HANDLE h):value_(h){if(h==INVALID_HANDLE_VALUE)io_error("open file");}
    ~Handle(){if(value_!=INVALID_HANDLE_VALUE)CloseHandle(value_);}
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
    HANDLE get()const noexcept{return value_;}
    void close(){if(value_!=INVALID_HANDLE_VALUE){const auto h=value_;value_=INVALID_HANDLE_VALUE;if(!CloseHandle(h))io_error("close file");}}
};
// Existing, local fixed-drive directory; no mutation. Owner must control directory.
// Reject reparse points along the path. This is not a race-proof sandbox.
inline fs::path existing_directory(std::u16string_view input){
    const auto normalized=canonical_directory(input);
    if(normalized.empty())throw std::invalid_argument("directory required");
    const fs::path path(normalized);
    const auto root=path.root_path();
    if(GetDriveTypeW(root.c_str())!=DRIVE_FIXED)throw std::invalid_argument("local fixed drive required");
    auto current=root;
    for(const auto& component:path.relative_path()){
        current/=component;
        const DWORD attr=GetFileAttributesW(current.c_str());
        if(attr==INVALID_FILE_ATTRIBUTES)io_error("directory not available");
        if(!(attr&FILE_ATTRIBUTE_DIRECTORY) || (attr&FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::invalid_argument("directory component is not a plain directory");
    }
    return path;
}
inline fs::path package_directory(const Settings& settings,std::u16string_view default_path){
    if(settings.min_free_gib<1 || settings.min_free_gib>100)throw std::invalid_argument("free-space preference");
    const auto path=existing_directory(settings.package_directory.empty()?default_path:std::u16string_view(settings.package_directory));
    ULARGE_INTEGER available{};
    if(!GetDiskFreeSpaceExW(path.c_str(),&available,nullptr,nullptr))io_error("free space check");
    if(available.QuadPart<static_cast<unsigned long long>(settings.min_free_gib)*1024ull*1024ull*1024ull)
        throw std::runtime_error("insufficient available space");
    return path;
}
// Native schema-1 profile store. Existing trusted private directory; fixed filename.
// Single owner, no concurrent writers, no automatic migration or overwrite on invalid data.
class FileStore final:public Store {
    fs::path directory_;
    LexicalPathPolicy paths_;
    fs::path path()const{return directory_/L"profiles.json";}
    void verify_directory()const{(void)existing_directory(directory_.u16string());}
    std::string bytes()const{
        verify_directory();
        Handle file(CreateFileW(path().c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                               FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        BY_HANDLE_FILE_INFORMATION info{};
        if(!GetFileInformationByHandle(file.get(),&info))io_error("file information");
        if((info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) ||
           info.nNumberOfLinks!=1 || info.nFileSizeHigh || info.nFileSizeLow>codec::max_bytes)
            throw std::invalid_argument("profile file type or size");
        std::string data(static_cast<std::size_t>(info.nFileSizeLow),'\0');
        DWORD actual=0;
        if(!ReadFile(file.get(),data.data(),info.nFileSizeLow,&actual,nullptr) || actual!=info.nFileSizeLow)
            io_error("read profile");
        char extra=0;
        if(!ReadFile(file.get(),&extra,1,&actual,nullptr) || actual!=0)io_error("profile size changed");
        return data;
    }
    void publish(const Book& book,bool create){
        verify_directory();
        const auto data=codec::encode(book,paths_);
        if(!create)(void)load(); // malformed or missing store is never silently overwritten
        fs::path temporary;
        for(unsigned i=0;i<32;++i){
            const auto candidate=directory_/(L".ssc-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(i)+L".tmp");
            const HANDLE raw=CreateFileW(candidate.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(raw==INVALID_HANDLE_VALUE){
                if(GetLastError()==ERROR_FILE_EXISTS || GetLastError()==ERROR_ALREADY_EXISTS)continue;
                io_error("create profile temporary");
            }
            temporary=candidate;
            try{
                Handle file(raw);
                DWORD actual=0;
                const auto size=static_cast<DWORD>(data.size());
                if(!WriteFile(file.get(),data.data(),size,&actual,nullptr) || actual!=size)io_error("write profile");
                if(!FlushFileBuffers(file.get()))io_error("flush profile");
                file.close();
                const DWORD flags=MOVEFILE_WRITE_THROUGH|(create?0u:MOVEFILE_REPLACE_EXISTING);
                if(!MoveFileExW(temporary.c_str(),path().c_str(),flags))io_error("publish profile");
                return;
            }catch(...){DeleteFileW(temporary.c_str());throw;}
        }
        throw std::runtime_error("temporary profile slots occupied");
    }
public:
    explicit FileStore(std::u16string_view directory):directory_(existing_directory(directory)){}
    Book load()const override{return codec::decode(bytes(),paths_);}
    void save(const Book& book)override{publish(book,false);}
    void create(const Book& book){publish(book,true);}
};
}
