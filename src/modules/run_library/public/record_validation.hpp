#pragma once
#include "src/modules/run_library/public/repository.hpp"
#include <limits>
#include <stdexcept>

namespace supreme::run_library {
inline bool hex_id(std::string_view s,std::size_t length) noexcept {
    if(s.size()!=length)return false;
    for(char c:s)if(!((c>='0' && c<='9') || (c>='a' && c<='f')))return false;
    return true;
}
inline void validate_id(std::string_view id){
    if(!hex_id(id,32))throw std::invalid_argument("run identity");
}
// Stable ASCII comparison keys; display labels are separate UTF-8 text.
inline bool key(std::string_view s) noexcept {
    if(s.empty() || s.size()>128)return false;
    for(unsigned char c:s)if(c<33 || c>126)return false;
    return true;
}
inline void validate(const Record& r){
    validate_id(r.run_id);
    if(!key(r.category) || !hex_id(r.profile,64) || !key(r.event_semantics) ||
       !hex_id(r.manifest_sha256,64))throw std::invalid_argument("record metadata");
    if(r.origin!=Origin::local && r.origin!=Origin::imported && r.origin!=Origin::synthetic)
        throw std::invalid_argument("origin");
    if(r.status!=Status::finished && r.status!=Status::restarted && r.status!=Status::abandoned && r.status!=Status::failed)
        throw std::invalid_argument("status");
    constexpr auto max=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if((r.igt_ms && (*r.igt_ms>max || r.status!=Status::finished)) || (r.rta_ns && *r.rta_ns>max))
        throw std::invalid_argument("time");
}
inline bool identical(const Record& a,const Record& b) noexcept {
    return a.run_id==b.run_id && a.category==b.category && a.profile==b.profile &&
        a.event_semantics==b.event_semantics && a.manifest_sha256==b.manifest_sha256 &&
        a.origin==b.origin && a.status==b.status && a.igt_ms==b.igt_ms && a.rta_ns==b.rta_ns;
}
inline void validate(const Annotation& a){
    if(a.name.empty() || a.name.size()>256)throw std::invalid_argument("label length");
    for(std::size_t i=0;i<a.name.size();){
        const auto c=static_cast<unsigned char>(a.name[i++]);
        unsigned cp=c,count=0,minimum=0;
        if(c>=0x80){
            if(c>=0xc2 && c<=0xdf){count=1;minimum=0x80;cp=c&31;}
            else if(c>=0xe0 && c<=0xef){count=2;minimum=0x800;cp=c&15;}
            else if(c>=0xf0 && c<=0xf4){count=3;minimum=0x10000;cp=c&7;}
            else throw std::invalid_argument("label UTF-8");
            if(count>a.name.size()-i)throw std::invalid_argument("label UTF-8");
            for(unsigned j=0;j<count;++j){
                const auto d=static_cast<unsigned char>(a.name[i++]);
                if((d&0xc0)!=0x80)throw std::invalid_argument("label UTF-8");
                cp=(cp<<6)|(d&63);
            }
            if(cp<minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))
                throw std::invalid_argument("label UTF-8");
        }
        if(cp<32 || (cp>=127 && cp<=159))throw std::invalid_argument("label control");
    }
}
struct Comparison {
    std::string category,profile,event_semantics;
    Method method;
};
inline void validate(const Comparison& q){
    if(!key(q.category) || !hex_id(q.profile,64) || !key(q.event_semantics) ||
       (q.method!=Method::igt && q.method!=Method::rta))throw std::invalid_argument("comparison");
}
class QueryRepository:public Repository {
public:
    virtual std::optional<Annotation> annotation(std::string_view id)const=0;
    virtual std::optional<Record> best_local(const Comparison&)const=0;
};
}
