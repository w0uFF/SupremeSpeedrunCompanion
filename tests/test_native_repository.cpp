#include "src/modules/run_library/infrastructure/windows_repository.hpp"
#include <iostream>
using namespace supreme::run_library;
using windows::NativeRepository;
#define CHECK(x) do{if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
Record record(char id,Origin origin=Origin::local){
    return {std::string(32,id),"alpine-easy",std::string(64,'a'),"fixture-v1",std::string(64,'b'),origin,Status::finished,0,100};
}
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 2;
    try{
        const auto action=std::wstring_view(argv[1]);
        const bool create=action==L"create";
        NativeRepository db(argv[2],create);
        const auto first=record('1');
        if(create){
            CHECK(db.insert(first)==Insert::inserted);
            CHECK(db.insert(first)==Insert::identical);
            auto conflict=first;conflict.manifest_sha256=std::string(64,'c');CHECK(db.insert(conflict)==Insert::conflict);
            auto missing=record('2');missing.igt_ms.reset();missing.rta_ns=0;CHECK(db.insert(missing)==Insert::inserted);
            CHECK(db.insert(record('3',Origin::imported))==Insert::inserted);
            CHECK(db.insert(record('4',Origin::synthetic))==Insert::inserted);
            auto tie=record('0');tie.rta_ns=200;CHECK(db.insert(tie)==Insert::inserted);
            auto other=record('5');other.profile=std::string(64,'c');CHECK(db.insert(other)==Insert::inserted);
            auto incomplete=record('6');incomplete.status=Status::restarted;incomplete.igt_ms.reset();CHECK(db.insert(incomplete)==Insert::inserted);
            db.annotate(first.run_id,{"Test ' \xc3\xa9",true});
            try{db.annotate(std::string(32,'9'),{"missing",false});return 1;}catch(const std::runtime_error&){}
            try{auto bad=first;bad.status=Status::restarted;(void)db.insert(bad);return 1;}catch(const std::invalid_argument&){}
            try{db.annotate(first.run_id,{"\xc0\x80",false});return 1;}catch(const std::invalid_argument&){}
        }else if(action!=L"read")return 2;
        CHECK(db.recent(100).size()==7 && db.recent(1)[0].run_id==std::string(32,'6'));
        CHECK(identical(*db.find(first.run_id),first));
        CHECK(!db.find(std::string(32,'9')));
        CHECK(db.annotation(first.run_id)->favorite && db.annotation(first.run_id)->name=="Test ' \xc3\xa9");
        CHECK(!db.annotation(std::string(32,'9')));
        Comparison q{first.category,first.profile,first.event_semantics,Method::igt};
        CHECK(db.best_local(q)->run_id==std::string(32,'0'));
        q.method=Method::rta;CHECK(db.best_local(q)->run_id==std::string(32,'2'));
        q.event_semantics="other";CHECK(!db.best_local(q));
        bool rejected=false;std::thread wrong([&]{try{(void)db.recent(1);}catch(const std::logic_error&){rejected=true;}});wrong.join();CHECK(rejected);
        std::cout<<"Native repository insert-only, null/zero, conflict, labels, PB, reopen and owner-thread checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 3;}
}
