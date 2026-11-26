#include "tools/media/fixture_performance.hpp"
#include <iostream>
using namespace ssc::media_fixture;
#define CHECK(x) do{if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
int main(){
    std::vector<std::int64_t> samples;
    for(std::int64_t i=100;i>0;--i)samples.push_back(i);
    auto result=summarize_latency(samples);
    CHECK(result.count==100 && result.median_us==50 && result.p95_us==95 && result.p99_us==99 && result.max_us==100);
    CHECK(samples.front()==100 && samples.back()==1);
    result=summarize_latency({0});
    CHECK(result.count==1 && result.median_us==0 && result.max_us==0);
    result=summarize_latency({8,2,4});
    CHECK(result.median_us==4 && result.p95_us==8 && result.p99_us==8);
    for(const auto& bad:std::vector<std::vector<std::int64_t>>{{},{-1},std::vector<std::int64_t>(latency_sample_limit+1,1)}){
        try{(void)summarize_latency(bad);return 1;}catch(const std::invalid_argument&){}
    }
    CHECK(summarize_latency(std::vector<std::int64_t>(latency_sample_limit,1)).count==latency_sample_limit);
    return 0;
}
