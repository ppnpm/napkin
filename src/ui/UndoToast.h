#pragma once
#include "../domain/Types.h"
#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;

namespace napkin {

// SPEC.md §6. For an application whose premise is throwing things in without
// thinking, accidental deletion is the fastest way to lose a user's trust — so
// every delete is undoable for a few seconds, in one click, without hunting for
// the trash.
class UndoToast : public QWidget {
    Q_OBJECT
public:
    static constexpr int kVisibleMs = 8000;

    explicit UndoToast(QWidget* parent = nullptr);

    // Replaces any offer already showing: the most recent delete is the one the
    // user is most likely to have meant.
    void offer(const QString& message, BufferId id);
    void dismiss();

    // Re-centres without touching the message or restarting the countdown.
    void reposition();

    BufferId pendingId() const { return pending_; }

signals:
    void undoRequested(BufferId id);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QLabel*      message_ = nullptr;
    QPushButton* undo_    = nullptr;
    QTimer*      timer_   = nullptr;
    BufferId     pending_ = kNoBuffer;
};

}  // namespace napkin
