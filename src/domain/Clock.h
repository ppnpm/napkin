#pragma once
#include "Types.h"
#include <functional>

namespace napkin {

// Injectable so lifecycle logic (age-based sectioning in §6, trash purge) is
// testable without sleeping. Production code never calls the setter.
Timestamp nowMs();
void setClockForTesting(std::function<Timestamp()> clock);
void resetClock();

}  // namespace napkin
