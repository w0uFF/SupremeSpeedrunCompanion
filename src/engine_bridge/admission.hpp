#pragma once
#include "contracts/observation.hpp"
namespace supreme::engine_bridge {
// This is an offline policy boundary, not a loaded-module attestation.
enum class ReferenceMatch { unknown, original, display_original, display_keyboard };
enum class AdmissionReason { unknown_reference, loader_not_validated };
struct Admission {
    AdmissionReason reason;
    Capabilities capabilities;
};
constexpr Admission assess(ReferenceMatch match) noexcept {
    return {match == ReferenceMatch::unknown ? AdmissionReason::unknown_reference
                                             : AdmissionReason::loader_not_validated, 0};
}
}
