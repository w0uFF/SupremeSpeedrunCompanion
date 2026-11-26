#include "src/modules/submission/public/draft.hpp"
namespace supreme::submission {
bool plausible_video_link(const std::string& s) noexcept {
 if(s.size()>2048) return false;
 for(unsigned char c:s) if(c<=32||c>=127||c=='\\'||c=='#') return false;
 constexpr std::string_view prefixes[]={"https://www.youtube.com/watch?v=","https://youtube.com/watch?v=","https://youtu.be/","https://www.twitch.tv/videos/"};
 for(auto p:prefixes) if(s.compare(0,p.size(),p)==0 && s.size()>p.size()) {
  const auto rest=std::string_view(s).substr(p.size());
  // Single unambiguous video ID only; richer URL parsing belongs in the platform adapter.
  for(char c:rest) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return false;
  return true;
 }
 return false;
}
Decision review(const Draft& d,bool confirmed) noexcept {
 if(!d.completed) return Decision::incomplete_run;
 if(!categories::find(d.category_id)) return Decision::unknown_category;
 if(d.proof!=player_setup::Proof::observed_menu_and_result) return Decision::missing_evidence;
 if(!d.rules_confirmed||!d.official_method) return Decision::unresolved_rules;
 if((*d.official_method==TimingMethod::ingame&&(!d.igt_ms||!*d.igt_ms))||(*d.official_method==TimingMethod::realtime&&(!d.rta_ms||!*d.rta_ms))) return Decision::missing_time;
 if(*d.official_method!=TimingMethod::ingame&&*d.official_method!=TimingMethod::realtime) return Decision::unresolved_rules;
 if(!plausible_video_link(d.video_url)) return Decision::missing_video;
 return confirmed?Decision::offline_only:Decision::awaiting_confirmation;
}
}
