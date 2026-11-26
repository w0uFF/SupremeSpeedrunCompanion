#pragma once
#include "../model/status.hpp"
namespace supreme::diagnostics {
class Query { const IObservationSource& source_; public: explicit Query(const IObservationSource& s):source_(s){} Status execute() const { auto o=source_.sample(); auto c=source_.capabilities(); if(o.schema!=schema_version || o.origin==Origin::unavailable) return {0,Origin::unavailable,{},{}}; return {c,o.origin,(c&bit(Capability::identity))?o.track_id:std::nullopt,(c&bit(Capability::igt))?o.igt_ms:std::nullopt}; } };
}
