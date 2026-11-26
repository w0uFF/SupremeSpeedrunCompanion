#pragma once
#include "contracts/observation.hpp"
namespace supreme {
class UnavailableEngine final : public IObservationSource {
public: Capabilities capabilities() const noexcept override { return 0; }
Observation sample() const override { return {}; }
};
}
