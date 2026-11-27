#include "src/engine_bridge/reference_level_path.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace supreme::engine_bridge;
using supreme::categories::Track;
static void check(bool ok) { if (!ok) throw std::runtime_error("reference level classification mismatch"); }
int main() {
    try {
        const char* regions[]{"Alpine", "Forest", "Village"};
        const char* difficulties[]{"Easy", "Medium", "Hard"};
        const char* weather[]{"Day", "Night", "Sunset", "cloudy"};
        const Weather expected_weather[]{Weather::day, Weather::night, Weather::sunset, Weather::cloudy};
        const Track expected[][3]{{Track::alpine_easy, Track::alpine_medium, Track::alpine_hard},
            {Track::forest_easy, Track::forest_medium, Track::forest_hard},
            {Track::village_easy, Track::village_medium, Track::village_hard}};
        int competitive = 0, other = 0;
        for (int r = 0; r < 3; ++r) for (int w = 0; w < 4; ++w) {
            for (int d = 0; d < 3; ++d) {
                auto path = std::string("Data/Levels/") + regions[r] + "/Tracks/" + difficulties[d] + "/" + weather[w] + "/Level_Data.txt";
                auto x = classify_reference_level(path);
                check(x.kind == LevelKind::competitive && x.track == expected[r][d] && x.weather == expected_weather[w]);
                ++competitive;
            }
            for (const auto* type : {"Halfpipe", "Ramp"}) {
                auto x = classify_reference_level(std::string("Data/Levels/") + regions[r] + "/" + type + "/Easy/" + weather[w] + "/Level_Data.txt");
                check(x.kind == (std::string_view(type) == "Ramp" ? LevelKind::ramp : LevelKind::halfpipe) && !x.track && x.weather == expected_weather[w]);
                ++other;
            }
        }
        for (int w = 0; w < 4; ++w) {
            auto x = classify_reference_level(std::string("Data/Levels/Practice/Tracks/Easy/") + weather[w] + "/Level_Data.txt");
            check(x.kind == LevelKind::practice && !x.track && x.weather == expected_weather[w]);
            ++other;
        }
        check(classify_reference_level("dAtA\\Levels/Alpine\\Tracks/Easy/day/LEVEL_DATA.TXT").track == Track::alpine_easy);
        for (const auto* path : {"", "/Data/Levels/Alpine/Tracks/Easy/day/Level_Data.txt", "C:/Data/Levels/Alpine/Tracks/Easy/day/Level_Data.txt", "Data//Levels/Alpine/Tracks/Easy/day/Level_Data.txt", "Data/Levels/../Tracks/Easy/day/Level_Data.txt", "Data/Levels/Alpine/Tracks/Easy/day/Object_Data.txt", "Data/Levels/Practice/Tracks/Hard/day/Level_Data.txt", "Data/Levels/Alpine/Ramp/Hard/day/Level_Data.txt", "Data/Levels/Forest/Tracks/Expert/day/Level_Data.txt", "Data/Levels/Forest/Tracks/Easy/rain/Level_Data.txt", "Data/Levels/Alpine/Tracks/Easy/day/Level_Data.txt/", "Data/Levels/Alpine/Tracks/Easy/day/Level_Data.txt:stream"}) {
            auto x = classify_reference_level(path);
            check(x.kind == LevelKind::unknown && !x.track && !x.weather);
        }
        check(classify_reference_level(std::string(129,'x')).kind == LevelKind::unknown);
        auto embedded = std::string("Data/Levels/Alpine/Tracks/Easy/day/Level_Data.txt");
        embedded.push_back('\0');
        check(classify_reference_level(embedded).kind == LevelKind::unknown);
        std::cout << "Reference paths: " << competitive << " competitive, " << other << " other; negative cases PASS\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
