#pragma once
#include "../model/status.hpp"
namespace supreme::diagnostics { inline std::string render(const Status& s) { return std::string("Source: ")+(s.origin==Origin::synthetic?"synthetic":s.origin==Origin::native?"native (adapter supplied)":"unavailable")+"\nTrack: "+s.track.value_or("unknown")+"\nIGT ms: "+(s.igt_ms?std::to_string(*s.igt_ms):"unknown")+"\n"; } }
