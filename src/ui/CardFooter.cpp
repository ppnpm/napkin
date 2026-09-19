#include "CardFooter.h"
#include <algorithm>
#include <QEvent>
#include "Icons.h"
#include "Tokens.h"
#include "../domain/Clock.h"
#include "../domain/TimeFormat.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

namespace napkin {
namespace {
using namespace tokens;
constexpr int kIcon = 14;
constexpr int kIconGap = 7;
}  // namespace

CardFooter::CardFooter(QString actionLabel, QWidget* parent)
    : QWidget(parent), label_(std::move(actionLabel))
{
    setFixedHeight(footerHeight(font()));
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
    setAccessibleName(label_);
}

void CardFooter::setActionLabel(const QString& label)
{
    if (label_ == label) return;
    label_ = label;
    update();
}

void CardFooter::setTimestamp(qint64 modifiedAt)
{
    modifiedAt_ = modifiedAt;
    refreshTimestamp();
}

void CardFooter::flash(const QString& message)
{
    flash_ = message;
    if (!flashTimer_) {
        flashTimer_ = new QTimer(this);
        flashTimer_->setSingleShot(true);
        connect(flashTimer_, &QTimer::timeout, this, [this] { flash_.clear(); update(); });
    }
    flashTimer_->start(1600);
    update();
}

void CardFooter::acknowledgeAction(const QString& message)
{
    actionFlash_ = message;
    if (!actionFlashTimer_) {
        actionFlashTimer_ = new QTimer(this);
        actionFlashTimer_->setSingleShot(true);
        connect(actionFlashTimer_, &QTimer::timeout, this, [this] { actionFlash_.clear(); update(); });
    }
    actionFlashTimer_->start(1600);
    update();
}

void CardFooter::setClipped(bool clipped)
{
    if (clipped_ == clipped) return;
    clipped_ = clipped;
    setToolTip(clipped_ ? tr("There is more here than fits — open it to read it all")
                        : QString());
    update();
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
    // As wide as the wider of the two labels, so the click target does not
    // shrink under the pointer while "Copied" is showing.
    const int w = kIcon + kIconGap + std::max(fm.horizontalAdvance(label_),
                                              fm.horizontalAdvance(actionFlash_));
    return QRect(0, 0, w + 8, height());
}

void CardFooter::changeEvent(QEvent* e)
{
    // The footer's height is derived from the font, so it has to be re-derived
    // when the font moves under it — Settings ▸ Appearance can change the face
    // and the size of a window that is already open.
    if (e->type() == QEvent::FontChange || e->type() == QEvent::ApplicationFontChange)
        setFixedHeight(footerHeight(font()));
    QWidget::changeEvent(e);
}

void CardFooter::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPalette& pal = palette();

    // While acknowledging, the action is drawn at full accent strength, as the
    // age's flash is: it is the answer to the click.
    const QColor actionColour = !actionFlash_.isEmpty() ? readableAccent(pal, 1.0)
                              : hoveringAction_         ? text(pal, 230)
                                                        : text(pal, kTextTertiary);
    const QRect action = actionRect();
    icons::drawCopy(&p, QRect(action.left(), (height() - kIcon) / 2, kIcon, kIcon),
                    actionColour);

    p.setPen(actionColour);
    p.drawText(QRect(action.left() + kIcon + kIconGap, 0,
                     action.width() - kIcon - kIconGap, height()),
               Qt::AlignLeft | Qt::AlignVCenter, shownActionLabel());

    // A clipped card has to say so. The previous attempt painted a fade in the
    // card's own paintEvent — but the text edit is a CHILD and paints after its
    // parent, so the glyphs went straight over the gradient at full opacity and
    // a 1 MB paste looked identical to a 21-line note.
    QString right = age_;
    if (clipped_) right = right.isEmpty() ? tr("more…") : tr("more…   %1").arg(age_);
    if (!flash_.isEmpty()) right = flash_;
    if (right.isEmpty()) return;

    // The flash is the acknowledgement, so it is drawn at full strength rather
    // than the quiet tertiary the age uses.
    p.setPen(flash_.isEmpty() ? text(pal, kTextTertiary) : readableAccent(pal, 1.0));
    p.drawText(rect(), Qt::AlignRight | Qt::AlignVCenter, right);
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
