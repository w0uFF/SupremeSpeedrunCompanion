#include "src/modules/configuration/infrastructure/windows_profile_store.hpp"
#include <iostream>
using namespace supreme::configuration;
namespace win=supreme::configuration::windows;
struct RejectPath final:PathPolicy {bool accepts(std::u16string_view)const override{return false;}};
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 2;
    try{
        const auto directory=std::filesystem::path(argv[2]).u16string();
        win::FileStore store(directory);
        const auto action=std::wstring_view(argv[1]);
        if(action==L"create"){
            Settings initial;store.create({1,"Training",{{"Training",initial}}});
        }else if(action==L"replace"){
            LexicalPathPolicy policy;Controller controller(store,policy);
            auto values=controller.resolve({"Training",{}}).values;
            values.fps=60;values.input_overlay=true;
            controller.replace_profile("Training",values);
        }else if(action==L"read"){
            LexicalPathPolicy policy;
            std::cout<<codec::encode(store.load(),policy);
        }else if(action==L"directory"){
            // Existing directory validation only; do not require a particular free-space quantity on CI.
            if(win::existing_directory(directory).u16string()!=canonical_directory(directory))return 1;
            try{(void)win::existing_directory(directory+u"\\missing");return 1;}catch(const std::runtime_error&){}
            Settings s;s.min_free_gib=0;
            try{(void)win::package_directory(s,directory);return 1;}catch(const std::invalid_argument&){}
        }else return 2;
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 3;}
}
