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
    undo_ = new QPushButton(tr("Undo"));
    undo_->setFlat(true);
    undo_->setCursor(Qt::PointingHandCursor);

    layout->addWidget(message_);
    layout->addWidget(undo_);

    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    timer_->setInterval(kVisibleMs);
    connect(timer_, &QTimer::timeout, this, &UndoToast::dismiss);

    connect(undo_, &QPushButton::clicked, this, [this] {
        const BufferId id = pending_;
        dismiss();
        if (id != kNoBuffer) emit undoRequested(id);
    });

    hide();
}

void UndoToast::offer(const QString& message, BufferId id)
{
    pending_ = id;
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
    pending_ = kNoBuffer;
    hide();
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
