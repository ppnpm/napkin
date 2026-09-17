#pragma once
#include "../domain/Item.h"
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
    int countForBuffer(BufferId bufferId);

    void updateText(ItemId id, const QString& text);
    void remove(ItemId id);

    // SPEC.md §5: a blob is unlinked only when no item references it any more.
    // A query, not a refcount — so no drift is possible.
    bool blobIsReferenced(const QString& hash);
    std::vector<QString> allBlobHashes();

private:
    int nextPosition(BufferId bufferId);

    Database& db_;
};

}  // namespace napkin
