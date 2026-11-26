#include "src/modules/recorder/public/quality.hpp"
#include <iostream>
using namespace supreme::recorder;
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
int main(){
    const auto max=std::numeric_limits<std::uint64_t>::max();
    CHECK(qpc_to_100ns(100,100,3)==0);
    CHECK(qpc_to_100ns(100,101,3)==3333333);
    CHECK(qpc_to_100ns(100,104,3)==13333333);
    CHECK(qpc_to_100ns(max-1,max,10000000)==1);
    CHECK(!qpc_to_100ns(1,0,1));
    CHECK(!qpc_to_100ns(0,1,0));
    CHECK(!qpc_to_100ns(0,1,1000000001));
    CHECK(!qpc_to_100ns(0,max,1));
    Coverage c;CHECK(!c.internal_gap());
    CHECK(c.observe(10,20));CHECK(c.observe(50,10));
    CHECK(c.packets==2 && c.duration==30 && c.first==10 && c.end==60 && c.internal_gap()==20);
    CHECK(!c.observe(59,1));CHECK(!c.observe(60,0));CHECK(!c.observe(max,1));
    CHECK(c.packets==2 && c.end==60);
    Coverage all;CHECK(all.observe(0,max));CHECK(all.internal_gap()==0);
    return 0;
}
