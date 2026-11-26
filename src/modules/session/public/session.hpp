#pragma once
#include "src/modules/categories/public/catalog.hpp"
#include <cstdint>
#include <vector>
namespace supreme::session {
using categories::Track; using categories::Format;
enum class State { awaiting_race, racing, completed, abandoned };
enum class AttemptState { racing, finished, restarted, abandoned };
enum class EventKind { start, reset_to_track, finish, restart_race, abandon, championship_victory };
enum class Outcome { accepted, duplicate_or_stale, invalid_event, wrong_identity, limit_reached };
struct Event { std::uint64_t session_id, sequence, attempt_id, monotonic_ms; EventKind kind; std::optional<Track> track; std::optional<std::uint64_t> igt_ms; };
struct Attempt { std::uint64_t id, started_ms; Track track; AttemptState state; std::optional<std::uint64_t> end_ms, igt_ms; unsigned resets=0; };
struct Snapshot { State state; std::size_t completed_tracks; bool retry_observed; std::optional<Track> next_track; std::optional<std::uint64_t> session_elapsed_ms; };
// Offline model. Events must come from a synthetic, ordered owner-thread producer.
// A native adapter must separately validate identities, clocks and transitions.
class Session {
 public:
 static constexpr std::size_t max_attempts=128;
 Session(std::uint64_t id, const categories::Category& category, std::array<Track,9> plan);
 Outcome consume(const Event& e);
 Snapshot snapshot() const;
 const std::vector<Attempt>& attempts() const noexcept { return attempts_; }
 private:
 std::uint64_t id_, last_sequence_=0, last_ms_=0, last_attempt_id_=0;
 Format format_; std::optional<Track> individual_; std::array<Track,9> plan_;
 State state_=State::awaiting_race; std::size_t completed_=0; bool retry_=false;
 std::optional<std::uint64_t> start_ms_, end_ms_;
 std::vector<Attempt> attempts_;
 std::optional<Track> expected() const noexcept;
};
}
