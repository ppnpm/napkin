#include "BoardLayout.h"
#include "Tokens.h"

#include <QFont>
#include <QFontMetrics>
#include <QTextDocument>

namespace napkin {
namespace {
using namespace tokens;

// One document reused for every measurement. Constructing a QTextDocument per
// card per layout pass was a measurable slice of the 44 ms sweep at 1000 items.
QTextDocument& scratch()
{
    static QTextDocument doc;
    static bool ready = false;
    if (!ready) { doc.setDocumentMargin(1); ready = true; }
    return doc;
}

}  // namespace

void BoardLayout::setViewport(int width) { viewportWidth_ = width; }

void BoardLayout::setFont(const QFont& body)
{
    delete body_;
    body_ = new QFont(body);
}

int BoardLayout::heightFor(const Item& item, int columnWidth, bool* clipped) const
{
    const MeasureKey key{item.id, item.modifiedAt, columnWidth};
    if (item.id != kNoItem) {
        const auto it = cache_.constFind(item.id);
        if (it != cache_.constEnd() && it->first == key) {
            if (clipped) *clipped = it->second.clipped;
            return it->second.height;
        }
    }

    const int inner = std::max(40, columnWidth - kCardPad * 2);
    const int chrome = kCardPad * 2 - 6 + kCardFooterH + kGapTight;

    int content = 0;
    if (item.type == ItemType::Text) {
        QTextDocument& doc = scratch();
        if (body_) doc.setDefaultFont(*body_);
        // Only as much text as can still change the answer. A card caps at
        // kCardMaxHeight, and this many characters overflows that at any column
        // width we allow — so laying out the rest of a pasted log would be work
        // whose result is already known.
        doc.setPlainText(item.text.left(kMeasureLimit));
        doc.setTextWidth(inner);
        content = int(std::ceil(doc.size().height())) + 2;
        if (item.text.size() > kMeasureLimit) content = kCardMaxHeight;   // certainly over
    } else {
        // From the stored dimensions: no file is opened and no image decoded
        // just to find out how tall a card is.
        const QFontMetrics fm(body_ ? *body_ : QFont());
        const int captionH = fm.height() + 6;
        if (item.width > 0 && item.height > 0) {
            const int drawn = item.height * std::min(inner, item.width)
                              / std::max(1, item.width);
            content = drawn + captionH;
        } else {
            content = 96 + captionH;
        }
    }
    const int natural = content + chrome;
    const Measurement measured{std::clamp(natural, kCardMinHeight, kCardMaxHeight),
                               natural > kCardMaxHeight};
    if (item.id != kNoItem) {
        // Bounded: a buffer nobody is looking at should not pin its measurements
        // for the life of the process.
        if (cache_.size() > 4000) cache_.clear();
        cache_.insert(item.id, {key, measured});
    }
    if (clipped) *clipped = measured.clipped;
    return measured.height;
}

void BoardLayout::rebuild(const std::vector<Item>& items)
{
    placements_.clear();
    const int usable = std::max(kCardMinWidth, viewportWidth_ - kPadX * 2);
    int columns = std::max(1, (usable + kCardGap) / (kCardMinWidth + kCardGap));
    columnWidth_ = std::clamp((usable - (columns - 1) * kCardGap) / columns,
                              kCardMinWidth, kCardMaxWidth);
    // Recompute the count against the settled width so the two agree; an
    // earlier version derived them independently and the column count flipped
    // on 20px of unrelated chrome.
    columns = std::max(1, (usable + kCardGap) / (columnWidth_ + kCardGap));
    columnWidth_ = std::clamp((usable - (columns - 1) * kCardGap) / columns,
                              kCardMinWidth, kCardMaxWidth);

    std::vector<int> bottoms(size_t(columns), kPadTop);
    placements_.reserve(items.size());
    for (const auto& item : items) {
        size_t shortest = 0;
        for (size_t i = 1; i < bottoms.size(); ++i)
            if (bottoms[i] < bottoms[shortest]) shortest = i;

        bool clipped = false;
        const int h = heightFor(item, columnWidth_, &clipped);
        const int x = kPadX + int(shortest) * (columnWidth_ + kCardGap);
        placements_.push_back({item.id, QRect(x, bottoms[shortest], columnWidth_, h), clipped});
        bottoms[shortest] += h + kCardGap;
    }

    totalHeight_ = kPadTop;
    for (int b : bottoms) totalHeight_ = std::max(totalHeight_, b);
    totalHeight_ += kPadTop - kCardGap;
}

std::vector<int> BoardLayout::indicesIn(const QRect& visible, int overscan) const
{
    const QRect band = visible.adjusted(0, -overscan, 0, overscan);
    std::vector<int> out;
    for (size_t i = 0; i < placements_.size(); ++i)
        if (placements_[i].rect.intersects(band)) out.push_back(int(i));
    return out;
}

int BoardLayout::indexOf(ItemId id) const
{
    for (size_t i = 0; i < placements_.size(); ++i)
        if (placements_[i].id == id) return int(i);
    return -1;
}

}  // namespace napkin
