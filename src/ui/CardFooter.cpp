#include "CardFooter.h"
#include "Icons.h"
#include "Tokens.h"
#include "../domain/Clock.h"
#include "../domain/TimeFormat.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>

namespace napkin {
namespace {
using namespace tokens;
constexpr int kIcon = 14;
constexpr int kIconGap = 7;
}  // namespace

CardFooter::CardFooter(QString actionLabel, QWidget* parent)
    : QWidget(parent), label_(std::move(actionLabel))
{
    setFixedHeight(kCardFooterH);
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
    setAccessibleName(label_);
}

void CardFooter::setTimestamp(qint64 modifiedAt)
{
    modifiedAt_ = modifiedAt;
    refreshTimestamp();
}

void CardFooter::refreshTimestamp()
{
    const QString fresh = modifiedAt_ ? relativeTime(modifiedAt_, nowMs()) : QString();
    if (fresh == age_) return;
    age_ = fresh;
    update();
}

QRect CardFooter::actionRect() const
{
    const QFontMetrics fm(font());
    const int w = kIcon + kIconGap + fm.horizontalAdvance(label_);
    return QRect(0, 0, w + 8, height());
}

void CardFooter::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPalette& pal = palette();

    const QColor actionColour = hoveringAction_ ? text(pal, 230) : text(pal, kTextTertiary);
    const QRect action = actionRect();
    icons::drawCopy(&p, QRect(action.left(), (height() - kIcon) / 2, kIcon, kIcon),
                    actionColour);

    p.setPen(actionColour);
    p.drawText(QRect(action.left() + kIcon + kIconGap, 0,
                     action.width() - kIcon - kIconGap, height()),
               Qt::AlignLeft | Qt::AlignVCenter, label_);

    if (age_.isEmpty()) return;
    p.setPen(text(pal, kTextTertiary));
    p.drawText(rect(), Qt::AlignRight | Qt::AlignVCenter, age_);
}

void CardFooter::mousePressEvent(QMouseEvent* e)
{
    // Only the action swallows the click. Everywhere else on the footer the
    // press belongs to the card, so clicking a card's chrome still selects it.
    if (actionRect().contains(e->pos())) { pressed_ = true; e->accept(); return; }
    e->ignore();
}

void CardFooter::mouseReleaseEvent(QMouseEvent* e)
{
    if (pressed_ && actionRect().contains(e->pos())) emit actionTriggered();
    pressed_ = false;
    e->accept();
}

void CardFooter::enterEvent(QEnterEvent* e)
{
    hoveringAction_ = actionRect().contains(e->position().toPoint());
    setCursor(hoveringAction_ ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
    QWidget::enterEvent(e);
}

void CardFooter::leaveEvent(QEvent* e)
{
    hoveringAction_ = false;
    setCursor(Qt::ArrowCursor);
    update();
    QWidget::leaveEvent(e);
}

}  // namespace napkin
