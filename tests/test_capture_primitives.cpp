#include "src/modules/recorder/public/pixel_conversion.hpp"
#include "src/modules/recorder/public/audio_observation.hpp"
#include <iostream>
using namespace supreme::recorder;
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
int main(){
    // Independently calculated BT.601 limited-range reference vectors.
    std::vector<std::uint8_t> black(24,0),white(24,255);
    CHECK(bgra_to_nv12(black.data(),black.size(),12,2,2)==std::vector<std::uint8_t>({16,16,16,16,128,128}));
    CHECK(bgra_to_nv12(white.data(),white.size(),12,2,2)==std::vector<std::uint8_t>({235,235,235,235,128,128}));
    const std::uint8_t red[]={0,0,255,1,0,0,255,2,0,0,255,3,0,0,255,4};
    CHECK(bgra_to_nv12(red,sizeof(red),8,2,2)==std::vector<std::uint8_t>({82,82,82,82,90,240}));
    const std::uint8_t mixed[]={0,0,255,0,0,255,0,0,255,0,0,0,255,255,255,0};
    CHECK(bgra_to_nv12(mixed,sizeof(mixed),8,2,2)==std::vector<std::uint8_t>({82,144,41,235,128,128}));
    for(int i=0;i<4;++i){try{
        bgra_to_nv12(i==0?nullptr:black.data(),i==1?23:24,i==2?7:12,i==3?3:2,2);return 1;
    }catch(const std::invalid_argument&){}}
    AudioObservation a;
    CHECK(a.observe(100,10,false,false,false));CHECK(a.observe(110,10,true,false,false));
    CHECK(a.observe(125,10,false,true,false));CHECK(a.observe(130,10,false,false,false));
    CHECK(a.observe(200,10,false,false,true));CHECK(a.observe(500,10,false,false,false));
    CHECK(a.packets==6 && a.frames==60 && a.silent==10 && a.discontinuities==1 && a.timestamp_errors==1 && a.gaps==5 && a.regressions==1);
    CHECK(!a.observe(0,0,false,false,false));CHECK(!a.observe(UINT64_MAX,1,false,false,false));
    CHECK(a.frames==60 && a.next_position==510);
    return 0;
}
