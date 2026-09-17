#pragma once
#include "Item.h"
#include <QString>
#include <vector>

namespace napkin {

// Derived display, never user-entered metadata (SPEC.md §1, §3). Napkin has no
// title field; a card's label is computed from what the buffer already holds.
struct BufferPreview {
    QString primary;     // the line the card leads with
    QString secondary;   // quiet detail line; may be empty
    int     itemCount = 0;
    bool    hasImage  = false;
    QString thumbHash;   // first image item in the buffer, if any
    QString thumbMime;
    bool    thumbAnimated = false;

    bool isEmpty() const { return primary.isEmpty() && secondary.isEmpty(); }
};

// `head` is the first few items of the buffer in position order; `totalCount`
// is how many it actually has. Only the head is loaded so a list of 5000
// buffers never reads every item (§12).
BufferPreview derivePreview(const std::vector<Item>& head, int totalCount);

// How many items the preview needs: enough to find an image a line or two down.
inline constexpr int kPreviewHeadSize = 4;

// First non-blank line of a block of text, whitespace-trimmed.
QString firstLine(const QString& text);

// Human-readable byte size: "4.2 MB".
QString formatBytes(qint64 bytes);

}  // namespace napkin
