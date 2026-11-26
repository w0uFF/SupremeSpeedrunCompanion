// Standalone system WinSQLite feasibility. No copied implementation, SDK header or import library.
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace {
struct sqlite3;
struct sqlite3_stmt;
// SQLite's documented system-library API uses stdcall on Win32.
// Only fixed-arity entry points; no callbacks or variadic calls are used.
using ExecCallback=int (__cdecl*)(void*,int,char**,char**);
using Destructor=void (__cdecl*)(void*);
constexpr int ok=0,constraint=19,row=100,done=101,integer=1,null_value=5;
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
    const char* (WINAPI* version)()=library.resolve<decltype(version)>("sqlite3_libversion");
};
class Db {
    const Api& api_;
    sqlite3* db_=nullptr;
public:
    Db(const Api& api,const wchar_t* path):api_(api){
        if(api_.open(path,&db_)!=ok){
            if(db_)api_.close(db_);
            throw std::runtime_error("database open failed");
        }
    }
    ~Db(){api_.close(db_);}
    Db(const Db&)=delete;
    Db& operator=(const Db&)=delete;
    sqlite3* get() const{return db_;}
    int exec(const char* sql){return api_.exec(db_,sql,nullptr,nullptr,nullptr);}
};
class Statement{
    const Api& api_;
    sqlite3_stmt* statement_=nullptr;
public:
    Statement(const Api& api,Db& db,const char* sql):api_(api){
        if(api_.prepare(db.get(),sql,-1,&statement_,nullptr)!=ok){
            if(statement_)api_.finalize(statement_);
            throw std::runtime_error("prepare failed");
        }
    }
    ~Statement(){api_.finalize(statement_);}
    Statement(const Statement&)=delete;
    Statement& operator=(const Statement&)=delete;
    sqlite3_stmt* get()const{return statement_;}
};
int count(const Api& api,Db& db,const char* query){
    Statement s(api,db,query);
    if(api.step(s.get())!=row)throw std::runtime_error("missing count");
    return api.column_int(s.get(),0);
}
}
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
int wmain(int argc,wchar_t** argv){
    if(argc!=2 || std::filesystem::exists(std::filesystem::symlink_status(argv[1])))return 2;
    const Api api;
    constexpr char id[]="run'with quote";
    {
        Db db(api,argv[1]);
        CHECK(db.exec("PRAGMA foreign_keys=ON;PRAGMA synchronous=FULL;"
                      "CREATE TABLE runs(id TEXT PRIMARY KEY NOT NULL,igt_ms INTEGER,rta_ns INTEGER);"
                      "CREATE TABLE labels(id TEXT PRIMARY KEY REFERENCES runs(id),name TEXT);BEGIN IMMEDIATE;")==ok);
        Statement insert(api,db,"INSERT INTO runs VALUES(?,?,?);");
        CHECK(api.bind_text(insert.get(),1,id,-1,nullptr)==ok);
        CHECK(api.bind_null(insert.get(),2)==ok);
        CHECK(api.bind_int64(insert.get(),3,0)==ok);
        CHECK(api.step(insert.get())==done);
        CHECK(api.reset(insert.get())==ok);
        CHECK(db.exec("INSERT INTO labels SELECT id,'original' FROM runs;COMMIT;BEGIN IMMEDIATE;UPDATE labels SET name='changed';")==ok);
        CHECK(api.step(insert.get())==constraint);
        CHECK(api.reset(insert.get())==constraint);
        CHECK(db.exec("ROLLBACK;")==ok);
        CHECK(count(api,db,"SELECT count(*) FROM labels WHERE name='original'")==1);
        CHECK(db.exec("INSERT INTO labels VALUES('missing','x');")==constraint);
    }
    {
        Db db(api,argv[1]);
        Statement read(api,db,"SELECT igt_ms,rta_ns FROM runs WHERE id=?;");
        CHECK(api.bind_text(read.get(),1,id,-1,nullptr)==ok);
        CHECK(api.step(read.get())==row);
        CHECK(api.column_type(read.get(),0)==null_value);
        CHECK(api.column_type(read.get(),1)==integer && api.column_int64(read.get(),1)==0);
        CHECK(api.step(read.get())==done);
        CHECK(count(api,db,"SELECT count(*) FROM labels WHERE name='original'")==1);
    }
    std::cout<<"WinSQLite "<<api.version()<<": parameter binding, null/zero, rollback, foreign keys, reopen passed\n";
    return 0;
}
