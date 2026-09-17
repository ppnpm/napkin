#pragma once
#include "Types.h"
#include <QString>

namespace napkin {

// One captured piece of content. Exactly two types exist (SPEC.md §3); a URL is
// a text item that happens to render as a link chip, not a stored type.
struct Item {
    ItemId    id       = kNoItem;
    BufferId  bufferId = kNoBuffer;
    int       position = 0;
    ItemType  type     = ItemType::Text;
    Timestamp createdAt = 0;

    QString text;        // type == Text
    QString blobHash;    // type == Image, sha256 hex
    QString sourceName;  // original filename if imported; empty if pasted
    int     width  = 0;
    int     height = 0;
    qint64  byteSize = 0;

    bool isEmpty() const
    {
        return type == ItemType::Text ? text.trimmed().isEmpty() : blobHash.isEmpty();
    }

    static Item makeText(QString content)
    {
        Item i;
        i.type = ItemType::Text;
        i.text = std::move(content);
        return i;
    }

    static Item makeImage(QString hash, int w, int h, qint64 bytes, QString source = {})
    {
        Item i;
        i.type       = ItemType::Image;
        i.blobHash   = std::move(hash);
        i.width      = w;
        i.height     = h;
        i.byteSize   = bytes;
        i.sourceName = std::move(source);
        return i;
    }
};

}  // namespace napkin
