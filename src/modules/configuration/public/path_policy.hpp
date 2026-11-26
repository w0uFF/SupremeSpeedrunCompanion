#pragma once
#include "src/modules/configuration/public/settings.hpp"
namespace supreme::configuration {
inline bool well_formed_utf16(std::u16string_view s) noexcept {
    for(std::size_t i=0;i<s.size();++i){
        const auto c=s[i];
        if(c>=0xd800 && c<=0xdbff){
            if(++i==s.size() || s[i]<0xdc00 || s[i]>0xdfff)return false;
        }else if(c>=0xdc00 && c<=0xdfff)return false;
    }
    return true;
}
inline bool reserved_component(std::u16string_view part) {
    auto base=part.substr(0,part.find(u'.'));
    while(!base.empty() && base.back()==u' ')base.remove_suffix(1);
    std::u16string upper(base);
    for(auto& c:upper)if(c>=u'a' && c<=u'z')c=static_cast<char16_t>(c-u'a'+u'A');
    if(upper==u"CON" || upper==u"PRN" || upper==u"AUX" || upper==u"NUL" ||
       upper==u"CONIN$" || upper==u"CONOUT$")return true;
    return upper.size()==4 && (upper.substr(0,3)==u"COM" || upper.substr(0,3)==u"LPT") &&
        ((upper[3]>=u'1' && upper[3]<=u'9') || upper[3]==u'\u00b9' || upper[3]==u'\u00b2' || upper[3]==u'\u00b3');
}
// Lexical, not filesystem validation. No drive roots, relative/UNC/device paths or ADS.
inline std::u16string canonical_directory(std::u16string_view path) {
    if(path.empty())return {};
    if(path.size()>240 || !well_formed_utf16(path) || path.size()<4 ||
       !((path[0]>=u'A' && path[0]<=u'Z') || (path[0]>=u'a' && path[0]<=u'z')) ||
       path[1]!=u':' || (path[2]!=u'\\' && path[2]!=u'/'))throw std::invalid_argument("directory form");
    std::u16string result(path);
    for(auto& c:result)if(c==u'/')c=u'\\';
    std::size_t from=3;
    while(from<=result.size()){
        const auto at=result.find(u'\\',from);
        const auto end=at==std::u16string::npos?result.size():at;
        const auto part=std::u16string_view(result).substr(from,end-from);
        if(part.empty() || part==u"." || part==u".." || part.back()==u' ' || part.back()==u'.' || reserved_component(part))
            throw std::invalid_argument("directory component");
        for(auto c:part)if(c<32 || c==127 || std::u16string_view(u"<>:\"|?*").find(c)!=std::u16string_view::npos)
            throw std::invalid_argument("directory character");
        if(at==std::u16string::npos)break;
        from=at+1;
    }
    return result;
}
class LexicalPathPolicy final:public PathPolicy {
public:
    bool accepts(std::u16string_view path) const override {
        try{return canonical_directory(path)==path;}catch(const std::invalid_argument&){return false;}
    }
};
}
