#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace supreme::configuration {
inline constexpr unsigned schema=1;
enum class Resolution { p720, p1080, p1440, p2160 };
enum class Encoder { automatic, software, hardware };
enum class Retention { finished, all_attempts, pb_only };
enum class Key { unassigned, f5, f6, f7, f8, f9, f10, f11, f12 };
struct Settings {
    bool auto_record=false, game_audio=false;
    Resolution resolution=Resolution::p720;
    unsigned fps=30, bitrate_mbps=5;
    Encoder encoder=Encoder::automatic;
    bool input_overlay=false, igt_overlay=true, rta_overlay=true;
    unsigned overlay_opacity=100;
    std::u16string package_directory;
    Retention retention=Retention::finished;
    unsigned min_free_gib=5;
    bool auto_package=false, remember_loadout=true;
    Key restart_key=Key::unassigned, record_key=Key::unassigned, overlay_key=Key::unassigned;
};
// Native infrastructure validates paths without creating folders or touching game files.
// A nonempty path cannot be accepted until an adapter explicitly validates it.
class PathPolicy {
public:
    virtual ~PathPolicy()=default;
    virtual bool accepts(std::u16string_view path) const=0;
};
template<class E> bool in_range(E value,E last) noexcept {
    const auto n=static_cast<int>(value);
    return n>=0 && n<=static_cast<int>(last);
}
inline void validate(const Settings& s,const PathPolicy& paths) {
    if(!in_range(s.resolution,Resolution::p2160) || !in_range(s.encoder,Encoder::hardware) ||
       !in_range(s.retention,Retention::pb_only) || (s.fps!=30 && s.fps!=60) ||
       s.bitrate_mbps<3 || s.bitrate_mbps>40 || s.overlay_opacity<10 || s.overlay_opacity>100 ||
       s.min_free_gib<1 || s.min_free_gib>100 || !in_range(s.restart_key,Key::f12) ||
       !in_range(s.record_key,Key::f12) || !in_range(s.overlay_key,Key::f12))
        throw std::invalid_argument("unsupported settings value");
    const Key keys[]{s.restart_key,s.record_key,s.overlay_key};
    for(unsigned i=0;i<3;++i)for(unsigned j=i+1;j<3;++j)
        if(keys[i]!=Key::unassigned && keys[i]==keys[j])throw std::invalid_argument("duplicate hotkey");
    if(s.package_directory.size()>240 || (!s.package_directory.empty() && !paths.accepts(s.package_directory)))
        throw std::invalid_argument("package directory rejected");
}
inline std::string folded(std::string_view name) {
    if(name.empty() || name.size()>48 || name.back()==' ')throw std::invalid_argument("profile name");
    std::string result;
    for(char c:name){
        const bool alnum=(c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9');
        if(!alnum && (result.empty() || (c!=' ' && c!='_' && c!='-')))throw std::invalid_argument("profile name");
        result.push_back(c>='A' && c<='Z'?static_cast<char>(c-'A'+'a'):c);
    }
    return result;
}
struct Profile { std::string name; Settings values; };
struct Book { unsigned version=schema; std::string active; std::vector<Profile> profiles; };
class Store {
public:
    virtual ~Store()=default;
    virtual Book load() const=0;
    virtual void save(const Book&)=0;
};
inline const Profile& lookup(const Book& book,std::string_view name) {
    const auto it=std::find_if(book.profiles.begin(),book.profiles.end(),[name](const Profile& p){return p.name==name;});
    if(it==book.profiles.end())throw std::invalid_argument("missing profile");
    return *it;
}
inline void validate(const Book& book,const PathPolicy& paths) {
    if(book.version!=schema || book.profiles.empty() || book.profiles.size()>64)throw std::invalid_argument("profile schema/count");
    std::vector<std::string> names;
    for(const auto& p:book.profiles){
        auto key=folded(p.name);
        if(std::find(names.begin(),names.end(),key)!=names.end())throw std::invalid_argument("duplicate profile");
        names.push_back(std::move(key));validate(p.values,paths);
    }
    (void)lookup(book,book.active);
}
struct LauncherSelection {
    std::string profile; // blank selects stored active
    std::optional<Settings> whole_override; // null selects profile; no implicit per-field merge
};
enum class Source { profile, launcher, session };
struct Snapshot { std::string profile; Settings values; Source source; };
// One owner thread. Returns copies so a subsequent edit cannot mutate an active attempt.
class Controller {
    Store& store_;
    const PathPolicy& paths_;
public:
    Controller(Store& store,const PathPolicy& paths):store_(store),paths_(paths){}
    Snapshot resolve(const LauncherSelection& launch) const {
        const auto book=store_.load();validate(book,paths_);
        const auto& profile=lookup(book,launch.profile.empty()?book.active:launch.profile);
        if(launch.whole_override){validate(*launch.whole_override,paths_);return {profile.name,*launch.whole_override,Source::launcher};}
        return {profile.name,profile.values,Source::profile};
    }
    Snapshot session_edit(const Snapshot& previous,const Settings& values) const {
        validate(values,paths_);return {previous.profile,values,Source::session};
    }
    void replace_profile(std::string_view name,const Settings& values) {
        auto book=store_.load();validate(book,paths_);validate(values,paths_);
        auto it=std::find_if(book.profiles.begin(),book.profiles.end(),[name](const Profile& p){return p.name==name;});
        if(it==book.profiles.end())throw std::invalid_argument("missing profile");
        it->values=values;store_.save(book);
    }
};
}
