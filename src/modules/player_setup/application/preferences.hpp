#pragma once
#include "src/modules/player_setup/public/setup.hpp"
#include <stdexcept>
namespace supreme::player_setup {
struct Preferences { bool remember; std::optional<Loadout> saved; RecordingPolicy recording; };
class PreferencesStore {
public:
    virtual ~PreferencesStore()=default;
    virtual std::optional<Preferences> load() const=0;
    virtual void save(const Preferences&)=0;
};
// Store is injected; storage errors propagate, never treated as missing preferences.
class PreferencesController {
    PreferencesStore& store_;
    const Availability& available_;
public:
    PreferencesController(PreferencesStore& store,const Availability& available):store_(store),available_(available){}
    std::optional<Loadout> remembered() const {
        const auto prefs=store_.load();
        return prefs?resolve(prefs->remember,prefs->saved,available_):std::nullopt;
    }
    void update(const Preferences& prefs) {
        if(prefs.saved && (prefs.saved->rider.empty() || prefs.saved->board.empty() || !available_.allows(*prefs.saved)))
            throw std::invalid_argument("unavailable loadout");
        switch(prefs.recording) {
        case RecordingPolicy::automatic: case RecordingPolicy::ask: case RecordingPolicy::training: break;
        default: throw std::invalid_argument("unknown recording policy");
        }
        store_.save(prefs);
    }
};
}
