#pragma once
#include "Types.h"
#include <QString>

namespace napkin {

// Relative timestamps for the card footer (SPEC.md §7). Stored UTC, displayed
// local. `now` is passed in rather than read, so this is pure and testable.
QString relativeTime(Timestamp when, Timestamp now);

// How often the visible cards need repainting for the label to stay honest.
// One minute is enough: the shortest label that changes is "N minutes ago".
inline constexpr int kTimeRefreshMs = 60'000;

}  // namespace napkin
