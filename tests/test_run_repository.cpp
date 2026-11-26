#include "src/modules/run_library/public/repository.hpp"
#include <iostream>
using namespace supreme::run_library;
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<'\n';return 1;}}while(false)
int main(){
    const Record ref{"id","category","profile","events","digest",Origin::local,Status::finished,0,100};
    CHECK(comparable(ref,ref,Method::igt));
    CHECK(comparable(ref,ref,Method::rta));
    auto r=ref;r.igt_ms.reset();CHECK(!comparable(r,ref,Method::igt));CHECK(comparable(r,ref,Method::rta));
    r=ref;r.origin=Origin::imported;CHECK(!comparable(r,ref,Method::igt));
    r=ref;r.origin=Origin::synthetic;CHECK(!comparable(r,ref,Method::igt));
    r=ref;r.status=Status::restarted;CHECK(!comparable(r,ref,Method::igt));
    r=ref;r.event_semantics="other";CHECK(!comparable(r,ref,Method::igt));
    r=ref;r.profile="other";CHECK(!comparable(r,ref,Method::igt));
    r=ref;r.category="other";CHECK(!comparable(r,ref,Method::igt));
    CHECK(!comparable(ref,ref,static_cast<Method>(99)));
    return 0;
}
