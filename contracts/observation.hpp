#pragma once
#include <cstdint>
#include <optional>
#include <string>
namespace supreme {
// In-process C++ contract only; never passed across a game ABI.
inline constexpr unsigned schema_version=1;
enum class Capability : unsigned { lifecycle=1, identity=2, igt=4, mapped_input=8, physics=16, replay=32 };
using Capabilities=unsigned;
constexpr Capabilities bit(Capability c) { return static_cast<unsigned>(c); }
enum class Origin { unavailable, synthetic, native };
struct ClockSample { std::uint64_t ticks; std::uint64_t frequency_hz; };
struct Observation { unsigned schema=schema_version; std::uint64_t sequence=0; Origin origin=Origin::unavailable; std::optional<std::string> track_id; std::optional<std::uint64_t> igt_ms; };
class IClock { public: virtual ~IClock()=default; virtual ClockSample sample() const=0; };
class IObservationSource { public: virtual ~IObservationSource()=default; virtual Capabilities capabilities() const noexcept=0; virtual Observation sample() const=0; };
}
