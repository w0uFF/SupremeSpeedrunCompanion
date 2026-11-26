#pragma once
#include "src/modules/configuration/public/profile_codec.hpp"
namespace supreme::configuration::launcher {
struct Json {
    enum class Kind { object, array, text, boolean, number, null_value };
    Kind kind=Kind::null_value;
    std::map<std::u16string,Json> object;
    std::vector<Json> array;
    std::u16string text;
    std::string number;
    bool boolean=false;
};
[[noreturn]] inline void invalid(){throw std::invalid_argument("invalid launcher state");}
// Bounded reader of PR66 JSON. Unrelated settings are parsed, never interpreted or retained.
class Parser {
    std::string_view input_;
    std::size_t pos_=0,nodes_=0;
    void ws(){while(pos_<input_.size() && std::string_view(" \t\r\n").find(input_[pos_])!=std::string_view::npos)++pos_;}
    char peek(){return pos_<input_.size()?input_[pos_]:'\0';}
    char get(){if(pos_==input_.size())invalid();return input_[pos_++];}
    bool take(char c){ws();if(peek()!=c)return false;++pos_;return true;}
    bool digit(){return peek()>='0' && peek()<='9';}
    void literal(std::string_view word){if(input_.substr(pos_,word.size())!=word)invalid();pos_+=word.size();}
    unsigned hex4(){
        unsigned n=0;
        for(unsigned i=0;i<4;++i){
            const char c=get();unsigned d=0;
            if(c>='0' && c<='9')d=static_cast<unsigned>(c-'0');
            else if(c>='a' && c<='f')d=static_cast<unsigned>(c-'a'+10);
            else if(c>='A' && c<='F')d=static_cast<unsigned>(c-'A'+10);
            else invalid();
            n=n*16+d;
        }
        return n;
    }
    static void append(std::u16string& out,unsigned cp){
        if(cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))invalid();
        if(cp<0x10000)out.push_back(static_cast<char16_t>(cp));
        else {cp-=0x10000;out.push_back(static_cast<char16_t>(0xd800+(cp>>10)));out.push_back(static_cast<char16_t>(0xdc00+(cp&1023)));}
    }
    unsigned utf8(unsigned c){
        unsigned count=0,min=0,cp=0;
        if(c>=0xc2 && c<=0xdf){count=1;min=0x80;cp=c&31;}
        else if(c>=0xe0 && c<=0xef){count=2;min=0x800;cp=c&15;}
        else if(c>=0xf0 && c<=0xf4){count=3;min=0x10000;cp=c&7;}
        else invalid();
        for(unsigned i=0;i<count;++i){
            const auto d=static_cast<unsigned char>(get());if((d&0xc0)!=0x80)invalid();cp=(cp<<6)|(d&63);
        }
        if(cp<min)invalid();
        return cp;
    }
    std::u16string string(){
        if(!take('"'))invalid();
        std::u16string out;
        for(;;){
            const auto c=static_cast<unsigned char>(get());
            if(c=='"')break;
            if(c<32)invalid();
            if(c=='\\'){
                switch(get()){
                    case '"':out.push_back(u'"');break;case '\\':out.push_back(u'\\');break;
                    case '/':out.push_back(u'/');break;case 'b':out.push_back(u'\b');break;
                    case 'f':out.push_back(u'\f');break;case 'n':out.push_back(u'\n');break;
                    case 'r':out.push_back(u'\r');break;case 't':out.push_back(u'\t');break;
                    case 'u':{
                        auto cp=hex4();
                        if(cp>=0xd800 && cp<=0xdbff){
                            if(get()!='\\' || get()!='u')invalid();
                            const auto low=hex4();if(low<0xdc00 || low>0xdfff)invalid();
                            cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);
                        }
                        append(out,cp);break;
                    }
                    default:invalid();
                }
            }else append(out,c<128?c:utf8(c));
            if(out.size()>4096)invalid();
        }
        return out;
    }
    std::string number(){
        const auto begin=pos_;
        if(peek()=='-')++pos_;
        if(peek()=='0'){++pos_;if(digit())invalid();}
        else {if(!digit())invalid();while(digit())++pos_;}
        if(peek()=='.'){++pos_;if(!digit())invalid();while(digit())++pos_;}
        if(peek()=='e' || peek()=='E'){++pos_;if(peek()=='+' || peek()=='-')++pos_;if(!digit())invalid();while(digit())++pos_;}
        if(pos_-begin>128)invalid();
        return std::string(input_.substr(begin,pos_-begin));
    }
    Json value(unsigned depth){
        ws();if(depth>12 || ++nodes_>16384 || pos_==input_.size())invalid();
        Json out;const char c=peek();
        if(c=='{'){
            ++pos_;out.kind=Json::Kind::object;
            if(take('}'))return out;
            do{
                auto key=string();if(!take(':') || out.object.size()>=1024)invalid();
                auto item=value(depth+1);if(!out.object.emplace(std::move(key),std::move(item)).second)invalid();
                if(take('}'))return out;
            }while(take(','));
            invalid();
        }
        if(c=='['){
            ++pos_;out.kind=Json::Kind::array;
            if(take(']'))return out;
            do{
                if(out.array.size()>=1024)invalid();
                out.array.push_back(value(depth+1));if(take(']'))return out;
            }while(take(','));
            invalid();
        }
        if(c=='"'){out.kind=Json::Kind::text;out.text=string();}
        else if(c=='t' || c=='f'){out.kind=Json::Kind::boolean;out.boolean=c=='t';literal(c=='t'?"true":"false");}
        else if(c=='n'){literal("null");}
        else {out.kind=Json::Kind::number;out.number=number();}
        return out;
    }
public:
    explicit Parser(std::string_view input):input_(input){
        if(input.size()>codec::max_bytes)invalid();
        if(input.substr(0,3)=="\xef\xbb\xbf")pos_=3;
    }
    Json root(){auto result=value(0);ws();if(pos_!=input_.size())invalid();return result;}
};
inline const Json* optional_member(const Json& v,std::u16string_view key,Json::Kind kind){
    if(v.kind!=Json::Kind::object)invalid();
    const auto it=v.object.find(std::u16string(key));
    if(it==v.object.end())return nullptr;
    if(it->second.kind!=kind)invalid();
    return &it->second;
}
inline bool contains(const Json* list,std::u16string_view name){
    if(!list)return false;
    bool found=false;
    for(const auto& item:list->array){if(item.kind!=Json::Kind::text)invalid();if(item.text==name)found=true;}
    return found;
}
// Decimal conversion without rounding, locale, or floating point. Slider values must be integral.
inline unsigned integral(std::string_view text){
    if(text.empty() || text.front()=='-')invalid();
    std::string digits;int fraction=0;bool dot=false;
    std::size_t i=0;
    for(;i<text.size() && text[i]!='e' && text[i]!='E';++i){
        if(text[i]=='.'){dot=true;continue;}
        digits.push_back(text[i]);if(dot)++fraction;
    }
    int exp=0;bool negative=false;
    if(i<text.size()){
        ++i;if(text[i]=='+' || text[i]=='-'){negative=text[i]=='-';++i;}
        for(;i<text.size();++i){if(exp>256)invalid();exp=exp*10+(text[i]-'0');}
        if(exp>256)invalid();if(negative)exp=-exp;
    }
    const auto first=digits.find_first_not_of('0');
    if(first==std::string::npos)return 0;
    digits.erase(0,first);
    const int shift=exp-fraction;
    if(shift<0){
        const auto remove=static_cast<std::size_t>(-shift);
        if(remove>=digits.size() || digits.find_first_not_of('0',digits.size()-remove)!=std::string::npos)invalid();
        digits.resize(digits.size()-remove);
    }else{
        if(shift>3)invalid();
        digits.append(static_cast<std::size_t>(shift),'0');
    }
    if(digits.size()>3)invalid();
    unsigned result=0;for(char c:digits)result=result*10+static_cast<unsigned>(c-'0');
    return result;
}
struct Selection {
    LauncherSelection preferences;
    bool diagnostic_requested=false;
};
inline Selection decode(std::string_view bytes,const PathPolicy& paths){
    const auto root=Parser(bytes).root();
    const auto* active=optional_member(root,u"active",Json::Kind::object);
    if(!active)invalid();
    const bool ordered=contains(optional_member(*active,u"order",Json::Kind::array),u"ssc");
    const bool enabled=contains(optional_member(*active,u"enabled",Json::Kind::array),u"ssc");
    const auto* config=optional_member(*active,u"configuration",Json::Kind::object);
    const auto* ssc=config?optional_member(*config,u"ssc",Json::Kind::object):nullptr;
    auto values=codec::encode_settings(Settings{});
    bool request=false,override=false;
    std::string profile;
    if(ssc)for(const auto& pair:ssc->object){
        const auto& k=pair.first;const auto& v=pair.second;
        if(k==u"load_diagnostic" || k==u"use_launcher_overrides"){
            if(v.kind!=Json::Kind::boolean)invalid();
            if(k==u"load_diagnostic")request=v.boolean;else override=v.boolean;
        }else if(k==u"profile_name"){
            if(v.kind!=Json::Kind::text)invalid();
            profile=codec::narrow(v.text);if(!profile.empty())(void)folded(profile);
        }else{
            const auto it=values.object.find(k);if(it==values.object.end())invalid();
            switch(it->second.kind){
                case codec::Value::Kind::boolean:if(v.kind!=Json::Kind::boolean)invalid();it->second=codec::Value::boolean(v.boolean);break;
                case codec::Value::Kind::number:if(v.kind!=Json::Kind::number)invalid();it->second=codec::Value::integer(integral(v.number));break;
                case codec::Value::Kind::string:if(v.kind!=Json::Kind::text)invalid();it->second=codec::Value::text(k==u"package_directory"?canonical_directory(v.text):v.text);break;
                default:invalid();
            }
        }
    }
    const auto checked=codec::decode_settings(values,paths);
    return {{profile,override?std::optional<Settings>(checked):std::nullopt},ordered && enabled && request};
}
}
