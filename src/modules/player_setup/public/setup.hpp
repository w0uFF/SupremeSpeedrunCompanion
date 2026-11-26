#pragma once
#include <optional>
#include <string>
namespace supreme::player_setup {
struct Loadout { std::string rider, board; };
class Availability { public: virtual ~Availability()=default; virtual bool allows(const Loadout&) const=0; };
inline std::optional<Loadout> resolve(bool remember, const std::optional<Loadout>& saved, const Availability& available) {
 if(remember && saved && !saved->rider.empty() && !saved->board.empty() && available.allows(*saved)) return saved;
 return std::nullopt;
}
enum class RecordingPolicy { automatic, ask, training };
enum class CaptureIntent { arm_before_selection, await_choice, off };
inline CaptureIntent capture_intent(RecordingPolicy policy, std::optional<bool> choice={}) noexcept {
 switch(policy) { case RecordingPolicy::automatic:return CaptureIntent::arm_before_selection; case RecordingPolicy::ask:return !choice?CaptureIntent::await_choice:(*choice?CaptureIntent::arm_before_selection:CaptureIntent::off); case RecordingPolicy::training:return CaptureIntent::off; } return CaptureIntent::off;
}
// A request to arm is not evidence that acquisition actually started.
enum class Proof { absent, incomplete, observed_menu_and_result };
inline Proof proof(bool acquired_before_selection, bool menu_observed, bool result_observed, bool continuous) noexcept {
 if(!acquired_before_selection) return Proof::absent;
 return menu_observed&&result_observed&&continuous?Proof::observed_menu_and_result:Proof::incomplete;
}
}
