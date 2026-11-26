// Standalone system WinSQLite feasibility. No copied implementation, SDK header or import library.
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "src/infrastructure/storage/winsqlite_api.hpp"
namespace {
using namespace supreme::storage::winsqlite;
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
