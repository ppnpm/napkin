#pragma once
#include "../domain/Item.h"
#include "../domain/Preview.h"
#include <optional>
#include <vector>

namespace napkin {

class Database;

class ItemRepository {
public:
    explicit ItemRepository(Database& db) : db_(db) {}

    // Appends at the end of the buffer. Assigns position and created_at.
    ItemId append(BufferId bufferId, Item item);

    std::optional<Item> find(ItemId id);
    std::vector<Item> listForBuffer(BufferId bufferId);

    // Only the first few items, for deriving a card preview. A list of 5000
    // buffers must never read every item to draw itself (SPEC.md §12).
    std::vector<Item> previewHead(BufferId bufferId, int limit = kPreviewHeadSize);
    int countForBuffer(BufferId bufferId);

    void updateText(ItemId id, const QString& text);
    void remove(ItemId id);

    // SPEC.md §5: a blob is unlinked only when no item references it any more.
    // A query, not a refcount — so no drift is possible.
    bool blobIsReferenced(const QString& hash);
    std::vector<QString> allBlobHashes();
    std::vector<Item> allImageItems();

private:
    int nextPosition(BufferId bufferId);

    Database& db_;
};

}  // namespace napkin
