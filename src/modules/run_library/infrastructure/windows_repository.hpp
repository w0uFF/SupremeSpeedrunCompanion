#pragma once
#include "src/infrastructure/storage/winsqlite_api.hpp"
#include "src/modules/run_library/public/record_validation.hpp"
#include <filesystem>
#include <thread>

namespace supreme::run_library::windows {
namespace sql=supreme::storage::winsqlite;
namespace fs=std::filesystem;
inline void require(int actual,int expected=sql::ok){
    if(actual!=expected)throw std::runtime_error("WinSQLite result "+std::to_string(actual));
}
class Database {
    const sql::Api& api_;
    sql::sqlite3* db_=nullptr;
public:
    Database(const sql::Api& api,const fs::path& path,bool create):api_(api){
        const auto parent=path.parent_path();
        if(!path.is_absolute() || path.filename()!=L"runs.sqlite" || parent.empty())
            throw std::invalid_argument("explicit private runs.sqlite path required");
        // Existing trusted folder. Reject directory links; no hostile-path/concurrent safety claim.
        auto current=parent.root_path();
        if(GetDriveTypeW(current.c_str())!=DRIVE_FIXED)throw std::invalid_argument("fixed drive required");
        for(const auto& component:parent.relative_path()){
            if(component==L"." || component==L"..")throw std::invalid_argument("relative component");
            current/=component;
            const DWORD attr=GetFileAttributesW(current.c_str());
            if(attr==INVALID_FILE_ATTRIBUTES || !(attr&FILE_ATTRIBUTE_DIRECTORY) || (attr&FILE_ATTRIBUTE_REPARSE_POINT))
                throw std::invalid_argument("private directory unavailable");
        }
        if(create){
            const HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("exclusive database creation failed");
            if(!CloseHandle(h))throw std::runtime_error("close reserved database file");
        }
        const HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("database unavailable");
        BY_HANDLE_FILE_INFORMATION info{};
        const bool safe=GetFileInformationByHandle(h,&info)!=FALSE &&
            !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) && info.nNumberOfLinks==1;
        CloseHandle(h);
        if(!safe)throw std::invalid_argument("database file type");
        // READWRITE without CREATE: existing/explicitly reserved file only. No URI interpretation.
        const auto utf8=path.u8string();
        const int rc=api_.open_v2(utf8.c_str(),&db_,2,nullptr);
        if(rc!=sql::ok){if(db_)api_.close(db_);db_=nullptr;require(rc);}
    }
    ~Database(){if(db_)api_.close(db_);}
    Database(const Database&)=delete;
    Database& operator=(const Database&)=delete;
    sql::sqlite3* get()const noexcept{return db_;}
    void exec(const char* statement)const{require(api_.exec(db_,statement,nullptr,nullptr,nullptr));}
    void rollback()const noexcept{(void)api_.exec(db_,"ROLLBACK",nullptr,nullptr,nullptr);}
};
class Statement {
    const sql::Api& api_;
    sql::sqlite3_stmt* stmt_=nullptr;
public:
    Statement(const sql::Api& api,const Database& db,const char* query):api_(api){
        const int rc=api_.prepare(db.get(),query,-1,&stmt_,nullptr);
        if(rc!=sql::ok || !stmt_){if(stmt_)api_.finalize(stmt_);throw std::runtime_error("prepare failed "+std::to_string(rc));}
    }
    ~Statement(){api_.finalize(stmt_);}
    Statement(const Statement&)=delete;
    Statement& operator=(const Statement&)=delete;
    // Bound text is borrowed until the statement is finalized. All callers keep it alive.
    void bind(int index,std::string_view value){
        require(api_.bind_text(stmt_,index,value.data(),static_cast<int>(value.size()),nullptr));
    }
    void bind(int index,std::int64_t value){require(api_.bind_int64(stmt_,index,value));}
    void bind(int index,std::optional<std::uint64_t> value){
        if(value)bind(index,static_cast<std::int64_t>(*value));else require(api_.bind_null(stmt_,index));
    }
    bool next(){const int rc=api_.step(stmt_);if(rc==sql::done)return false;require(rc,sql::row);return true;}
    void execute(){require(api_.step(stmt_),sql::done);}
    std::int64_t number(int column)const{
        if(api_.column_type(stmt_,column)!=sql::integer)throw std::runtime_error("stored integer type");
        return api_.column_int64(stmt_,column);
    }
    std::optional<std::uint64_t> elapsed(int column)const{
        if(api_.column_type(stmt_,column)==sql::null_value)return {};
        const auto n=number(column);if(n<0)throw std::runtime_error("stored negative time");
        return static_cast<std::uint64_t>(n);
    }
    std::string text(int column,std::size_t limit=256)const{
        if(api_.column_type(stmt_,column)!=sql::text_value)throw std::runtime_error("stored text type");
        const auto* bytes=api_.column_text(stmt_,column);
        const int n=api_.column_bytes(stmt_,column);
        if(!bytes || n<0 || static_cast<std::size_t>(n)>limit)throw std::runtime_error("stored text bound");
        return std::string(reinterpret_cast<const char*>(bytes),static_cast<std::size_t>(n));
    }
};
class Transaction {
    const Database& db_;bool committed_=false;
public:
    explicit Transaction(const Database& db):db_(db){db_.exec("BEGIN IMMEDIATE");}
    ~Transaction(){if(!committed_)db_.rollback();}
    Transaction(const Transaction&)=delete;
    Transaction& operator=(const Transaction&)=delete;
    void commit(){db_.exec("COMMIT");committed_=true;}
};
// Exact schema identity; derived index, not the authoritative evidence archive.
// Explicit version 1, no migration of older Python prototype databases.
inline constexpr char runs_sql[]="CREATE TABLE runs(seq INTEGER PRIMARY KEY,run_id TEXT NOT NULL UNIQUE,category TEXT NOT NULL,profile TEXT NOT NULL,event_semantics TEXT NOT NULL,manifest_sha256 TEXT NOT NULL,origin INTEGER NOT NULL CHECK(origin BETWEEN 0 AND 2),status INTEGER NOT NULL CHECK(status BETWEEN 0 AND 3),igt_ms INTEGER CHECK(igt_ms>=0 AND status=0),rta_ns INTEGER CHECK(rta_ns>=0))";
inline constexpr char labels_sql[]="CREATE TABLE labels(run_id TEXT PRIMARY KEY NOT NULL REFERENCES runs(run_id),name TEXT NOT NULL,favorite INTEGER NOT NULL CHECK(favorite IN (0,1)))";
inline constexpr char index_sql[]="CREATE INDEX runs_comparison ON runs(category,profile,event_semantics,origin,status)";
inline constexpr std::int64_t application_id=0x53534352;
inline constexpr int schema_version=1;
inline constexpr char columns[]="run_id,category,profile,event_semantics,manifest_sha256,origin,status,igt_ms,rta_ns";
class NativeRepository final:public QueryRepository {
    const std::thread::id owner_=std::this_thread::get_id();
    sql::Api api_;
    Database db_;
    void owner()const{if(std::this_thread::get_id()!=owner_)throw std::logic_error("repository owner thread");}
    std::int64_t scalar(const char* query)const{
        Statement s(api_,db_,query);if(!s.next())throw std::runtime_error("missing scalar");
        const auto n=s.number(0);if(s.next())throw std::runtime_error("multiple scalar rows");return n;
    }
    void schema()const{
        if(scalar("PRAGMA application_id")!=application_id || scalar("PRAGMA user_version")!=schema_version)
            throw std::runtime_error("unsupported native library identity/version");
        Statement s(api_,db_,"SELECT name,sql FROM sqlite_master WHERE sql IS NOT NULL ORDER BY name");
        for(const auto& expected: {std::pair<const char*,const char*>{"labels",labels_sql},{"runs",runs_sql},{"runs_comparison",index_sql}}){
            if(!s.next() || s.text(0)!=expected.first || s.text(1,2048)!=expected.second)
                throw std::runtime_error("unsupported library schema");
        }
        if(s.next())throw std::runtime_error("unexpected schema object");
    }
    static Record read(const Statement& s){
        const auto origin=s.number(5),status=s.number(6);
        if(origin<0 || origin>2 || status<0 || status>3)throw std::runtime_error("stored classification");
        Record r{s.text(0),s.text(1),s.text(2),s.text(3),s.text(4),
            static_cast<Origin>(origin),static_cast<Status>(status),s.elapsed(7),s.elapsed(8)};
        validate(r);return r;
    }
public:
    NativeRepository(const fs::path& path,bool create):db_(api_,path,create){
        if(create){
            Transaction tx(db_);
            if(scalar("PRAGMA user_version")!=0 || scalar("PRAGMA application_id")!=0 ||
               scalar("SELECT count(*) FROM sqlite_master")!=0)throw std::runtime_error("reserved database not empty");
            db_.exec(runs_sql);db_.exec(labels_sql);db_.exec(index_sql);
            db_.exec("PRAGMA application_id=0x53534352");db_.exec("PRAGMA user_version=1");tx.commit();
        }
        schema();
        db_.exec("PRAGMA foreign_keys=ON");db_.exec("PRAGMA synchronous=FULL");
        if(scalar("PRAGMA foreign_keys")!=1)throw std::runtime_error("foreign keys unavailable");
    }
    std::optional<Record> find(std::string_view id)const override{
        owner();validate_id(id);
        const auto q=std::string("SELECT ")+columns+" FROM runs WHERE run_id=?";
        Statement s(api_,db_,q.c_str());s.bind(1,id);
        if(!s.next())return {};
        auto r=read(s);if(s.next())throw std::runtime_error("duplicate identity");return r;
    }
    Insert insert(const Record& r)override{
        owner();validate(r);Transaction tx(db_);
        if(const auto old=find(r.run_id)){
            const auto result=identical(*old,r)?Insert::identical:Insert::conflict;
            tx.commit();return result;
        }
        Statement s(api_,db_,"INSERT INTO runs(run_id,category,profile,event_semantics,manifest_sha256,origin,status,igt_ms,rta_ns) VALUES(?,?,?,?,?,?,?,?,?)");
        s.bind(1,r.run_id);s.bind(2,r.category);s.bind(3,r.profile);s.bind(4,r.event_semantics);s.bind(5,r.manifest_sha256);
        s.bind(6,static_cast<std::int64_t>(r.origin));s.bind(7,static_cast<std::int64_t>(r.status));s.bind(8,r.igt_ms);s.bind(9,r.rta_ns);
        s.execute();tx.commit();return Insert::inserted;
    }
    std::vector<Record> recent(std::size_t limit)const override{
        owner();if(!limit || limit>1000)throw std::invalid_argument("history limit");
        const auto q=std::string("SELECT ")+columns+" FROM runs ORDER BY seq DESC LIMIT ?";
        Statement s(api_,db_,q.c_str());s.bind(1,static_cast<std::int64_t>(limit));
        std::vector<Record> result;while(s.next())result.push_back(read(s));return result;
    }
    void annotate(std::string_view id,const Annotation& a)override{
        owner();validate_id(id);validate(a);Transaction tx(db_);
        Statement s(api_,db_,"INSERT OR REPLACE INTO labels(run_id,name,favorite) VALUES(?,?,?)");
        s.bind(1,id);s.bind(2,a.name);s.bind(3,static_cast<std::int64_t>(a.favorite));s.execute();tx.commit();
    }
    std::optional<Annotation> annotation(std::string_view id)const override{
        owner();validate_id(id);Statement s(api_,db_,"SELECT name,favorite FROM labels WHERE run_id=?");s.bind(1,id);
        if(!s.next())return {};
        const auto n=s.number(1);if(n!=0 && n!=1)throw std::runtime_error("stored favorite");
        Annotation a{s.text(0),n==1};validate(a);if(s.next())throw std::runtime_error("duplicate annotation");return a;
    }
    std::optional<Record> best_local(const Comparison& query)const override{
        owner();validate(query);
        const std::string field=query.method==Method::igt?"igt_ms":"rta_ns";
        const auto q=std::string("SELECT ")+columns+" FROM runs WHERE category=? AND profile=? AND event_semantics=? AND origin=0 AND status=0 AND "+field+" IS NOT NULL ORDER BY "+field+",run_id LIMIT 1";
        Statement s(api_,db_,q.c_str());s.bind(1,query.category);s.bind(2,query.profile);s.bind(3,query.event_semantics);
        if(!s.next())return {};
        auto result=read(s);if(s.next())throw std::runtime_error("multiple PB rows");return result;
    }
};
}
