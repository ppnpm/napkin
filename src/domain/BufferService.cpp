#include "BufferService.h"
#include "Clock.h"
#include "../data/BufferRepository.h"
#include "../data/Database.h"
#include "../data/ItemRepository.h"

namespace napkin {

void Draft::setText(const QString& text)
{
    for (auto& i : items_) {
        if (i.type == ItemType::Text) { i.text = text; return; }
    }
    if (!text.trimmed().isEmpty()) items_.insert(items_.begin(), Item::makeText(text));
}

bool Draft::isEmpty() const
{
    for (const auto& i : items_)
        if (!i.isEmpty()) return false;
    return true;
}

BufferId BufferService::commitDraft(const Draft& draft)
{
    if (draft.isEmpty()) return kNoBuffer;  // invariant 5

    Transaction tx(db_);
    const BufferId id = buffers_.create();
    for (const auto& item : draft.items()) {
        if (item.isEmpty()) continue;
        items_.append(id, item);
    }
    tx.commit();
    return id;
}

ItemId BufferService::appendTo(BufferId id, Item item)
{
    Transaction tx(db_);
    const ItemId itemId = items_.append(id, std::move(item));
    buffers_.touch(id);
    tx.commit();
    return itemId;
}

void BufferService::updateTextItem(BufferId bufferId, ItemId itemId, const QString& text)
{
    Transaction tx(db_);
    items_.updateText(itemId, text);
    buffers_.touch(bufferId);
    tx.commit();
}

void BufferService::removeItem(BufferId bufferId, ItemId itemId)
{
    Transaction tx(db_);
    items_.remove(itemId);
    buffers_.touch(bufferId);
    tx.commit();
}

// Pin and Keep are metadata about the buffer, not edits to it, so neither
// bumps modified_at — flipping a pin must not reshuffle the list (SPEC.md §7).
void BufferService::setPinned(BufferId id, bool pinned) { buffers_.setPinned(id, pinned); }
void BufferService::setKept(BufferId id, bool kept)     { buffers_.setKept(id, kept); }

bool BufferService::trash(BufferId id)          { return buffers_.moveToTrash(id); }
void BufferService::trashConfirmed(BufferId id) { buffers_.moveToTrashConfirmed(id); }
void BufferService::restore(BufferId id)        { buffers_.restore(id); }

int BufferService::purgeExpiredTrash()
{
    return buffers_.purgeTrashOlderThan(nowMs() - kTrashRetentionDays * kMsPerDay);
}

Timestamp BufferService::olderThanCutoff()
{
    return nowMs() - kOlderThresholdDays * kMsPerDay;
}

}  // namespace napkin
