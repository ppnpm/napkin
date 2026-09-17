#include "BufferCardDelegate.h"
#include "BufferListModel.h"
#include "Icons.h"
#include "../media/Thumbnailer.h"
#include "../domain/Preview.h"
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
        const int imageCount = index.data(BufferListModel::ImageCountRole).toInt();

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
            }

            // An animation showing only its first frame says so, rather than
            // looking like a still that happens not to move.
            if (thumbs[size_t(i)].animated && !(live && i == 0)) {
                QFont badge = option.font;
                badge.setPointSizeF(std::max(6.0, option.font.pointSizeF() - 3.5));
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

        if (imageCount > count) {
            const QRect more(x, content.top(), size, size);
            QPainterPath clip;
            clip.addRoundedRect(QRectF(more), 4, 4);
            p->fillPath(clip, dimmed(pal, 32));
            p->setFont(option.font);
            p->setPen(dimmed(pal, 170));
            p->drawText(more, Qt::AlignCenter, QStringLiteral("+%1").arg(imageCount - count));
            x = more.right() + gap;
        }

        content.setLeft(x + 7);
    }

    // --- content ---------------------------------------------------------------
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
