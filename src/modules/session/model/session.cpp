#include "src/modules/session/public/session.hpp"
#include <stdexcept>
namespace supreme::session {
Session::Session(std::uint64_t id,const categories::Category& c,std::array<Track,9> plan):id_(id),format_(c.format),individual_(c.track),plan_(plan) {
 const auto* canonical=categories::find(c.id);
 if(!id || !canonical || canonical->format!=c.format || canonical->track!=c.track || (format_==Format::all_tracks&&!categories::valid_plan(plan))) throw std::invalid_argument("invalid session plan");
 attempts_.reserve(max_attempts);
}
std::optional<Track> Session::expected() const noexcept {
 if(state_==State::completed||state_==State::abandoned) return {};
 if(format_==Format::individual) return individual_;
 if(format_==Format::all_tracks&&completed_<plan_.size()) return plan_[completed_];
 return {};
}
Outcome Session::consume(const Event& e) {
 if(e.session_id!=id_) return Outcome::wrong_identity;
 if(!e.sequence||e.sequence<=last_sequence_) return Outcome::duplicate_or_stale;
 if(e.monotonic_ms<last_ms_||state_==State::completed||state_==State::abandoned) return Outcome::invalid_event;
 const bool active=state_==State::racing;
 if(e.kind!=EventKind::start && e.kind!=EventKind::abandon && e.kind!=EventKind::championship_victory && (!active||attempts_.back().id!=e.attempt_id)) return Outcome::wrong_identity;
 switch(e.kind) {
 case EventKind::start:
  if(active||!e.track||!categories::valid(*e.track)||!e.attempt_id||e.attempt_id<=last_attempt_id_||e.igt_ms) return Outcome::invalid_event;
  if(expected()&&expected()!=e.track) return Outcome::wrong_identity;
  if(attempts_.size()==max_attempts) return Outcome::limit_reached;
  attempts_.push_back({e.attempt_id,e.monotonic_ms,*e.track,AttemptState::racing,{},{},0});
  last_attempt_id_=e.attempt_id; if(!start_ms_) start_ms_=e.monotonic_ms; state_=State::racing; break;
 case EventKind::reset_to_track:
  if(attempts_.back().resets==1000000) return Outcome::limit_reached;
  ++attempts_.back().resets; break;
 case EventKind::restart_race:
  attempts_.back().state=AttemptState::restarted; attempts_.back().end_ms=e.monotonic_ms;
  retry_=true; state_=format_==Format::individual?State::abandoned:State::awaiting_race;
  if(state_==State::abandoned) end_ms_=e.monotonic_ms;
  break;
 case EventKind::finish:
  if(!e.track||*e.track!=attempts_.back().track||!e.igt_ms) return Outcome::invalid_event;
  attempts_.back().state=AttemptState::finished; attempts_.back().end_ms=e.monotonic_ms; attempts_.back().igt_ms=e.igt_ms;
  ++completed_; state_=State::awaiting_race;
  if(format_==Format::individual||(format_==Format::all_tracks&&completed_==9)){state_=State::completed;end_ms_=e.monotonic_ms;}
  break;
 case EventKind::championship_victory:
  if(format_!=Format::championship||active||!completed_||attempts_.back().state!=AttemptState::finished||e.attempt_id!=0) return Outcome::invalid_event;
  state_=State::completed; end_ms_=e.monotonic_ms; break;
 case EventKind::abandon:
  if((active&&e.attempt_id!=attempts_.back().id)||(!active&&e.attempt_id!=0)) return Outcome::wrong_identity;
  if(active){attempts_.back().state=AttemptState::abandoned;attempts_.back().end_ms=e.monotonic_ms;}
  state_=State::abandoned; end_ms_=e.monotonic_ms; break;
 default:return Outcome::invalid_event;
 }
 last_sequence_=e.sequence;last_ms_=e.monotonic_ms;return Outcome::accepted;
}
Snapshot Session::snapshot() const {
 std::optional<std::uint64_t> elapsed;
 if(state_==State::completed&&start_ms_&&end_ms_) elapsed=*end_ms_-*start_ms_;
 return {state_,completed_,retry_,expected(),elapsed};
}
}
