#include "src/modules/configuration/public/settings.hpp"
#include <iostream>
using namespace supreme::configuration;
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
struct Paths final:PathPolicy {
    bool accepts(std::u16string_view path)const override{return path==u"D:\\Runs";}
};
struct Memory final:Store {
    Book book;
    bool fail=false;
    Book load()const override{return book;}
    void save(const Book& b)override{if(fail)throw std::runtime_error("fixture write failure");book=b;}
};
int main(){
    Paths paths;Memory store;Settings training,tryhard;
    tryhard.auto_record=true;tryhard.fps=60;tryhard.package_directory=u"D:\\Runs";
    store.book={1,"Training",{{"Training",training},{"Tryhard",tryhard}}};
    Controller controller(store,paths);
    const auto first=controller.resolve({"",{}});CHECK(!first.values.auto_record && first.source==Source::profile);
    const auto attempt=controller.resolve({"Tryhard",{}});CHECK(attempt.values.auto_record && attempt.values.fps==60);
    const auto override=controller.resolve({"Tryhard",training});CHECK(!override.values.auto_record && override.source==Source::launcher);
    const auto edited=controller.session_edit(attempt,training);CHECK(edited.source==Source::session && !edited.values.auto_record);
    CHECK(controller.resolve({"Tryhard",{}}).values.auto_record);
    controller.replace_profile("Tryhard",training);
    CHECK(attempt.values.auto_record && attempt.values.fps==60 && !controller.resolve({"Tryhard",{}}).values.auto_record);
    store.fail=true;
    try{controller.replace_profile("Tryhard",tryhard);return 1;}catch(const std::runtime_error&){}
    CHECK(!controller.resolve({"Tryhard",{}}).values.auto_record);
    for(unsigned n=0;n<4;++n){
        auto invalid=training;
        if(n==0)invalid.fps=120;
        if(n==1)invalid.resolution=static_cast<Resolution>(99);
        if(n==2)invalid.restart_key=invalid.record_key=Key::f5;
        if(n==3)invalid.package_directory=u"unvalidated";
        try{(void)controller.session_edit(attempt,invalid);return 1;}catch(const std::invalid_argument&){}
    }
    store.book.profiles.push_back({"training",training});
    try{(void)controller.resolve({"",{}});return 1;}catch(const std::invalid_argument&){}
    return 0;
}
