#include "src/modules/configuration/public/profile_codec.hpp"
#include <iostream>
#include <iterator>
using namespace supreme::configuration;
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
int main(int argc,char** argv){
    LexicalPathPolicy paths;
    if(argc==2 && std::string_view(argv[1])=="--roundtrip"){
        try{
            std::string bytes;
            char c=0;
            while(std::cin.get(c)){if(bytes.size()==codec::max_bytes)return 2;bytes.push_back(c);}
            std::cout<<codec::encode(codec::decode(bytes,paths),paths);return 0;
        }catch(const std::invalid_argument&){return 2;}
    }
    if(argc!=1)return 2;
    Settings s;s.package_directory=u"D:\\Runs\\\u00e9preuve\\\U0001f3c2";
    Book b{1,"Training",{{"Training",s}}};
    const auto raw=codec::encode(b,paths);CHECK(codec::encode(codec::decode(raw,paths),paths)==raw);
    CHECK(raw.find("\\u00e9")!=std::string::npos && raw.find("\\ud83c\\udfc2")!=std::string::npos);
    CHECK(canonical_directory(u"D:/Runs")==u"D:\\Runs");
    for(auto p:{u"D:\\",u"D:\\..\\Runs",u"D:\\NUL.txt",u"D:\\CON .txt",u"D:\\CONOUT$",u"\\\\server\\share",u"D:\\a:stream",u"D:\\a\\"}){
        try{(void)canonical_directory(p);return 1;}catch(const std::invalid_argument&){}
    }
    for(const std::string bad:{"{}","{\"schema\":1,\"schema\":1}","{\"a\":{ \"b\": {\"c\":{\"d\":{\"e\":1}}}}}","{\"x\":\"\\ud800\"}","{\"x\":01}"}){
        try{(void)codec::decode(bad,paths);return 1;}catch(const std::invalid_argument&){}
    }
    return 0;
}
