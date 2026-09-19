#include "UndoToast.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>

namespace napkin {

UndoToast::UndoToast(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 10, 10, 10);
    layout->setSpacing(14);

    message_ = new QLabel;
    auto* undo = new QPushButton(tr("Undo"));
    undoButton_ = undo;
    undo->setFlat(true);
    undo->setCursor(Qt::PointingHandCursor);
    undo->setAccessibleName(tr("Undo the last deletion"));
    undo->setToolTip(tr("Undo (Ctrl+Z)"));

    layout->addWidget(message_);
    layout->addWidget(undo);

    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    timer_->setInterval(kVisibleMs);
    connect(timer_, &QTimer::timeout, this, &UndoToast::dismiss);

    connect(undo, &QPushButton::clicked, this, [this] { undoNow(); });

    hide();
}

bool UndoToast::undoNow()
{
    auto action = undo_;
    if (!action) return false;
    dismiss();
    action();
    emit undone();
    return true;
}

void UndoToast::inform(const QString& message)
{
    offer(message, nullptr);
}

void UndoToast::enterEvent(QEnterEvent* e)
{
    timer_->stop();
    QWidget::enterEvent(e);
}

void UndoToast::leaveEvent(QEvent* e)
{
    if (isVisible()) timer_->start();
    QWidget::leaveEvent(e);
}

void UndoToast::offer(const QString& message, std::function<void()> undo)
{
    undo_ = std::move(undo);
    undoButton_->setVisible(bool(undo_));
    message_->setText(message);
    adjustSize();
    reposition();
    show();
    raise();
    timer_->start();
}

void UndoToast::dismiss()
{
    timer_->stop();
    const bool had = bool(undo_);
    undo_ = nullptr;
    hide();
    // Expiring releases whatever the offer was holding, exactly as taking it up
    // would. Without this the blobs an abandoned offer protected are pinned
    // until the next delete.
    if (had) emit expired();
}

void UndoToast::reposition()
{
    if (!parentWidget()) return;
    const QSize s = sizeHint();
    move((parentWidget()->width() - s.width()) / 2, parentWidget()->height() - s.height() - 20);
    resize(s);
}

void UndoToast::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

    // Sits above the stack, so it needs its own surface rather than the card
    // fill — but still no shadow and no accent colour (SPEC.md §7).
    p.fillPath(path, palette().color(QPalette::Base));
    QColor border = palette().color(QPalette::Text);
    border.setAlpha(80);
    p.setPen(QPen(border, 1));
    p.drawPath(path);
}

}  // namespace napkin
