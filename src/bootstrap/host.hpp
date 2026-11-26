#pragma once
#include "contracts/observation.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>
#include <vector>
namespace supreme {
enum class Health { pending, disabled, unavailable, running, failed, stopped };
struct Descriptor { std::string id; std::vector<std::string> required, optional; Capabilities capabilities=0; bool enabled=true; };
class Module { public: virtual ~Module()=default; virtual bool start()=0; virtual void stop() noexcept=0; };
// Owner-thread only. Borrowed modules must outlive host. No registration during execution.
class Host {
    struct Entry { Descriptor d; Module* m; Health h=Health::pending; };
    std::map<std::string,Entry> entries_; std::vector<std::string> active_; bool started_=false;
    void visit(const std::string& id,std::map<std::string,int>& marks,std::vector<std::string>& order) const {
        if(marks[id]==1) throw std::invalid_argument("dependency cycle: "+id);
        if(marks[id]==2) return;
        marks[id]=1; const auto& d=entries_.at(id).d;
        for(const auto& dep:d.required) { if(!entries_.count(dep)) throw std::invalid_argument("missing dependency: "+dep); visit(dep,marks,order); }
        for(const auto& dep:d.optional) if(entries_.count(dep)) visit(dep,marks,order);
        marks[id]=2; order.push_back(id);
    }
public:
    Host()=default; Host(const Host&)=delete; Host& operator=(const Host&)=delete; ~Host(){stop();}
    void add(Descriptor d,Module& m) { if(started_||d.id.empty()) throw std::logic_error("invalid registration"); auto id=d.id; if(!entries_.emplace(id,Entry{std::move(d),&m}).second) throw std::invalid_argument("duplicate module"); }
    void start(Capabilities caps) {
        if(started_) throw std::logic_error("host already started");
        std::map<std::string,int> marks; std::vector<std::string> order;
        for(const auto& e:entries_) visit(e.first,marks,order); // no side effects until graph validated
        active_.reserve(order.size()); started_=true;
        for(const auto& id:order) {
            auto& e=entries_.at(id); auto& d=e.d;
            if(!d.enabled) {e.h=Health::disabled; continue;}
            if((d.capabilities&caps)!=d.capabilities || std::any_of(d.required.begin(),d.required.end(),[&](const auto& dep){return entries_.at(dep).h!=Health::running;})) {e.h=Health::unavailable; continue;}
            try { if(e.m->start()) {active_.push_back(id); e.h=Health::running;} else {e.m->stop(); e.h=Health::failed;} }
            catch(...) {e.m->stop(); e.h=Health::failed;}
        }
    }
    void stop() noexcept { for(auto i=active_.rbegin();i!=active_.rend();++i) {auto& e=entries_.at(*i);e.m->stop();e.h=Health::stopped;} active_.clear(); }
    Health health(const std::string& id) const {return entries_.at(id).h;}
};
}
