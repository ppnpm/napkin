#pragma once
#include "../domain/Item.h"
#include <QRect>
#include <QSize>
#include <vector>

class QFont;

namespace napkin {

// Geometry for the whole board, computed from the ITEMS rather than from
// widgets.
//
// This is what makes virtualization possible. A card's height depends only on
// its own content, so it can be measured without ever constructing the card:
// text against a shared QTextDocument, images from the width and height already
// stored in the row. The canvas then builds widgets for the visible slice only.
class BoardLayout {
public:
    struct Slot {
        ItemId id = kNoItem;
        QRect  rect;
        bool   clipped = false;   // content taller than a card may be
    };

    void setViewport(int width);
    void setFont(const QFont& body);
    void rebuild(const std::vector<Item>& items);

    int columnWidth() const { return columnWidth_; }
    int totalHeight() const { return totalHeight_; }

    const std::vector<Slot>& placements() const { return placements_; }
    std::vector<int> indicesIn(const QRect& visible, int overscan) const;
    int indexOf(ItemId id) const;

private:
    int heightFor(const Item& item, int columnWidth, bool* clipped) const;

    std::vector<Slot> placements_;
    int viewportWidth_ = 0;
    int columnWidth_ = 0;
    int totalHeight_ = 0;
    QFont* body_ = nullptr;
};

}  // namespace napkin
