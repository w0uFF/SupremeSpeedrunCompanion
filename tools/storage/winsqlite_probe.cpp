// Standalone Windows SDK storage feasibility, not a production Repository.
#include <winsqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace {
class Db {
    sqlite3* db_=nullptr;
public:
    explicit Db(const wchar_t* path) {
        if(sqlite3_open16(path,&db_)!=SQLITE_OK){
            if(db_)sqlite3_close(db_);
            throw std::runtime_error("database open failed");
        }
    }
    ~Db(){sqlite3_close(db_);}
    Db(const Db&)=delete;
    Db& operator=(const Db&)=delete;
    sqlite3* get() const{return db_;}
    int exec(const char* sql){return sqlite3_exec(db_,sql,nullptr,nullptr,nullptr);}
};
class Statement{
    sqlite3_stmt* statement_=nullptr;
public:
    Statement(Db& db,const char* sql){
        if(sqlite3_prepare_v2(db.get(),sql,-1,&statement_,nullptr)!=SQLITE_OK){
            if(statement_)sqlite3_finalize(statement_);
            throw std::runtime_error("prepare failed");
        }
    }
    ~Statement(){sqlite3_finalize(statement_);}
    Statement(const Statement&)=delete;
    Statement& operator=(const Statement&)=delete;
    sqlite3_stmt* get()const{return statement_;}
};
int count(Db& db,const char* query){
    Statement s(db,query);
    if(sqlite3_step(s.get())!=SQLITE_ROW)throw std::runtime_error("missing count");
    return sqlite3_column_int(s.get(),0);
}
}
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
int wmain(int argc,wchar_t** argv){
    if(argc!=2 || std::filesystem::exists(std::filesystem::symlink_status(argv[1])))return 2;
    constexpr char id[]="run'with quote";
    {
        Db db(argv[1]);
        CHECK(db.exec("PRAGMA foreign_keys=ON;PRAGMA synchronous=FULL;"
                      "CREATE TABLE runs(id TEXT PRIMARY KEY NOT NULL,igt_ms INTEGER,rta_ns INTEGER);"
                      "CREATE TABLE labels(id TEXT PRIMARY KEY REFERENCES runs(id),name TEXT);BEGIN IMMEDIATE;")==SQLITE_OK);
        Statement insert(db,"INSERT INTO runs VALUES(?,?,?);");
        CHECK(sqlite3_bind_text(insert.get(),1,id,-1,SQLITE_STATIC)==SQLITE_OK);
        CHECK(sqlite3_bind_null(insert.get(),2)==SQLITE_OK);
        CHECK(sqlite3_bind_int64(insert.get(),3,0)==SQLITE_OK);
        CHECK(sqlite3_step(insert.get())==SQLITE_DONE);
        CHECK(sqlite3_reset(insert.get())==SQLITE_OK);
        CHECK(db.exec("INSERT INTO labels SELECT id,'original' FROM runs;COMMIT;BEGIN IMMEDIATE;UPDATE labels SET name='changed';")==SQLITE_OK);
        CHECK(sqlite3_step(insert.get())==SQLITE_CONSTRAINT);
        CHECK(sqlite3_reset(insert.get())==SQLITE_CONSTRAINT);
        CHECK(db.exec("ROLLBACK;")==SQLITE_OK);
        CHECK(count(db,"SELECT count(*) FROM labels WHERE name='original'")==1);
        CHECK(db.exec("INSERT INTO labels VALUES('missing','x');")==SQLITE_CONSTRAINT);
    }
    {
        Db db(argv[1]);
        Statement read(db,"SELECT igt_ms,rta_ns FROM runs WHERE id=?;");
        CHECK(sqlite3_bind_text(read.get(),1,id,-1,SQLITE_STATIC)==SQLITE_OK);
        CHECK(sqlite3_step(read.get())==SQLITE_ROW);
        CHECK(sqlite3_column_type(read.get(),0)==SQLITE_NULL);
        CHECK(sqlite3_column_type(read.get(),1)==SQLITE_INTEGER && sqlite3_column_int64(read.get(),1)==0);
        CHECK(sqlite3_step(read.get())==SQLITE_DONE);
        CHECK(count(db,"SELECT count(*) FROM labels WHERE name='original'")==1);
    }
    std::cout<<"WinSQLite "<<sqlite3_libversion()<<": parameter binding, null/zero, rollback, foreign keys, reopen passed\n";
    return 0;
}
