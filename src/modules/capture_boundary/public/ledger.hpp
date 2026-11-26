#pragma once
#include <cstdint>
#include <optional>
#include <vector>
namespace supreme::capture_boundary {
// Synthetic observation ledger only. No acquisition, encoding or engine commands.
enum class Mode { automatic, manual };
enum class State { armed, acquiring, finalizing, finalized, failed };
enum class End { finished, restarted, abandoned };
struct Marker { std::uint64_t run_id, attempt_id, start_ms; std::optional<std::uint64_t> end_ms; std::optional<End> end; };
class Ledger {
 public:
 static constexpr std::size_t max_markers=128;
 Ledger(Mode mode, std::uint64_t capture_id):mode_(mode),id_(capture_id){markers_.reserve(max_markers);if(!capture_id)state_=State::failed;}
 bool acquired(std::uint64_t ms) noexcept {if(state_!=State::armed)return false;state_=State::acquiring;last_ms_=ms;return true;}
 bool selection_observed(std::uint64_t run,std::uint64_t ms) noexcept {
  if(!available(ms)||!run||active_||(mode_==Mode::automatic&&run_&&run_!=run))return false;
  for(const auto& m:markers_)if(m.run_id==run)return false;
  run_=run;selection_=true;last_ms_=ms;return true;
 }
 bool begin(std::uint64_t run,std::uint64_t attempt,std::uint64_t ms) {
  if(!available(ms)||!run||!attempt||active_||markers_.size()==max_markers||(mode_==Mode::automatic&&run_&&run_!=run))return false;
  for(const auto& m:markers_)if(m.run_id==run&&m.attempt_id==attempt)return false;
  if(run_!=run){run_=run;selection_=false;}
  markers_.push_back({run,attempt,ms,{},{}});active_=true;result_=false;last_ms_=ms;return true;
 }
 bool end(std::uint64_t run,std::uint64_t attempt,End reason,std::uint64_t ms) noexcept {
  if(!available(ms)||!active_||markers_.back().run_id!=run||markers_.back().attempt_id!=attempt)return false;
  if(reason!=End::finished&&reason!=End::restarted&&reason!=End::abandoned)return false;
  markers_.back().end_ms=ms;markers_.back().end=reason;active_=false;result_=false;last_ms_=ms;return true;
 }
 bool result_observed(std::uint64_t run,std::uint64_t attempt,std::uint64_t ms) noexcept {
  if(!available(ms)||active_||markers_.empty()||markers_.back().run_id!=run||markers_.back().attempt_id!=attempt||markers_.back().end!=End::finished)return false;
  result_=true;last_ms_=ms;return true;
 }
 // Manual ownership is preserved by ignoring automatic stop requests.
 // A final result is mandatory for normal automatic completion; abandoned/restarted
 // sessions may use the explicit incomplete close path.
 bool close(std::uint64_t ms,bool user_stop,bool incomplete=false) noexcept {
  if(!available(ms)||active_||(mode_==Mode::manual&&!user_stop))return false;
  if(mode_==Mode::automatic&&!incomplete&&!result_)return false;
  state_=State::finalizing;last_ms_=ms;return true;
 }
 bool finalized() noexcept {if(state_!=State::finalizing)return false;state_=State::finalized;return true;}
 void failure() noexcept {if(state_!=State::finalized)state_=State::failed;}
 std::uint64_t id() const noexcept{return id_;}
 State state() const noexcept{return state_;}
 bool observed_menu() const noexcept{return selection_;}
 const std::vector<Marker>& markers() const noexcept{return markers_;}
 private:
 bool available(std::uint64_t ms) const noexcept{return state_==State::acquiring&&ms>=last_ms_;}
 Mode mode_;std::uint64_t id_,run_=0,last_ms_=0;State state_=State::armed;bool active_=false,selection_=false,result_=false;std::vector<Marker> markers_;
};
}
