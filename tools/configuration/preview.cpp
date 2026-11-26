// Standalone development preview, never loaded into the game.
#include "src/modules/configuration/infrastructure/windows_launcher_state.hpp"
#include "src/modules/configuration/public/attempt_settings.hpp"
#include <iostream>
using namespace supreme::configuration;
int wmain(int argc,wchar_t** argv){
    if(argc!=3){std::cerr<<"usage: ssc_config_preview <private-profile-directory> <copied-mods-directory>\n";return 2;}
    try{
        LexicalPathPolicy paths;
        windows::FileStore store(std::filesystem::path(argv[1]).u16string());
        Controller controller(store,paths);
        const auto selection=windows::read_launcher_state(std::filesystem::path(argv[2]).u16string(),paths);
        const AttemptSettings snapshot(controller.resolve(selection.preferences),paths);
        codec::Value result;
        result.object.emplace(u"schema",codec::Value::integer(schema));
        result.object.emplace(u"native_capabilities",codec::Value::boolean(false));
        result.object.emplace(u"diagnostic_requested",codec::Value::boolean(selection.diagnostic_requested));
        result.object.emplace(u"profile",codec::Value::text(codec::widen(snapshot.get().profile)));
        result.object.emplace(u"source",codec::Value::text(snapshot.get().source==Source::launcher?u"launcher":u"profile"));
        result.object.emplace(u"settings",codec::encode_settings(snapshot.get().values));
        std::string out;codec::emit(out,result);std::cout<<out<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<"Configuration unavailable: "<<e.what()<<'\n';return 3;}
}
