#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace supreme::storage::winsqlite {
struct sqlite3;
struct sqlite3_stmt;
// SQLite's documented system-library API uses stdcall on Win32.
// Only fixed-arity entry points; no callbacks or variadic calls are used.
using ExecCallback=int (__cdecl*)(void*,int,char**,char**);
using Destructor=void (__cdecl*)(void*);
inline constexpr int ok=0,constraint=19,row=100,done=101,integer=1,text_value=3,null_value=5;
class SystemLibrary {
    HMODULE handle_=LoadLibraryExW(L"winsqlite3.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
public:
    SystemLibrary(){if(!handle_)throw std::runtime_error("system WinSQLite unavailable");}
    ~SystemLibrary(){FreeLibrary(handle_);}
    SystemLibrary(const SystemLibrary&)=delete;
    SystemLibrary& operator=(const SystemLibrary&)=delete;
    template<class T>T resolve(const char* name)const{
        auto address=GetProcAddress(handle_,name);
        if(!address)throw std::runtime_error(name);
        T result{};static_assert(sizeof(result)==sizeof(address),"function pointer size");
        std::memcpy(&result,&address,sizeof(result));return result;
    }
};
struct Api {
    SystemLibrary library;
    int (WINAPI* open)(const void*,sqlite3**)=library.resolve<decltype(open)>("sqlite3_open16");
    int (WINAPI* open_v2)(const char*,sqlite3**,int,const char*)=library.resolve<decltype(open_v2)>("sqlite3_open_v2");
    int (WINAPI* close)(sqlite3*)=library.resolve<decltype(close)>("sqlite3_close");
    int (WINAPI* exec)(sqlite3*,const char*,ExecCallback,void*,char**)=library.resolve<decltype(exec)>("sqlite3_exec");
    int (WINAPI* prepare)(sqlite3*,const char*,int,sqlite3_stmt**,const char**)=library.resolve<decltype(prepare)>("sqlite3_prepare_v2");
    int (WINAPI* finalize)(sqlite3_stmt*)=library.resolve<decltype(finalize)>("sqlite3_finalize");
    int (WINAPI* step)(sqlite3_stmt*)=library.resolve<decltype(step)>("sqlite3_step");
    int (WINAPI* reset)(sqlite3_stmt*)=library.resolve<decltype(reset)>("sqlite3_reset");
    int (WINAPI* bind_text)(sqlite3_stmt*,int,const char*,int,Destructor)=library.resolve<decltype(bind_text)>("sqlite3_bind_text");
    int (WINAPI* bind_null)(sqlite3_stmt*,int)=library.resolve<decltype(bind_null)>("sqlite3_bind_null");
    int (WINAPI* bind_int64)(sqlite3_stmt*,int,std::int64_t)=library.resolve<decltype(bind_int64)>("sqlite3_bind_int64");
    int (WINAPI* column_int)(sqlite3_stmt*,int)=library.resolve<decltype(column_int)>("sqlite3_column_int");
    std::int64_t (WINAPI* column_int64)(sqlite3_stmt*,int)=library.resolve<decltype(column_int64)>("sqlite3_column_int64");
    int (WINAPI* column_type)(sqlite3_stmt*,int)=library.resolve<decltype(column_type)>("sqlite3_column_type");
    const unsigned char* (WINAPI* column_text)(sqlite3_stmt*,int)=library.resolve<decltype(column_text)>("sqlite3_column_text");
    int (WINAPI* column_bytes)(sqlite3_stmt*,int)=library.resolve<decltype(column_bytes)>("sqlite3_column_bytes");
    const char* (WINAPI* version)()=library.resolve<decltype(version)>("sqlite3_libversion");
};
}
