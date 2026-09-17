#pragma once
#include <QString>
#include <vector>

namespace napkin {

class BlobStore;
class ItemRepository;

// Reconciliation, run at startup and after anything that hard-deletes rows.
// The write ordering (invariants 6 and 7) deliberately biases toward orphan
// blobs, so something has to collect them; and a row whose blob has gone
// missing must be reported rather than silently rendered blank (SPEC.md §8).
struct GcResult {
    int orphansRemoved = 0;   // files on disk that nothing references
    int thumbnailsRemoved = 0;   // renderings of images that no longer exist
    int temporariesRemoved = 0;   // .tmp files from an interrupted write
    std::vector<QString> missingBlobs;  // referenced hashes with no file
};

GcResult reconcileBlobs(ItemRepository& items, BlobStore& blobs,
                        const QString& thumbnailDir = {});

}  // namespace napkin
