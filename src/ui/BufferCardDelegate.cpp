#include "BufferCardDelegate.h"
#include "BufferListModel.h"
#include "Icons.h"
#include "../domain/Clock.h"
#include "../domain/TimeFormat.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace napkin {
namespace {

// Text that is present but secondary. Not placeholderText: on several themes
// that is faint enough to fail contrast for content the user needs to read.
QColor dimmed(const QPalette& pal, int alpha = 140)
{
    QColor c = pal.color(QPalette::Text);
    c.setAlpha(alpha);
    return c;
}

}  // namespace

BufferCardDelegate::BufferCardDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QFont BufferCardDelegate::timestampFont(const QFont& base) const
{
    QFont f = base;
    f.setPointSizeF(std::max(7.5, base.pointSizeF() - 1.5));
    return f;
}

int BufferCardDelegate::collapsedHeight() const
{
    const QFontMetrics fm(QApplication::font());
    const QFontMetrics tfm(timestampFont(QApplication::font()));
    // padding + primary + gap + secondary + gap + timestamp + padding
    return kPadding + fm.height() + 4 + fm.height() + 8 + tfm.height() + kPadding;
}

int BufferCardDelegate::sectionHeight(const QModelIndex& index) const
{
    return index.data(BufferListModel::SectionFirstRole).toBool() ? kSectionH : 0;
}

void BufferCardDelegate::setExpandedHeight(int h) { expandedHeight_ = std::clamp(h, 180, 520); }

QSize BufferCardDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // O(1): no text layout here. With 5000 rows this runs constantly.
    const bool expanded = index.data(BufferListModel::IsExpandedRole).toBool();
    const int body = expanded ? expandedHeight_ : collapsedHeight();
    return {option.rect.width(), sectionHeight(index) + body + kMarginY * 2};
}

QRect BufferCardDelegate::cardRect(const QRect& itemRect, const QModelIndex& index) const
{
    return itemRect.adjusted(kMarginX, sectionHeight(index) + kMarginY, -kMarginX, -kMarginY);
}

QRect BufferCardDelegate::contentRect(const QRect& itemRect, const QModelIndex& index) const
{
    return cardRect(itemRect, index).adjusted(kPadding, kPadding, -kPadding, -kPadding);
}

void BufferCardDelegate::paint(QPainter* p, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    const QPalette& pal = option.palette;
    const bool expanded = index.data(BufferListModel::IsExpandedRole).toBool();
    const bool isDraft  = index.data(BufferListModel::IsDraftRole).toBool();
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered  = option.state & QStyle::State_MouseOver;

    // --- section label --------------------------------------------------------
    if (index.data(BufferListModel::SectionFirstRole).toBool()) {
        QFont f = option.font;
        f.setPointSizeF(std::max(7.5, option.font.pointSizeF() - 1.0));
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        p->setFont(f);
        p->setPen(dimmed(pal, 120));
        const QRect labelRect(option.rect.left() + kMarginX, option.rect.top() + 12,
                              option.rect.width() - kMarginX * 2, kSectionH - 14);
        p->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter,
                    index.data(BufferListModel::SectionNameRole).toString());
    }

    // --- card ------------------------------------------------------------------
    const QRect card = cardRect(option.rect, index);
    QPainterPath path;
    path.addRoundedRect(QRectF(card), kRadius, kRadius);

    QColor fill = pal.color(QPalette::Base);
    if (hovered && !expanded) fill = fill.lighter(pal.color(QPalette::Window).lightness() > 128 ? 98 : 112);
    p->fillPath(path, fill);

    QColor border = pal.color(QPalette::Text);
    border.setAlpha(selected || expanded ? 90 : 38);
    p->setPen(QPen(border, 1));
    p->drawPath(path);

    // Focus is never signalled by colour alone: the selected card also carries a
    // solid accent rule down its leading edge (SPEC.md §14).
    if (selected || expanded) {
        QPainterPath clip;
        clip.addRoundedRect(QRectF(card), kRadius, kRadius);
        p->save();
        p->setClipPath(clip);
        p->fillRect(QRect(card.left(), card.top(), 3, card.height()),
                    pal.color(QPalette::Highlight));
        p->restore();
    }

    // The inline editor covers the content area while the row is open.
    if (expanded) { p->restore(); return; }

    // --- pin / keep indicators -------------------------------------------------
    // Never colour alone: each glyph is a distinct shape, and the model exposes
    // an accessible label alongside it (SPEC.md §14).
    const bool pinned = index.data(BufferListModel::PinnedRole).toBool();
    const bool kept   = index.data(BufferListModel::KeptRole).toBool();
    int glyphRight = 0;
    if (pinned || kept) {
        const int g = icons::kGlyphSize;
        int x = card.right() - kPadding - g;
        if (kept) {
            icons::drawKeep(p, QRect(x, card.top() + kPadding, g, g), dimmed(pal, 190));
            x -= g + 6;
        }
        if (pinned)
            icons::drawPin(p, QRect(x, card.top() + kPadding, g, g), dimmed(pal, 190));
        glyphRight = card.right() - kPadding - x + g;
    }

    // --- content ---------------------------------------------------------------
    const QRect content = contentRect(option.rect, index).adjusted(0, 0, -glyphRight, 0);
    const QFontMetrics fm(option.font);
    const QFontMetrics tfm(timestampFont(option.font));

    const QString primary   = index.data(BufferListModel::PrimaryRole).toString();
    const QString secondary = index.data(BufferListModel::SecondaryRole).toString();

    int y = content.top();
    p->setFont(option.font);
    if (primary.isEmpty()) {
        p->setPen(dimmed(pal, 110));
        p->drawText(QRect(content.left(), y, content.width(), fm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    isDraft ? QObject::tr("Type or paste something…") : QObject::tr("Empty"));
    } else {
        p->setPen(pal.color(QPalette::Text));
        p->drawText(QRect(content.left(), y, content.width(), fm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    fm.elidedText(primary, Qt::ElideRight, content.width()));
    }
    y += fm.height() + 4;

    if (!secondary.isEmpty()) {
        p->setPen(dimmed(pal));
        p->drawText(QRect(content.left(), y, content.width(), fm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    fm.elidedText(secondary, Qt::ElideRight, content.width()));
    }

    // --- timestamp -------------------------------------------------------------
    if (!isDraft) {
        const auto modified = index.data(BufferListModel::ModifiedAtRole).value<Timestamp>();
        p->setFont(timestampFont(option.font));
        p->setPen(dimmed(pal, 115));
        p->drawText(QRect(content.left(), content.bottom() - tfm.height(),
                          content.width(), tfm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter, relativeTime(modified, nowMs()));
    }

    p->restore();
}

}  // namespace napkin
