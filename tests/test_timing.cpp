#include "src/modules/timing/public/interval.hpp"
#include "src/modules/timing/infrastructure/platform_clock.hpp"
#include <iostream>
#include <stdexcept>
using namespace supreme;
using namespace supreme::timing;
#define CHECK(x) do {if(!(x)) {std::cerr<<"line "<<__LINE__<<'\n'; return 1;}} while(false)
class Fake final : public IClock {
public:
    ClockSample value{10,3}; bool fail=false;
    ClockSample sample() const override {if(fail) throw std::runtime_error("fixture"); return value;}
};
int main() {
    const auto max=std::numeric_limits<std::uint64_t>::max();
    CHECK(elapsed_ns({0,3},{1,3})==333333333);
    CHECK(elapsed_ns({0,3},{4,3})==1333333333);
    CHECK(elapsed_ns({max-1,ns_per_second},{max,ns_per_second})==1);
    CHECK(elapsed_ns({0,ns_per_second},{max,ns_per_second})==max);
    CHECK(!elapsed_ns({0,1},{max,1}));
    CHECK(!elapsed_ns({0,0},{1,0}));
    CHECK(!elapsed_ns({0,ns_per_second+1},{1,ns_per_second+1}));
    CHECK(!elapsed_ns({0,3},{1,4}));
    CHECK(!elapsed_ns({1,3},{0,3}));
    Fake fake; Interval interval(fake);
    CHECK(!interval.finish() && !interval.poll() && !interval.result());
    CHECK(interval.start() && !interval.start());
    fake.value.ticks=11; CHECK(interval.poll()==333333333);
    fake.value.ticks=13; CHECK(interval.finish());
    fake.fail=true; CHECK(interval.poll()==ns_per_second && !interval.finish());
    CHECK(interval.result()->begin.ticks==10 && interval.result()->end.ticks==13);
    fake.fail=false; Interval broken(fake); CHECK(broken.start());
    fake.value.ticks=15; CHECK(broken.poll().has_value());
    fake.value.ticks=14; CHECK(!broken.poll() && broken.state()==State::invalid);
    fake.value.ticks=16; CHECK(!broken.finish() && !broken.result());
    Interval failed(fake); fake.fail=true; CHECK(!failed.start() && failed.state()==State::invalid);
    fake.fail=false; Interval changed(fake); CHECK(changed.start());
    fake.value.frequency_hz=4; CHECK(!changed.finish() && changed.state()==State::invalid);
    PlatformClock clock; Interval actual(clock); CHECK(actual.start());
    CHECK(actual.poll().has_value() && actual.finish() && actual.result().has_value());
    CHECK(actual.result()->end.ticks>=actual.result()->begin.ticks);
    return 0;
}
