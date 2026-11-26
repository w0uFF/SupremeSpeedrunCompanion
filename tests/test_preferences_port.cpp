#include "src/modules/player_setup/application/preferences.hpp"
#include <iostream>
using namespace supreme::player_setup;
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
class Store final:public PreferencesStore {
public:
    std::optional<Preferences> data;
    bool fail=false;
    std::optional<Preferences> load() const override {if(fail) throw std::runtime_error("read failed");return data;}
    void save(const Preferences& p) override {if(fail) throw std::runtime_error("write failed");data=p;}
};
class Allowed final:public Availability {
public:
    bool enabled=true;
    bool allows(const Loadout&) const override {return enabled;}
};
int main(){
    Store s; Allowed a; PreferencesController c(s,a);
    CHECK(!c.remembered());
    c.update({true,Loadout{"rider","board"},RecordingPolicy::ask});
    CHECK(c.remembered()->board=="board");
    a.enabled=false; CHECK(!c.remembered());
    try {c.update({true,Loadout{"other","board"},RecordingPolicy::automatic});return 1;} catch(const std::invalid_argument&){}
    CHECK(s.data->saved->rider=="rider");
    a.enabled=true;
    c.update({false,s.data->saved,RecordingPolicy::training});CHECK(!c.remembered());
    s.fail=true;
    try {c.remembered();return 1;} catch(const std::runtime_error&){}
    try {c.update({false,std::nullopt,RecordingPolicy::ask});return 1;} catch(const std::runtime_error&){}
    CHECK(s.data->recording==RecordingPolicy::training);
    return 0;
}
