#pragma once
#include <array>
#include <optional>
#include <string_view>
namespace supreme::categories {
enum class Format { individual, all_tracks, championship };
enum class Track { alpine_easy, alpine_medium, alpine_hard, forest_easy, forest_medium, forest_hard, village_easy, village_medium, village_hard };
struct Category { std::string_view id, label; Format format; std::optional<Track> track; };
inline constexpr std::array<Category,11> catalog{{
 {"wk6y97ed","Alpine - easy",Format::individual,Track::alpine_easy},
 {"n2yzw5ek","Alpine - medium",Format::individual,Track::alpine_medium},
 {"7kjwxpgd","Alpine - hard",Format::individual,Track::alpine_hard},
 {"z27jn652","Forest - easy",Format::individual,Track::forest_easy},
 {"zdn96472","Forest - medium",Format::individual,Track::forest_medium},
 {"q256yqwk","Forest - hard",Format::individual,Track::forest_hard},
 {"jdr4zx02","Village - easy",Format::individual,Track::village_easy},
 {"jdzzy8xd","Village - medium",Format::individual,Track::village_medium},
 {"02qz5oz2","Village - hard",Format::individual,Track::village_hard},
 {"zd31pr82","all tracks",Format::all_tracks,std::nullopt},
 {"zdn54jqd","Beating the championship",Format::championship,std::nullopt}
}};
inline constexpr const Category* find(std::string_view id) noexcept { for (const auto& c:catalog) if(c.id==id) return &c; return nullptr; }
inline constexpr bool valid(Track t) noexcept { return static_cast<unsigned>(t)<9; }
// No ordering is declared official. This validates an explicit proposed practice plan.
inline bool valid_plan(const std::array<Track,9>& plan) noexcept {
 std::array<bool,9> seen{}; for(auto t:plan) { if(!valid(t)) return false; auto i=static_cast<unsigned>(t); if(seen[i]) return false; seen[i]=true; } return true;
}
}
