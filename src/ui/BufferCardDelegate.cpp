#include "BufferCardDelegate.h"
#include <algorithm>
#include "BufferListModel.h"
#include "Icons.h"
#include "Tokens.h"
#include "../media/Thumbnailer.h"
#include "../domain/Preview.h"

#include <QRegularExpression>
#include "../domain/Clock.h"
#include "../domain/TimeFormat.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace napkin {

using namespace tokens;

namespace {

// These four used to be declared here, with their own copy of the reasoning,
// identical in value to the ones in Tokens.h. A second copy of a design system
// is a second thing to forget to change: the file that calls itself the single
// visual system was not the file this delegate was painting from.
constexpr int kBorderResting = kCardBorderLight;
constexpr int kBorderActive  = kCardActive;

QColor dimmed(const QPalette& pal, int alpha = kTextSecondary)
{
    QColor c = pal.color(QPalette::Text);
    c.setAlpha(alpha);
    return c;
}

}  // namespace

BufferCardDelegate::BufferCardDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

// FTS5's snippet() marks the matched term with the two control characters we
// asked it for. Drawing the run between them at full strength is what makes a
// result list scannable — the eye goes straight to the word it searched for.
void BufferCardDelegate::drawSnippet(QPainter* p, const QRect& box, const QString& snippet,
                                     const QPalette& pal) const
{
    const QChar open(2), close(3);
    const QFontMetrics fm(p->font());
    int x = box.left();

    bool marked = false;
    for (const QString& run : snippet.split(QRegularExpression(QStringLiteral("[\\x02\\x03]")),
                                            Qt::KeepEmptyParts)) {
        if (!run.isEmpty()) {
            if (x > box.right()) break;
            p->setPen(marked ? pal.color(QPalette::Text) : dimmed(pal, kTextTertiary));
            QFont f = p->font();
            f.setBold(marked);
            p->setFont(f);
            const QString shown = fm.elidedText(run, Qt::ElideRight, box.right() - x);
            p->drawText(QRect(x, box.top(), box.right() - x, box.height()),
                        Qt::AlignLeft | Qt::AlignVCenter, shown);
            x += QFontMetrics(f).horizontalAdvance(shown);
            f.setBold(false);
            p->setFont(f);
        }
        marked = !marked;
    }
    Q_UNUSED(open); Q_UNUSED(close);
}

QFont BufferCardDelegate::timestampFont(const QFont& base) const
{
    return tokens::scaledBy(base, tokens::kTypeCaption);
}

int BufferCardDelegate::collapsedHeight() const
{
    const QFontMetrics fm(QApplication::font());
    const QFontMetrics tfm(timestampFont(QApplication::font()));
    // Two lines, not three. The old row reserved a secondary slot that was
    // empty on most buffers and a separate timestamp line below it — 96px of
    // which a third was blank. Count and age now share one line.
    const int text = kPadding + fm.height() + 3 + tfm.height() + kPadding;
    // A row carrying a thumbnail has to be tall enough for one. Derived from
    // text alone, the content area came to 35px and the thumbnail is 52px, so
    // it overflowed the card and painted straight over its own bottom border.
    return std::max(text, kThumbSize + kPadding * 2);
}

int BufferCardDelegate::sectionHeight(const QModelIndex& index) const
{
    return index.data(BufferListModel::SectionFirstRole).toBool() ? kSectionH : 0;
}

void BufferCardDelegate::setAnimationFrame(int row, const QPixmap& frame)
{
    animatedRow_ = row;
    animatedFrame_ = frame;
}

void BufferCardDelegate::clearAnimationFrame()
{
    animatedRow_ = -1;
    animatedFrame_ = {};
}

QSize BufferCardDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // O(1): no text layout here. With 5000 rows this runs constantly.
    return {option.rect.width(), sectionHeight(index) + collapsedHeight() + kMarginY * 2};
}

QRect BufferCardDelegate::cardRect(const QRect& itemRect, const QModelIndex& index) const
{
    QRect card = itemRect.adjusted(kMarginX, sectionHeight(index) + kMarginY, -kMarginX, -kMarginY);
    if (card.width() > kMaxCardWidth) {
        // Centre the column and let the window grow around it.
        card.setLeft(card.left() + (card.width() - kMaxCardWidth) / 2);
        card.setWidth(kMaxCardWidth);
    }
    return card;
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
    const bool isDraft  = index.data(BufferListModel::IsDraftRole).toBool();
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered  = option.state & QStyle::State_MouseOver;

    // --- section label --------------------------------------------------------
    if (index.data(BufferListModel::SectionFirstRole).toBool()) {
        QFont f = tokens::scaledBy(option.font, tokens::kTypeCaption);
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        p->setFont(f);
        p->setPen(dimmed(pal, kTextTertiary));
        const QRect card0 = cardRect(option.rect, index);
        const QRect labelRect(card0.left(), option.rect.top() + 12,
                              card0.width(), kSectionH - 14);
        p->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter,
                    index.data(BufferListModel::SectionNameRole).toString());
    }

    // --- card ------------------------------------------------------------------
    const QRect card = cardRect(option.rect, index);
    QPainterPath path;
    // Half-pixel inset, as the board's cards already do. On integral
    // coordinates an antialiased 1px stroke straddles two rows of pixels and
    // each gets about half the coverage: the list's edges measured 1.67:1
    // against the card while the board's identical token measured 3.3:1. The
    // colour was right and the geometry was throwing half of it away.
    path.addRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);

    // A buffer past the cutoff is drawn a touch quieter: still perfectly
    // readable, but the eye lands on what is current first.
    const bool older = index.data(BufferListModel::IsOlderRole).toBool();
    QColor fill = pal.color(QPalette::Base);
    if (hovered) {
        // A 2% shift is not a hover state, it is a rounding error. This is
        // still quiet, but it is actually perceptible.
        const bool lightTheme = pal.color(QPalette::Window).lightness() > 128;
        fill = lightTheme ? fill.darker(106) : fill.lighter(128);
    }
    p->fillPath(path, fill);

    QColor border = pal.color(QPalette::Text);
    border.setAlpha(selected ? kBorderActive : kBorderResting);
    p->setPen(QPen(border, 1));
    p->drawPath(path);

    // Focus is never signalled by colour alone: the selected card also carries a
    // solid accent rule down its leading edge (SPEC.md §14).
    if (selected) {
        QPainterPath clip;
        clip.addRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
        p->save();
        p->setClipPath(clip);
        p->fillRect(QRect(card.left(), card.top(), 3, card.height()),
                    pal.color(QPalette::Highlight));
        p->restore();
    }

    // Where the keyboard is, when the list has it: the same dotted accent ring
    // the board's cards use. Tabbing into the list showed nothing at all, so a
    // keyboard user could not tell the list had focus (usability test).
    if (option.state & QStyle::State_HasFocus) {
        QPainterPath ring;
        ring.addRoundedRect(QRectF(card).adjusted(3.5, 3.5, -3.5, -3.5), kRadius - 3, kRadius - 3);
        p->setPen(QPen(tokens::readableAccent(pal, 1.0), 2.0, Qt::DotLine));
        p->drawPath(ring);
    }

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
            icons::drawKeep(p, QRect(x, card.top() + kPadding, g, g), dimmed(pal, 215));
            x -= g + 6;
        }
        if (pinned)
            icons::drawPin(p, QRect(x, card.top() + kPadding, g, g), dimmed(pal, 215));
        glyphRight = card.right() - kPadding - x + g;
    }

    // --- thumbnails ------------------------------------------------------------
    // Up to three, so a buffer holding several images does not pretend to hold
    // one. Beyond that the row carries an overflow count.
    QRect content = contentRect(option.rect, index).adjusted(0, 0, -glyphRight, 0);
    std::vector<ImageRef> thumbs;
    if (const auto* m = qobject_cast<const BufferListModel*>(index.model()))
        thumbs = m->thumbsAt(index.row());

    if (thumbnailer_ && !thumbs.empty()) {
        const int count = int(thumbs.size());
        const int size = count == 1 ? kThumbSize : kThumbSizeMulti;
        const int gap = 5;
        const bool live = index.row() == animatedRow_ && !animatedFrame_.isNull();

        int x = content.left();
        for (int i = 0; i < count; ++i) {
            const QRect box(x, content.top(), size, size);
            const QPixmap pixmap = (live && i == 0)
                ? animatedFrame_
                : thumbnailer_->forBlob(thumbs[size_t(i)].hash, thumbs[size_t(i)].mime,
                                        size * 2);

            QPainterPath clip;
            clip.addRoundedRect(QRectF(box), 4, 4);
            if (pixmap.isNull()) {
                // The blob is gone. Say so visibly rather than drawing nothing —
                // silently blank content is indistinguishable from empty content.
                p->fillPath(clip, dimmed(pal, 50));
                p->setPen(dimmed(pal, 150));
                p->drawText(box, Qt::AlignCenter, QStringLiteral("?"));
            } else {
                const QPixmap scaled = pixmap.scaled(box.size(), Qt::KeepAspectRatioByExpanding,
                                                     Qt::SmoothTransformation);
                p->save();
                p->setClipPath(clip);
                p->drawPixmap(box, scaled,
                              QRect(QPoint((scaled.width() - box.width()) / 2,
                                           (scaled.height() - box.height()) / 2),
                                    box.size()));
                p->restore();
                // A hairline edge, so a picture the colour of the card — a dark
                // terminal screenshot in the dark theme — still reads as a
                // picture rather than as a hole (usability test, 2026-09-19).
                QColor edge = pal.color(QPalette::Text);
                edge.setAlpha(60);
                p->setPen(QPen(edge, 1));
                p->drawRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
            }

            // An animation showing only its first frame says so, rather than
            // looking like a still that happens not to move.
            if (thumbs[size_t(i)].animated && !(live && i == 0)) {
                QFont badge = tokens::scaledBy(option.font, tokens::kTypeMicro);
                badge.setBold(true);
                p->setFont(badge);
                const QFontMetrics bfm(badge);
                const QString text = QStringLiteral("GIF");
                const QRect pill(box.left() + 3, box.bottom() - bfm.height() - 2,
                                 bfm.horizontalAdvance(text) + 7, bfm.height() + 1);
                QPainterPath pillPath;
                pillPath.addRoundedRect(QRectF(pill), 3, 3);
                p->fillPath(pillPath, QColor(0, 0, 0, 160));
                p->setPen(Qt::white);
                p->drawText(pill, Qt::AlignCenter, text);
            }

            x = box.right() + gap;
        }

        content.setLeft(x + 7);
    }

    // --- content ---------------------------------------------------------------
    const QFontMetrics tfm(timestampFont(option.font));

    const QString primary = index.data(BufferListModel::PrimaryRole).toString();

    int y = content.top();

    // One weight step on one line is the largest "modern and sleek" return
    // available for zero pixels and zero colour. Everything else stays 400.
    QFont primaryFont = option.font;
    primaryFont.setWeight(QFont::Medium);
    p->setFont(primaryFont);
    const QFontMetrics pfm(primaryFont);

    if (primary.isEmpty()) {
        p->setPen(dimmed(pal, kTextTertiary));
        p->drawText(QRect(content.left(), y, content.width(), pfm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    isDraft ? QObject::tr("Empty — paste something into it")
                            : QObject::tr("Empty"));
    } else {
        p->setPen(older ? dimmed(pal, 210) : pal.color(QPalette::Text));
        p->drawText(QRect(content.left(), y, content.width(), pfm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    pfm.elidedText(primary, Qt::ElideRight, content.width()));
    }
    y += pfm.height() + 3;

    // When searching, the second line shows WHY this buffer matched rather than
    // how many items it has. A list of results that all read "4 items · 2 days
    // ago" tells you nothing about which one you wanted.
    const QString snippet = index.data(BufferListModel::SnippetRole).toString();
    if (!snippet.isEmpty()) {
        p->setFont(timestampFont(option.font));
        drawSnippet(p, QRect(content.left(), y, content.width(), tfm.height()), snippet, pal);
    } else if (!isDraft) {
        QStringList meta;
        const int count = index.data(BufferListModel::ItemCountRole).toInt();
        if (count > 1) meta << QObject::tr("%1 items").arg(count);
        const auto modified = index.data(BufferListModel::ModifiedAtRole).value<Timestamp>();
        meta << relativeTime(modified, nowMs());

        p->setFont(timestampFont(option.font));
        p->setPen(dimmed(pal, kTextTertiary));
        p->drawText(QRect(content.left(), y, content.width(), tfm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    tfm.elidedText(meta.join(QStringLiteral("  ·  ")), Qt::ElideRight,
                                   content.width()));
    }

    p->restore();
}

}  // namespace napkin
