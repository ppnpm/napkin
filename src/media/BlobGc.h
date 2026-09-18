#pragma once
#include <QSet>
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

// `protectedHashes` are blobs a live undo offer still depends on: their rows are
// already deleted, so the sweep would see them as orphans and reclaim the very
// files undo is about to restore. Relying on every caller to dismiss the offer
// first is not a guarantee — one forgotten call site destroys user data — so the
// protection travels with the sweep instead.
GcResult reconcileBlobs(ItemRepository& items, BlobStore& blobs,
                        const QString& thumbnailDir = {},
                        const QSet<QString>& protectedHashes = {});

}  // namespace napkin
