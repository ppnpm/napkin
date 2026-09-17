#include "ItemRepository.h"
#include "Database.h"
#include "Statement.h"
#include "../domain/Clock.h"

namespace napkin {
namespace {

Item readItem(const Statement& s)
{
    Item i;
    i.id         = s.columnInt64(0);
    i.bufferId   = s.columnInt64(1);
    i.position   = s.columnInt(2);
    i.type       = itemTypeFromString(s.columnText(3));
    i.createdAt  = s.columnInt64(4);
    i.text       = s.columnText(5);
    i.blobHash   = s.columnText(6);
    i.sourceName = s.columnText(7);
    i.width      = s.columnInt(8);
    i.height     = s.columnInt(9);
    i.byteSize   = s.columnInt64(10);
    i.mime       = s.columnText(11);
    i.animated   = s.columnBool(12);
    return i;
}

constexpr const char* kSelect =
    "SELECT id, buffer_id, position, type, created_at, text, blob_hash,"
    "       source_name, width, height, byte_size FROM items";

}  // namespace

int ItemRepository::nextPosition(BufferId bufferId)
{
    Statement s(db_, "SELECT COALESCE(MAX(position), -1) + 1 FROM items WHERE buffer_id = ?");
    s.bind(1, bufferId);
    return s.step() ? s.columnInt(0) : 0;
}

ItemId ItemRepository::append(BufferId bufferId, Item item)
{
    item.bufferId  = bufferId;
    item.position  = nextPosition(bufferId);
    item.createdAt = nowMs();

    Statement s(db_,
        "INSERT INTO items(buffer_id, position, type, created_at, text, blob_hash,"
        "                  source_name, width, height, byte_size, mime, animated)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
    s.bind(1, item.bufferId).bind(2, item.position).bind(3, itemTypeName(item.type))
     .bind(4, item.createdAt);

    // The schema CHECK requires exactly one of text / blob_hash to be non-null.
    if (item.type == ItemType::Text) { s.bind(5, item.text); s.bindNull(6); }
    else                             { s.bindNull(5); s.bind(6, item.blobHash); }

    s.bind(7, item.sourceName).bind(8, item.width).bind(9, item.height).bind(10, item.byteSize);
    s.bind(11, item.mime).bind(12, item.animated);
    s.exec();
    return db_.lastInsertId();
}

std::optional<Item> ItemRepository::find(ItemId id)
{
    Statement s(db_, "SELECT id, buffer_id, position, type, created_at, text, blob_hash,"
                     " source_name, width, height, byte_size, mime, animated FROM items WHERE id = ?");
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return readItem(s);
}

std::vector<Item> ItemRepository::listForBuffer(BufferId bufferId)
{
    Statement s(db_, "SELECT id, buffer_id, position, type, created_at, text, blob_hash,"
                     " source_name, width, height, byte_size, mime, animated FROM items"
                     " WHERE buffer_id = ? ORDER BY position ASC, id ASC");
    s.bind(1, bufferId);
    std::vector<Item> out;
    while (s.step()) out.push_back(readItem(s));
    return out;
}

std::vector<Item> ItemRepository::previewHead(BufferId bufferId, int limit)
{
    Statement s(db_, "SELECT id, buffer_id, position, type, created_at, text, blob_hash,"
                     " source_name, width, height, byte_size, mime, animated FROM items"
                     " WHERE buffer_id = ? ORDER BY position ASC, id ASC LIMIT ?");
    s.bind(1, bufferId).bind(2, limit);
    std::vector<Item> out;
    while (s.step()) out.push_back(readItem(s));
    return out;
}

int ItemRepository::countForBuffer(BufferId bufferId)
{
    Statement s(db_, "SELECT COUNT(*) FROM items WHERE buffer_id = ?");
    s.bind(1, bufferId);
    return s.step() ? s.columnInt(0) : 0;
}

ItemRepository::Counts ItemRepository::countsForBuffer(BufferId bufferId)
{
    Statement s(db_, "SELECT COUNT(*), COALESCE(SUM(type = 'image'), 0)"
                     " FROM items WHERE buffer_id = ?");
    s.bind(1, bufferId);
    Counts c;
    if (s.step()) { c.total = s.columnInt(0); c.images = s.columnInt(1); }
    return c;
}

void ItemRepository::updateText(ItemId id, const QString& text)
{
    Statement s(db_, "UPDATE items SET text = ? WHERE id = ? AND type = 'text'");
    s.bind(1, text).bind(2, id);
    s.exec();
}

void ItemRepository::remove(ItemId id)
{
    Statement s(db_, "DELETE FROM items WHERE id = ?");
    s.bind(1, id);
    s.exec();
}

bool ItemRepository::blobIsReferenced(const QString& hash)
{
    Statement s(db_, "SELECT 1 FROM items WHERE blob_hash = ? LIMIT 1");
    s.bind(1, hash);
    return s.step();
}

std::vector<Item> ItemRepository::allImageItems()
{
    Statement s(db_, "SELECT id, buffer_id, position, type, created_at, text, blob_hash,"
                     " source_name, width, height, byte_size, mime, animated FROM items"
                     " WHERE type = 'image'");
    std::vector<Item> out;
    while (s.step()) out.push_back(readItem(s));
    return out;
}

std::vector<QString> ItemRepository::allBlobHashes()
{
    Statement s(db_, "SELECT DISTINCT blob_hash FROM items WHERE blob_hash IS NOT NULL");
    std::vector<QString> out;
    while (s.step()) out.push_back(s.columnText(0));
    return out;
}

}  // namespace napkin
