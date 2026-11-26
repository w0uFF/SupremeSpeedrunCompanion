#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace ssc::media_fixture {
// Test-only bounded nearest-rank summary; no production performance threshold.
struct LatencySummary {
    std::size_t count;
    std::int64_t median_us, p95_us, p99_us, max_us;
};
inline constexpr std::size_t latency_sample_limit=1440;
inline LatencySummary summarize_latency(std::vector<std::int64_t> samples) {
    if(samples.empty() || samples.size()>latency_sample_limit ||
       std::any_of(samples.begin(),samples.end(),[](auto value){return value<0;}))
        throw std::invalid_argument("fixture latency sample bounds");
    std::sort(samples.begin(),samples.end());
    const auto rank=[&samples](std::size_t percent){return samples[(samples.size()*percent+99)/100-1];};
    return {samples.size(),rank(50),rank(95),rank(99),samples.back()};
}
}
