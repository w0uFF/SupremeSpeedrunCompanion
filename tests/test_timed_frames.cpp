#include "src/modules/recorder/public/timed_frames.hpp"
#include <sstream>
#include <iostream>
using namespace supreme::recorder;
#define CHECK(x) do{if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
int main(){
    std::vector<TimedFrame> frames;
    for(auto t:{10000000ull,10400000ull,12000000ull})frames.push_back({t,std::vector<std::uint8_t>(fixture_bytes,16)});
    const auto intervals=frame_intervals(frames);
    CHECK(intervals[0].pts==0 && intervals[0].duration==400000);
    CHECK(intervals[1].pts==400000 && intervals[1].duration==1000000);
    CHECK(intervals[2].pts==2000000 && intervals[2].duration==fixture_tail);
    std::stringstream stream;write_timed_frames(stream,frames);
    const auto restored=read_timed_frames(stream);CHECK(restored.size()==3 && restored[1].time==frames[1].time && restored[2].pixels==frames[2].pixels);
    auto invalid=frames;invalid[1].time=invalid[0].time;
    try{(void)frame_intervals(invalid);return 1;}catch(const std::invalid_argument&){}
    auto raw=stream.str();raw.pop_back();std::stringstream truncated(raw);
    try{(void)read_timed_frames(truncated);return 1;}catch(const std::invalid_argument&){}
    return 0;
}
