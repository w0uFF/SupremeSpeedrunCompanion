// Standalone storage feasibility probe. Use a fresh path inside a private temporary directory.
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
class Db {
    sqlite3* db_=nullptr;
public:
    explicit Db(const char* path) {
        if(sqlite3_open_v2(path,&db_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,nullptr)!=SQLITE_OK) {
            if(db_) sqlite3_close(db_);
            throw std::runtime_error("open failed");
        }
    }
    ~Db(){sqlite3_close(db_);}
    Db(const Db&)=delete;
    Db& operator=(const Db&)=delete;
    int exec(const char* sql){return sqlite3_exec(db_,sql,nullptr,nullptr,nullptr);}
    int scalar(const char* sql) {
        sqlite3_stmt* statement=nullptr;
        if(sqlite3_prepare_v2(db_,sql,-1,&statement,nullptr)!=SQLITE_OK)
            throw std::runtime_error("prepare failed");
        const auto rc=sqlite3_step(statement);
        const auto value=rc==SQLITE_ROW?sqlite3_column_int(statement,0):-1;
        sqlite3_finalize(statement);
        if(rc!=SQLITE_ROW)throw std::runtime_error("row unavailable");
        return value;
    }
};
}
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
int main(int argc,char** argv) {
    if(argc!=2 || std::filesystem::exists(std::filesystem::symlink_status(argv[1])))return 2;
    {
        Db d(argv[1]);
        CHECK(d.exec("PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL;"
                     "CREATE TABLE runs(id TEXT PRIMARY KEY NOT NULL, time_ms INTEGER);"
                     "CREATE TABLE labels(id TEXT PRIMARY KEY REFERENCES runs(id), name TEXT);"
                     "BEGIN IMMEDIATE; INSERT INTO runs VALUES('one',0);"
                     "INSERT INTO labels VALUES('one','original'); COMMIT;")==SQLITE_OK);
        CHECK(d.exec("BEGIN IMMEDIATE; UPDATE labels SET name='changed'; INSERT INTO runs VALUES('one',9);")==SQLITE_CONSTRAINT);
        CHECK(d.exec("ROLLBACK;")==SQLITE_OK);
        CHECK(d.scalar("SELECT count(*) FROM labels WHERE name='original'")==1);
        CHECK(d.exec("INSERT INTO labels VALUES('missing','x');")==SQLITE_CONSTRAINT);
        CHECK(d.scalar("SELECT time_ms FROM runs WHERE id='one'")==0);
    }
    {
        Db d(argv[1]);
        CHECK(d.scalar("SELECT count(*) FROM runs")==1);
        CHECK(d.scalar("SELECT count(*) FROM labels WHERE name='original'")==1);
        CHECK(d.scalar("SELECT time_ms FROM runs WHERE id='one'")==0);
    }
    std::cout<<"SQLite "<<sqlite3_libversion()<<": transaction, duplicate constraint, rollback, foreign key, zero and reopen passed\n";
    return 0;
}
