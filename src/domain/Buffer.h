#pragma once
#include "Types.h"
#include <optional>

namespace napkin {

// Metadata only. Items are loaded separately so the list view never pays for
// content it does not draw (SPEC.md §12: windowed queries, never SELECT *).
struct Buffer {
    BufferId  id         = kNoBuffer;
    Timestamp createdAt  = 0;
    Timestamp modifiedAt = 0;
    bool      pinned     = false;
    bool      kept       = false;
    std::optional<Timestamp> deletedAt;  // set => in trash

    bool isPersisted() const { return id != kNoBuffer; }
    bool inTrash() const { return deletedAt.has_value(); }
};

}  // namespace napkin
