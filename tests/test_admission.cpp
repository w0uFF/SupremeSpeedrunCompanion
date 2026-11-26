#include <initializer_list>
#include "src/engine_bridge/admission.hpp"
using namespace supreme::engine_bridge;
int main() {
    for (auto match : {ReferenceMatch::unknown, ReferenceMatch::original,
                       ReferenceMatch::display_original, ReferenceMatch::display_keyboard}) {
        const auto result = assess(match);
        if (result.capabilities != 0) return 1;
        if (result.reason != (match == ReferenceMatch::unknown ? AdmissionReason::unknown_reference
                                                             : AdmissionReason::loader_not_validated)) return 2;
    }
    return 0;
}
