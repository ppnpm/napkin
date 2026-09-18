#pragma once
#include "../domain/Types.h"
#include <QWidget>
#include <functional>

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
    // user is most likely to have meant. The action is a closure so buffer-level
    // and item-level undo share one widget rather than one growing an enum.
    void offer(const QString& message, std::function<void()> undo);
    void dismiss();

    // Re-centres without touching the message or restarting the countdown.
    void reposition();

    bool hasOffer() const { return bool(undo_); }

signals:
    void undone();

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QLabel*      message_ = nullptr;
    QTimer*      timer_   = nullptr;
    std::function<void()> undo_;
};

}  // namespace napkin
