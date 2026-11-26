#pragma once
#include "contracts/observation.hpp"
namespace supreme::diagnostics { struct Status { Capabilities available; Origin origin; std::optional<std::string> track; std::optional<std::uint64_t> igt_ms; }; }
