#include "src/bootstrap/host.hpp"
#include "src/engine_bridge/unavailable.hpp"
#include "src/modules/diagnostics/controller/controller.hpp"
#include "src/modules/diagnostics/view/text.hpp"
#include <iostream>
using namespace supreme;
#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(0)
struct Mock : IObservationSource { Capabilities caps=bit(Capability::identity)|bit(Capability::igt); Observation o{1,1,Origin::synthetic,"fixture",0}; Capabilities capabilities() const noexcept override{return caps;} Observation sample()const override{return o;} };
struct M : Module { std::string id; std::vector<std::string>& log; bool ok=true,throws=false; M(std::string i,std::vector<std::string>& l):id(std::move(i)),log(l){} bool start() override{log.push_back("+"+id); if(throws) throw 1; return ok;} void stop() noexcept override{log.push_back("-"+id);} };
int main(){
try {
UnavailableEngine empty; diagnostics::Query q(empty); diagnostics::Controller c(q); CHECK(c.refresh().origin==Origin::unavailable); CHECK(!c.refresh().igt_ms); CHECK(diagnostics::render(c.refresh()).find("unknown")!=std::string::npos);
Mock mock; diagnostics::Query mq(mock); CHECK(mq.execute().igt_ms==0); CHECK(mq.execute().track=="fixture"); CHECK(diagnostics::render(mq.execute()).find("synthetic")!=std::string::npos); mock.caps=0; CHECK(!mq.execute().track && !mq.execute().igt_ms); mock.o.schema=2; CHECK(mq.execute().origin==Origin::unavailable);
std::cout<<"PASS TEST-CONTRACT-001 unknown/zero, mock replacement, capability and schema filtering\n";
std::vector<std::string> log; log.reserve(30); M a("a",log),b("b",log),f("f",log),g("g",log),d("d",log),e("e",log),t("t",log); f.ok=false; t.throws=true;
{ Host h; h.add({"b",{"a"},{"d"}},b); h.add({"a",{},{}},a); h.add({"f",{},{}},f); h.add({"g",{"f"},{}},g); h.add({"d",{},{},0,false},d); h.add({"e",{},{},bit(Capability::igt)},e); h.add({"t",{},{}},t); h.start(0); CHECK(h.health("a")==Health::running && h.health("b")==Health::running); CHECK(h.health("f")==Health::failed && h.health("g")==Health::unavailable); CHECK(h.health("d")==Health::disabled && h.health("e")==Health::unavailable && h.health("t")==Health::failed); h.stop(); h.stop(); CHECK((log==std::vector<std::string>{"+a","+b","+f","-f","+t","-t","-b","-a"})); }
std::cout<<"PASS TEST-HOST-001 order, reverse shutdown, failure isolation, disabled optional, capability gate\n";
log.clear(); {Host h; h.add({"a",{"b"},{}},a); h.add({"b",{},{"a"}},b); try{h.start(0); CHECK(false);}catch(const std::invalid_argument&){} CHECK(log.empty());} {Host h; h.add({"a",{"missing"},{}},a); try{h.start(0); CHECK(false);}catch(const std::invalid_argument&){} CHECK(log.empty());} {Host h; h.add({"a",{},{}},a); try{h.add({"a",{},{}},b);CHECK(false);}catch(const std::invalid_argument&){} }
std::cout<<"PASS TEST-HOST-002 cycle, missing dependency and duplicate rejection\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
