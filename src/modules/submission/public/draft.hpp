#pragma once
#include "src/modules/categories/public/catalog.hpp"
#include "src/modules/player_setup/public/setup.hpp"
#include <cstdint>
#include <optional>
#include <string>
namespace supreme::submission {
// This is an offline review model, not an API client or a credential store.
enum class TimingMethod { ingame, realtime };
enum class Decision { incomplete_run, unknown_category, missing_evidence, unresolved_rules, missing_time, missing_video, awaiting_confirmation, offline_only };
struct Draft { std::string category_id, video_url; bool completed=false; player_setup::Proof proof=player_setup::Proof::absent; std::optional<std::uint64_t> igt_ms, rta_ms; std::optional<TimingMethod> official_method; bool rules_confirmed=false; };
// A deliberately narrow preliminary HTTPS video-link check; no network or publication.
bool plausible_video_link(const std::string& url) noexcept;
Decision review(const Draft& draft, bool confirmed) noexcept;
}
