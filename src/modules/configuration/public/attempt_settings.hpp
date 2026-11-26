#pragma once
#include "src/modules/configuration/public/settings.hpp"
namespace supreme::configuration {
// Owned, read-only settings for an individual attempt; no live references to profile or controller.
class AttemptSettings final {
    const Snapshot snapshot_;
public:
    AttemptSettings(Snapshot snapshot,const PathPolicy& paths):snapshot_(std::move(snapshot)){
        (void)folded(snapshot_.profile);
        validate(snapshot_.values,paths);
        if(!in_range(snapshot_.source,Source::session))throw std::invalid_argument("snapshot source");
    }
    const Snapshot& get()const noexcept{return snapshot_;}
};
}
