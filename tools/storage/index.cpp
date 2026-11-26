// Standalone trusted-controller bridge. No archive parser, no local origin admission.
#include "src/modules/run_library/infrastructure/windows_repository.hpp"
#include <iostream>
#include <array>
using namespace supreme::run_library;
namespace {
std::string field(){
    std::string result;
    char c=0;
    for(;;){
        if(!std::cin.get(c))throw std::invalid_argument("truncated record");
        if(c=='\n')break;
        if(c<33 || c>126 || result.size()==128)throw std::invalid_argument("record field");
        result.push_back(c);
    }
    return result;
}
std::array<std::string,9> fields(){
    std::array<std::string,9> result;
    for(auto& value:result)value=field();
    if(std::cin.peek()!=std::char_traits<char>::eof())throw std::invalid_argument("trailing record data");
    return result;
}
std::optional<std::uint64_t> elapsed(std::string_view s){
    if(s=="-")return {};
    if(s.empty() || (s.size()>1 && s.front()=='0'))throw std::invalid_argument("elapsed");
    std::uint64_t n=0;
    constexpr auto max=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    for(char c:s){
        if(c<'0' || c>'9')throw std::invalid_argument("elapsed");
        const auto d=static_cast<unsigned>(c-'0');
        if(n>(max-d)/10)throw std::invalid_argument("elapsed overflow");
        n=n*10+d;
    }
    return n;
}
Record record(){
    const auto f=fields();
    const auto origin=f[5]=="imported"?Origin::imported:f[5]=="synthetic"?Origin::synthetic:throw std::invalid_argument("nonlocal origin required");
    const auto status=f[6]=="finished"?Status::finished:f[6]=="restarted"?Status::restarted:
        f[6]=="abandoned"?Status::abandoned:f[6]=="failed"?Status::failed:throw std::invalid_argument("status");
    Record r{f[0],f[1],f[2],f[3],f[4],origin,status,elapsed(f[7]),elapsed(f[8])};
    validate(r);return r;
}
}
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 2;
    try{
        const std::wstring_view action(argv[1]);
        if(action==L"init"){windows::NativeRepository db(argv[2],true);return 0;}
        if(action==L"presence"){
            const auto id=field();
            if(std::cin.peek()!=std::char_traits<char>::eof())throw std::invalid_argument("trailing identity data");
            windows::NativeRepository db(argv[2],false,true);
            std::cout<<(db.find(id)?"present":"missing")<<'\n';return 0;
        }
        if(action==L"compare"){
            const auto r=record();
            windows::NativeRepository db(argv[2],false,true);
            const auto existing=db.find(r.run_id);
            std::cout<<(!existing?"missing":identical(*existing,r)?"identical":"conflict")<<'\n';return 0;
        }
        if(action!=L"insert")return 2;
        const auto r=record();
        windows::NativeRepository db(argv[2],false);
        const auto result=db.insert(r);
        std::cout<<(result==Insert::inserted?"inserted":result==Insert::identical?"identical":"conflict")<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 3;}
}
