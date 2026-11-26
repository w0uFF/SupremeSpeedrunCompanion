#include "src/modules/configuration/infrastructure/launcher_json.hpp"
#include "src/modules/configuration/public/attempt_settings.hpp"
#include <iostream>
using namespace supreme::configuration;
#define CHECK(x) do{if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
struct Memory final:Store{
    Book book{1,"Training",{{"Training",Settings{}},{"Tryhard",Settings{}}}};
    Book load()const override{return book;}
    void save(const Book& b)override{book=b;}
};
int main(int argc,char** argv){
    LexicalPathPolicy paths;Memory store;store.book.profiles[1].values.auto_record=true;
    Controller controller(store,paths);
    if(argc==2 && std::string_view(argv[1])=="--resolve"){
        try{
            std::string input;char c=0;
            while(std::cin.get(c)){if(input.size()==codec::max_bytes)return 2;input.push_back(c);}
            auto launch=launcher::decode(input,paths);
            auto snapshot=controller.resolve(launch.preferences);
            codec::Value out;
            out.object.emplace(u"diagnostic_requested",codec::Value::boolean(launch.diagnostic_requested));
            out.object.emplace(u"profile",codec::Value::text(codec::widen(snapshot.profile)));
            out.object.emplace(u"settings",codec::encode_settings(snapshot.values));
            out.object.emplace(u"source",codec::Value::text(snapshot.source==Source::launcher?u"launcher":u"profile"));
            std::string bytes;codec::emit(bytes,out);std::cout<<bytes;return 0;
        }catch(const std::invalid_argument&){return 2;}
    }
    if(argc!=1)return 2;
    auto launch=launcher::decode(R"({"active":{"order":["other","ssc"],"enabled":["ssc"],"configuration":{"other":{"x":[null,-12.5,1e20]},"ssc":{"profile_name":"Tryhard","load_diagnostic":true,"use_launcher_overrides":true,"bitrate_mbps":1.2e1,"fps":"60","package_directory":"D:/Runs/\u00e9"}}},"presets":{"unused":{"arbitrary":true}}})",paths);
    CHECK(launch.diagnostic_requested && launch.preferences.whole_override->bitrate_mbps==12);
    auto session=controller.resolve(launch.preferences);
    const AttemptSettings attempt(session,paths);
    session.values.fps=30;
    controller.replace_profile("Tryhard",session.values);
    CHECK(attempt.get().values.fps==60 && attempt.get().values.package_directory==u"D:\\Runs\\\u00e9");
    CHECK(!launcher::decode(R"({"active":{"enabled":["ssc"],"configuration":{"ssc":{"load_diagnostic":true}}}})",paths).diagnostic_requested);
    CHECK(controller.resolve(launcher::decode(R"({"active":{}})",paths).preferences).source==Source::profile);
    for(const char* input:{R"({"active":{"order":[3]}})",R"({"active":{"configuration":{"ssc":{"bitrate_mbps":12.0001}}}})",R"({"active":{"configuration":{"ssc":{"unknown":true}}}})",R"({"active":{},"active":{}})",R"({"active":{"configuration":{"ssc":{"package_directory":"D:\u0000"}}}})",R"({"active":{"configuration":{"ssc":{"bitrate_mbps":1e300}}}})",R"({"active":{"configuration":{"ssc":{"fps":60}}}})"}){
        try{(void)launcher::decode(input,paths);return 1;}catch(const std::invalid_argument&){}
    }
    return 0;
}
