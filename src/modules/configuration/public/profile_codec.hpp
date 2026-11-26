#pragma once
#include "src/modules/configuration/public/path_policy.hpp"
#include <map>

namespace supreme::configuration::codec {
inline constexpr std::size_t max_bytes=256u*1024u;
// Restricted schema-1 ASCII JSON: objects, strings, booleans, nonnegative integers.
// UTF-16 string units travel as JSON \u escapes. No general-purpose JSON dependency.
struct Value {
    enum class Kind { object, string, boolean, number };
    Kind kind=Kind::object;
    std::map<std::u16string,Value> object;
    std::u16string str;
    bool flag=false;
    unsigned number=0;
    static Value text(std::u16string v){Value n;n.kind=Kind::string;n.str=std::move(v);return n;}
    static Value boolean(bool v){Value n;n.kind=Kind::boolean;n.flag=v;return n;}
    static Value integer(unsigned v){Value n;n.kind=Kind::number;n.number=v;return n;}
};
[[noreturn]] inline void invalid(){throw std::invalid_argument("invalid schema-1 profile JSON");}
class Parser {
    std::string_view s_;
    std::size_t at_=0;
    void ws(){while(at_<s_.size() && (s_[at_]==' ' || s_[at_]=='\t' || s_[at_]=='\n' || s_[at_]=='\r'))++at_;}
    bool take(char c){ws();if(at_<s_.size() && s_[at_]==c){++at_;return true;}return false;}
    char next(){if(at_==s_.size())invalid();return s_[at_++];}
    std::u16string string(){
        if(!take('"'))invalid();
        std::u16string r;
        for(;;){
            auto c=static_cast<unsigned char>(next());
            if(c=='"')break;
            if(c=='\\'){
                const auto e=next();
                if(e=='u'){
                    unsigned n=0;
                    for(unsigned i=0;i<4;++i){
                        const auto h=next();
                        unsigned v=0;
                        if(h>='0' && h<='9')v=static_cast<unsigned>(h-'0');
                        else if(h>='a' && h<='f')v=static_cast<unsigned>(h-'a'+10);
                        else if(h>='A' && h<='F')v=static_cast<unsigned>(h-'A'+10);
                        else invalid();
                        n=n*16+v;
                    }
                    r.push_back(static_cast<char16_t>(n));continue;
                }
                switch(e){
                    case '"': c='"';break;case '\\':c='\\';break;case '/':c='/';break;
                    case 'b':c=8;break;case 'f':c=12;break;case 'n':c=10;break;
                    case 'r':c=13;break;case 't':c=9;break;default:invalid();
                }
            }else if(c<32 || c>=127)invalid();
            r.push_back(static_cast<char16_t>(c));
            if(r.size()>256)invalid();
        }
        if(r.size()>256 || !well_formed_utf16(r))invalid();
        return r;
    }
    Value parse(unsigned depth){
        ws();if(at_==s_.size() || depth>4)invalid();
        const auto c=s_[at_];
        if(c=='{'){
            ++at_;Value v;
            if(take('}'))return v;
            do{
                auto key=string();
                if(!take(':') || v.object.size()>=64)invalid();
                auto value=parse(depth+1);
                if(!v.object.emplace(std::move(key),std::move(value)).second)invalid();
                if(take('}'))return v;
            }while(take(','));
            invalid();
        }
        if(c=='"')return Value::text(string());
        if(s_.substr(at_,4)=="true"){at_+=4;return Value::boolean(true);}
        if(s_.substr(at_,5)=="false"){at_+=5;return Value::boolean(false);}
        if(c<'0' || c>'9')invalid();
        unsigned n=0;
        do{
            const unsigned d=static_cast<unsigned>(s_[at_++]-'0');
            if(n>1000000)invalid();
            n=n*10+d;
            if(c=='0' && at_<s_.size() && s_[at_]>='0' && s_[at_]<='9')invalid();
        }while(at_<s_.size() && s_[at_]>='0' && s_[at_]<='9');
        return Value::integer(n);
    }
public:
    explicit Parser(std::string_view input):s_(input){if(input.size()>max_bytes)invalid();}
    Value root(){auto result=parse(0);ws();if(at_!=s_.size())invalid();return result;}
};
inline const Value& member(const Value& v,const char16_t* key,Value::Kind kind){
    if(v.kind!=Value::Kind::object)invalid();
    const auto it=v.object.find(key);
    if(it==v.object.end() || it->second.kind!=kind)invalid();
    return it->second;
}
inline std::string narrow(std::u16string_view s){
    std::string r;for(auto c:s){if(c>=127)invalid();r.push_back(static_cast<char>(c));}return r;
}
inline std::u16string widen(std::string_view s){
    std::u16string r;for(unsigned char c:s){if(c>=127)invalid();r.push_back(static_cast<char16_t>(c));}return r;
}
inline constexpr const char16_t* resolutions[]={u"720p",u"1080p",u"1440p",u"2160p"};
inline constexpr const char16_t* encoders[]={u"auto",u"software",u"hardware"};
inline constexpr const char16_t* retentions[]={u"finished",u"all_attempts",u"pb_only"};
inline constexpr const char16_t* keys[]={u"Unassigned",u"F5",u"F6",u"F7",u"F8",u"F9",u"F10",u"F11",u"F12"};
template<class E,std::size_t N> E choice(const Value& v,const char16_t* const (&names)[N]){
    for(std::size_t i=0;i<N;++i)if(v.str==names[i])return static_cast<E>(i);
    invalid();
}
inline Settings decode_settings(const Value& v,const PathPolicy& paths){
    if(v.kind!=Value::Kind::object || v.object.size()!=18)invalid();
    auto b=[&](const char16_t* k){return member(v,k,Value::Kind::boolean).flag;};
    auto n=[&](const char16_t* k){return member(v,k,Value::Kind::number).number;};
    auto t=[&](const char16_t* k)->const Value&{return member(v,k,Value::Kind::string);};
    Settings s;
    s.auto_record=b(u"auto_record");s.game_audio=b(u"game_audio");
    s.resolution=choice<Resolution>(t(u"resolution"),resolutions);
    const auto fps=t(u"fps").str;if(fps!=u"30" && fps!=u"60")invalid();s.fps=fps==u"30"?30u:60u;
    s.bitrate_mbps=n(u"bitrate_mbps");s.encoder=choice<Encoder>(t(u"encoder"),encoders);
    s.input_overlay=b(u"input_overlay");s.igt_overlay=b(u"igt_overlay");s.rta_overlay=b(u"rta_overlay");
    s.overlay_opacity=n(u"overlay_opacity");s.package_directory=t(u"package_directory").str;
    s.retention=choice<Retention>(t(u"retention"),retentions);s.min_free_gib=n(u"min_free_gib");
    s.auto_package=b(u"auto_package");s.remember_loadout=b(u"remember_loadout");
    s.restart_key=choice<Key>(t(u"restart_key"),keys);s.record_key=choice<Key>(t(u"record_key"),keys);
    s.overlay_key=choice<Key>(t(u"overlay_key"),keys);
    validate(s,paths);return s;
}
inline Value encode_settings(const Settings& s){
    Value v;auto& m=v.object;
    auto b=[&](const char16_t* k,bool x){m.emplace(k,Value::boolean(x));};
    auto n=[&](const char16_t* k,unsigned x){m.emplace(k,Value::integer(x));};
    auto t=[&](const char16_t* k,std::u16string x){m.emplace(k,Value::text(std::move(x)));};
    b(u"auto_record",s.auto_record);b(u"game_audio",s.game_audio);t(u"resolution",resolutions[static_cast<unsigned>(s.resolution)]);
    t(u"fps",s.fps==30?u"30":u"60");n(u"bitrate_mbps",s.bitrate_mbps);t(u"encoder",encoders[static_cast<unsigned>(s.encoder)]);
    b(u"input_overlay",s.input_overlay);b(u"igt_overlay",s.igt_overlay);b(u"rta_overlay",s.rta_overlay);n(u"overlay_opacity",s.overlay_opacity);
    t(u"package_directory",s.package_directory);t(u"retention",retentions[static_cast<unsigned>(s.retention)]);n(u"min_free_gib",s.min_free_gib);
    b(u"auto_package",s.auto_package);b(u"remember_loadout",s.remember_loadout);
    t(u"restart_key",keys[static_cast<unsigned>(s.restart_key)]);t(u"record_key",keys[static_cast<unsigned>(s.record_key)]);
    t(u"overlay_key",keys[static_cast<unsigned>(s.overlay_key)]);return v;
}
inline void quote(std::string& out,std::u16string_view s){
    constexpr char hex[]="0123456789abcdef";
    out.push_back('"');
    for(auto c:s){
        if(c==u'"' || c==u'\\'){out.push_back('\\');out.push_back(static_cast<char>(c));}
        else if(c<32 || c>=127){out+="\\u";for(int shift=12;shift>=0;shift-=4)out.push_back(hex[(static_cast<unsigned>(c)>>shift)&15u]);}
        else out.push_back(static_cast<char>(c));
    }
    out.push_back('"');
}
inline void emit(std::string& out,const Value& v){
    switch(v.kind){
        case Value::Kind::string:quote(out,v.str);break;
        case Value::Kind::boolean:out+=v.flag?"true":"false";break;
        case Value::Kind::number:out+=std::to_string(v.number);break;
        case Value::Kind::object:{
            out.push_back('{');bool first=true;
            for(const auto& item:v.object){if(!first)out.push_back(',');first=false;quote(out,item.first);out.push_back(':');emit(out,item.second);}
            out.push_back('}');break;
        }
    }
}
inline Book decode(std::string_view bytes,const PathPolicy& paths){
    const auto root=Parser(bytes).root();
    if(root.kind!=Value::Kind::object || root.object.size()!=3)invalid();
    Book book;book.version=member(root,u"schema",Value::Kind::number).number;
    book.active=narrow(member(root,u"active",Value::Kind::string).str);
    const auto& profiles=member(root,u"profiles",Value::Kind::object);
    for(const auto& p:profiles.object)book.profiles.push_back({narrow(p.first),decode_settings(p.second,paths)});
    validate(book,paths);return book;
}
inline std::string encode(const Book& book,const PathPolicy& paths){
    validate(book,paths);
    Value root,profiles;
    for(const auto& p:book.profiles)profiles.object.emplace(widen(p.name),encode_settings(p.values));
    root.object.emplace(u"schema",Value::integer(book.version));root.object.emplace(u"active",Value::text(widen(book.active)));
    root.object.emplace(u"profiles",std::move(profiles));std::string out;emit(out,root);
    if(out.size()>max_bytes)invalid();return out;
}
}
