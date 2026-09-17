#pragma once
#include "Buffer.h"
#include "Item.h"
#include <vector>

namespace napkin {

class Database;
class BufferRepository;
class ItemRepository;

// Lifecycle constants from SPEC.md §6. Napkin never auto-deletes a live buffer;
// age only moves it into the collapsed OLDER section.
inline constexpr int kOlderThresholdDays  = 30;
inline constexpr int kTrashRetentionDays  = 30;
inline constexpr qint64 kMsPerDay = 24LL * 60 * 60 * 1000;

// An unpersisted buffer. Ctrl+N produces one of these and nothing else:
// invariant 5 says no row exists until there is content, which is what makes
// "abandoned empty buffers" a non-problem rather than a cleanup rule.
class Draft {
public:
    void add(Item item) { items_.push_back(std::move(item)); }
    void setText(const QString& text);

    bool isEmpty() const;
    const std::vector<Item>& items() const { return items_; }
    void clear() { items_.clear(); }

private:
    std::vector<Item> items_;
};

class BufferService {
public:
    BufferService(Database& db, BufferRepository& buffers, ItemRepository& items)
        : db_(db), buffers_(buffers), items_(items) {}

    // Returns kNoBuffer and writes nothing when the draft has no content.
    BufferId commitDraft(const Draft& draft);

    ItemId appendTo(BufferId id, Item item);
    void updateTextItem(BufferId bufferId, ItemId itemId, const QString& text);
    void removeItem(BufferId bufferId, ItemId itemId);

    void setPinned(BufferId id, bool pinned);
    void setKept(BufferId id, bool kept);

    // false => the buffer is kept and the caller must confirm first.
    bool trash(BufferId id);
    void trashConfirmed(BufferId id);
    void restore(BufferId id);

    // The only automatic hard delete in Napkin. Touches nothing the user has
    // not already deleted.
    int purgeExpiredTrash();

    // Buffers older than this belong in the collapsed OLDER section.
    static Timestamp olderThanCutoff();

private:
    Database&         db_;
    BufferRepository& buffers_;
    ItemRepository&   items_;
};

}  // namespace napkin
