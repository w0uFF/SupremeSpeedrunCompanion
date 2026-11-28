#pragma once
#include "src/modules/recorder/public/persistent_output.hpp"
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace supreme::recorder::staging {
// Existing private root, exclusive cooperating writers, no external mutation.
// Path checks are defensive, not a hostile same-user race/ACL sandbox.
class FilesystemStore final:public Store {
    const fs::path root_;
    static bool present(const fs::path& p){
        std::error_code ec;const auto status=fs::symlink_status(p,ec);
        if(ec && ec!=std::errc::no_such_file_or_directory)throw fs::filesystem_error("media status",p,ec);
        return status.type()!=fs::file_type::not_found;
    }
    static void no_reparse(const fs::path& p){
#ifdef _WIN32
        const auto attr=GetFileAttributesW(p.c_str());
        if(attr==INVALID_FILE_ATTRIBUTES || (attr&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("media reparse path");
#else
        if(fs::is_symlink(fs::symlink_status(p)))throw std::runtime_error("media symlink path");
#endif
    }
    static void directory(const fs::path& p,bool private_dir){
        no_reparse(p);
        if(!fs::is_directory(fs::symlink_status(p)))throw std::runtime_error("media directory");
#ifndef _WIN32
        if(private_dir){struct stat st{};if(::lstat(p.c_str(),&st)!=0 || st.st_uid!=::geteuid() || (st.st_mode&0077))throw std::runtime_error("media directory permissions");}
#else
        (void)private_dir; // Caller must establish private ACL; not inferred from path.
#endif
    }
    void root() const {
        if(!root_.is_absolute() || root_==root_.root_path() || root_.filename().empty() || root_.native().find(fs::path::value_type{})!=fs::path::string_type::npos)
            throw std::invalid_argument("explicit media root");
        auto p=root_.root_path();
#ifdef _WIN32
        if(root_.root_name().native().size()!=2 || GetDriveTypeW(p.c_str())!=DRIVE_FIXED)throw std::invalid_argument("fixed drive required");
#endif
        directory(p,false);
        for(const auto& c:root_.relative_path()){
            if(c=="." || c==".." || c.empty())throw std::invalid_argument("media root component");
            p/=c;directory(p,p==root_);
        }
    }
    fs::path slot(const std::string& token,bool require=true) const {
        if(!valid_token(token))throw std::invalid_argument("media token");
        root();const auto p=root_/token;
        if(require)directory(p,true);
        return p;
    }
    static fs::path marker_path(const fs::path& p,Marker m){
        switch(m){case Marker::intent:return p/"intent";case Marker::incomplete:return p/"incomplete";case Marker::finalized:return p/"finalized";}
        throw std::invalid_argument("media marker");
    }
    static bool regular(const fs::path& p){
        if(!present(p))return false;
        no_reparse(p);
        if(!fs::is_regular_file(fs::symlink_status(p)) || fs::hard_link_count(p)!=1)throw std::runtime_error("media file type");
        return true;
    }
    static void sync_directory(const fs::path& p){
#ifndef _WIN32
        const int fd=::open(p.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
        if(fd<0)throw std::runtime_error("media directory open");
        const bool synced=::fsync(fd)==0;const bool closed=::close(fd)==0;
        if(!synced || !closed)throw std::runtime_error("media directory flush");
#else
        // Win32 file flush/write-through below; directory-entry power-loss
        // persistence is not established by this adapter.
        (void)p;
#endif
    }
    static void write_new(const fs::path& p,const std::string& data){
        if(data.empty() || data.size()>metadata_limit)throw std::invalid_argument("media metadata bound");
#ifdef _WIN32
        const HANDLE h=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
        if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("exclusive media marker creation");
        DWORD written=0;const auto length=static_cast<DWORD>(data.size());
        const bool ok=WriteFile(h,data.data(),length,&written,nullptr)!=FALSE && written==length && FlushFileBuffers(h)!=FALSE;
        const bool closed=CloseHandle(h)!=FALSE;
#else
        const int h=::open(p.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
        if(h<0)throw std::runtime_error("exclusive media marker creation");
        std::size_t offset=0;
        while(offset<data.size()){
            const auto n=::write(h,data.data()+offset,data.size()-offset);
            if(n<0 && errno==EINTR)continue;
            if(n<=0)break;
            offset+=static_cast<std::size_t>(n);
        }
        const bool ok=offset==data.size() && ::fsync(h)==0;
        const bool closed=::close(h)==0;
#endif
        if(!ok || !closed)throw std::runtime_error("media marker write/flush");
        sync_directory(p.parent_path());
    }
public:
    explicit FilesystemStore(fs::path root):root_(std::move(root)){this->root();}
    void reserve_slot(const OutputIdentity& id) override {
        if(!id.valid())throw std::invalid_argument("media identity");
        const auto p=slot(id.token,false);
        // Advisory preflight, not a reservation of physical disk blocks.
        if(fs::space(root_).available<id.byte_budget)throw std::runtime_error("media space unavailable");
#ifdef _WIN32
        if(!CreateDirectoryW(p.c_str(),nullptr))throw std::runtime_error("exclusive media directory creation");
#else
        if(::mkdir(p.c_str(),0700)!=0)throw std::runtime_error("exclusive media directory creation");
#endif
        directory(p,true);sync_directory(root_);
    }
    bool exists(const std::string& token) const override {
        const auto p=slot(token,false);if(!present(p))return false;directory(p,true);return true;
    }
    std::optional<std::string> read(const std::string& token,Marker m) const override {
        const auto p=marker_path(slot(token),m);if(!regular(p))return {};
        std::ifstream in(p,std::ios::binary);if(!in)throw std::runtime_error("media marker read");
        char bytes[metadata_limit+1]{};in.read(bytes,static_cast<std::streamsize>(sizeof(bytes)));
        const auto n=in.gcount();if(in.bad() || !in.eof() || n<=0 || n>static_cast<std::streamsize>(metadata_limit))throw std::runtime_error("media marker bound");
        return std::string(bytes,static_cast<std::size_t>(n));
    }
    void append(const std::string& token,Marker m,const std::string& data) override {write_new(marker_path(slot(token),m),data);}
    fs::path destination(const std::string& token) const override {
        const auto p=slot(token)/"video.mp4";if(present(p))throw std::runtime_error("media destination exists");return p;
    }
    std::optional<std::uint64_t> media_size(const std::string& token) const override {
        const auto p=slot(token)/"video.mp4";if(!regular(p))return {};return fs::file_size(p);
    }
    void flush_media(const std::string& token) override {
        const auto p=slot(token)/"video.mp4";if(!regular(p))throw std::runtime_error("media missing");
#ifdef _WIN32
        const HANDLE h=CreateFileW(p.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("media flush open");
        BY_HANDLE_FILE_INFORMATION info{};
        const bool ok=GetFileInformationByHandle(h,&info)!=FALSE && !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) && info.nNumberOfLinks==1 && FlushFileBuffers(h)!=FALSE;
        const bool closed=CloseHandle(h)!=FALSE;
#else
        const int h=::open(p.c_str(),O_RDWR|O_NOFOLLOW);
        if(h<0)throw std::runtime_error("media flush open");
        struct stat st{};const bool ok=::fstat(h,&st)==0 && S_ISREG(st.st_mode) && st.st_nlink==1 && ::fsync(h)==0;
        const bool closed=::close(h)==0;
#endif
        if(!ok || !closed)throw std::runtime_error("media flush");
        sync_directory(p.parent_path());
    }
};
}
