#pragma once
#include "src/modules/categories/public/catalog.hpp"
#include <array>
#include <optional>
#include <string_view>

namespace supreme::engine_bridge {
// Lexical reference classification only, never a loaded-track observation.
enum class LevelKind { unknown, competitive, practice, halfpipe, ramp };
enum class Weather { day, night, sunset, cloudy };
struct ReferenceLevel {
    LevelKind kind = LevelKind::unknown;
    std::optional<categories::Track> track;
    std::optional<Weather> weather;
};
namespace level_path_detail {
constexpr char lower_ascii(char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}
constexpr bool equal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (lower_ascii(a[i]) != lower_ascii(b[i])) return false;
    return true;
}
}
inline ReferenceLevel classify_reference_level(std::string_view path) noexcept {
    using namespace level_path_detail;
    // Exactly seven relative components; no normalization beyond ASCII case and separators.
    if (path.empty() || path.size() > 128) return {};
    std::array<std::string_view, 7> parts{};
    std::size_t start = 0, count = 0;
    for (std::size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/' || path[i] == '\\') {
            if (i == start || count == parts.size()) return {};
            parts[count++] = path.substr(start, i - start);
            start = i + 1;
        }
    }
    if (count != parts.size() || !equal(parts[0], "Data") || !equal(parts[1], "Levels") ||
        !equal(parts[6], "Level_Data.txt")) return {};
    constexpr std::array<std::string_view, 4> weather_names{"day", "night", "sunset", "cloudy"};
    constexpr std::array<Weather, 4> weather_values{Weather::day, Weather::night, Weather::sunset, Weather::cloudy};
    std::optional<Weather> weather;
    for (std::size_t i = 0; i < weather_names.size(); ++i)
        if (equal(parts[5], weather_names[i])) weather = weather_values[i];
    if (!weather) return {};
    if (equal(parts[2], "Practice")) {
        if (equal(parts[3], "Tracks") && equal(parts[4], "Easy"))
            return {LevelKind::practice, std::nullopt, weather};
        return {};
    }
    using categories::Track;
    constexpr std::array<std::string_view, 3> regions{"Alpine", "Forest", "Village"};
    constexpr std::array<std::string_view, 3> difficulties{"Easy", "Medium", "Hard"};
    constexpr std::array<std::array<Track, 3>, 3> tracks{{
        {Track::alpine_easy, Track::alpine_medium, Track::alpine_hard},
        {Track::forest_easy, Track::forest_medium, Track::forest_hard},
        {Track::village_easy, Track::village_medium, Track::village_hard}}};
    for (std::size_t r = 0; r < regions.size(); ++r) {
        if (!equal(parts[2], regions[r])) continue;
        if (equal(parts[4], "Easy")) {
            if (equal(parts[3], "Halfpipe")) return {LevelKind::halfpipe, std::nullopt, weather};
            if (equal(parts[3], "Ramp")) return {LevelKind::ramp, std::nullopt, weather};
        }
        if (equal(parts[3], "Tracks")) {
            for (std::size_t d = 0; d < difficulties.size(); ++d)
                if (equal(parts[4], difficulties[d])) return {LevelKind::competitive, tracks[r][d], weather};
        }
    }
    return {};
}
}
