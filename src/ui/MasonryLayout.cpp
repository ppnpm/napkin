#include "MasonryLayout.h"
#include <QWidget>

namespace napkin {

MasonryLayout::MasonryLayout(QWidget* parent) : QLayout(parent) {}

MasonryLayout::~MasonryLayout()
{
    while (QLayoutItem* item = takeAt(0)) delete item;
}

void MasonryLayout::setColumnWidth(int min, int max)
{
    minColumn_ = min;
    maxColumn_ = max;
    invalidate();
}

void MasonryLayout::setSpacingBetween(int spacing)
{
    gap_ = spacing;
    invalidate();
}

void MasonryLayout::addItem(QLayoutItem* item) { items_.append(item); }

void MasonryLayout::insertWidgetAt(int index, QWidget* widget)
{
    addChildWidget(widget);
    items_.insert(std::clamp(index, 0, int(items_.size())), new QWidgetItem(widget));
    invalidate();
}
int MasonryLayout::count() const { return int(items_.size()); }

QLayoutItem* MasonryLayout::itemAt(int index) const
{
    return index >= 0 && index < items_.size() ? items_.at(index) : nullptr;
}

QLayoutItem* MasonryLayout::takeAt(int index)
{
    return index >= 0 && index < items_.size() ? items_.takeAt(index) : nullptr;
}

int MasonryLayout::columnCount(int width) const
{
    const QMargins m = contentsMargins();
    const int usable = width - m.left() - m.right();
    if (usable <= 0) return 1;
    // As many columns as fit at the minimum width, then widen them to share the
    // space — so a card is never narrower than minColumn_ and never wider than
    // maxColumn_ just because the window is large.
    int columns = (usable + gap_) / (minColumn_ + gap_);
    columns = std::max(1, columns);
    while (columns > 1 && (usable - (columns - 1) * gap_) / columns > maxColumn_)
        break;
    const int widest = (usable - (columns - 1) * gap_) / columns;
    if (widest > maxColumn_) columns = std::max(columns, (usable + gap_) / (maxColumn_ + gap_));
    return std::max(1, columns);
}

int MasonryLayout::columnWidth(int width) const
{
    const QMargins m = contentsMargins();
    const int usable = width - m.left() - m.right();
    const int columns = columnCount(width);
    return std::max(minColumn_ / 2, std::min(maxColumn_, (usable - (columns - 1) * gap_) / columns));
}

int MasonryLayout::layoutInto(const QRect& rect, bool apply) const
{
    const QMargins m = contentsMargins();
    const int columns = columnCount(rect.width());
    const int cw = columnWidth(rect.width());

    QList<int> bottoms;
    for (int i = 0; i < columns; ++i) bottoms.append(rect.top() + m.top());

    for (QLayoutItem* item : items_) {
        QWidget* w = item->widget();
        if (w && w->isHidden()) continue;

        int shortest = 0;
        for (int i = 1; i < columns; ++i)
            if (bottoms[i] < bottoms[shortest]) shortest = i;

        const int h = item->hasHeightForWidth() ? item->heightForWidth(cw)
                                                : item->sizeHint().height();
        const int x = rect.left() + m.left() + shortest * (cw + gap_);
        if (apply) item->setGeometry(QRect(x, bottoms[shortest], cw, h));
        bottoms[shortest] += h + gap_;
    }

    int tallest = rect.top() + m.top();
    for (int b : bottoms) tallest = std::max(tallest, b);
    return tallest - rect.top() - gap_ + m.bottom();
}

void MasonryLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    layoutInto(rect, true);
}

int MasonryLayout::heightForWidth(int width) const
{
    return layoutInto(QRect(0, 0, width, 0), false);
}

QSize MasonryLayout::sizeHint() const
{
    const QMargins m = contentsMargins();
    return {minColumn_ + m.left() + m.right(), heightForWidth(minColumn_)};
}

QSize MasonryLayout::minimumSize() const
{
    const QMargins m = contentsMargins();
    return {minColumn_ / 2 + m.left() + m.right(), 0};
}

}  // namespace napkin
