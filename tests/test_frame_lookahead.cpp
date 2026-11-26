#include "src/modules/recorder/public/frame_lookahead.hpp"
#include <iostream>
using namespace supreme::recorder;
#define CHECK(x) do{if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
TimedFrame frame(std::uint64_t t){return {t,std::vector<std::uint8_t>(fixture_bytes,16)};}
int main(){
    FrameLookahead look;
    CHECK(!look.push(frame(1000000)));
    auto a=look.push(frame(1300000));CHECK(a && a->pts==0 && a->duration==300000 && a->bytes.size()==fixture_bytes);
    try{(void)look.push(frame(1300000));return 1;}catch(const std::invalid_argument&){}
    CHECK(look.count()==2);
    auto b=look.push(frame(2800000));CHECK(b && b->pts==300000 && b->duration==1000000);
    auto c=look.finish();CHECK(c.pts==1800000 && c.duration==400000);
    try{(void)look.finish();return 1;}catch(const std::logic_error&){}
    try{(void)look.push(frame(3000000));return 1;}catch(const std::invalid_argument&){}
    FrameLookahead bound;
    CHECK(!bound.push(frame(0)));
    try{(void)bound.push(frame(FrameLookahead::max_span+1));return 1;}catch(const std::invalid_argument&){}
    for(std::size_t i=1;i<FrameLookahead::max_frames;++i)CHECK(bound.push(frame(i*400000)));
    try{(void)bound.push(frame(160000000));return 1;}catch(const std::invalid_argument&){}
    return 0;
}
