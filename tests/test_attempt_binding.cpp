#include "src/modules/exporter/public/attempt_binding.hpp"
#include <iostream>
using namespace supreme::configuration;
#define CHECK(x) do{if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
int main(){
    LexicalPathPolicy paths;
    Snapshot session{"Private Profile",Settings{},Source::launcher};
    session.values.auto_record=true;session.values.fps=60;
    session.values.package_directory=u"D:\\Private\\Runs";
    const AttemptSettings fixed(session,paths);
    supreme::exporter::AttemptBinding binding(std::string(32,'1'),std::string(32,'2'),fixed);
    session.values.auto_record=false;session.values.fps=30;
    const auto json=binding.evidence_json();
    CHECK(json.find("Private")==std::string::npos);
    CHECK(json.find("\"fps\":\"60\"")!=std::string::npos);
    CHECK(json.find("\"auto_record\":true")!=std::string::npos);
    CHECK(json.find("\"package_directory\":\"\"")!=std::string::npos);
    try{supreme::exporter::AttemptBinding bad("bad",std::string(32,'2'),fixed);return 1;}catch(const std::invalid_argument&){}
    std::cout<<json;
    return 0;
}
