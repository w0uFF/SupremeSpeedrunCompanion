#pragma once
#include "src/modules/configuration/infrastructure/windows_profile_store.hpp"
#include "src/modules/configuration/infrastructure/launcher_json.hpp"

namespace supreme::configuration::windows {
// Caller supplies an explicit trusted, existing mods directory. No discovery of game state,
// no path guessing, redirects or modification. A missing/invalid file is an error.
inline launcher::Selection read_launcher_state(std::u16string_view mods_directory,const PathPolicy& paths){
    const auto directory=existing_directory(mods_directory);
    return launcher::decode(read_plain_file(directory/L"mods.json"),paths);
}
}
