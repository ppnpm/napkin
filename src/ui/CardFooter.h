#pragma once
#include <QWidget>

class QTimer;

class QLabel;

namespace napkin {

// The strip at the bottom of every card: one low-emphasis action on the left,
// the item's age on the right.
//
// The action duplicates Ctrl+C deliberately. Ctrl+C acts on the *selection*,
// which means selecting first; the footer button is the one-click path for the
// overwhelmingly common case of "give me that one thing". Two mechanisms, but
// for two different intents rather than the same one twice.
class CardFooter : public QWidget {
    Q_OBJECT
public:
    CardFooter(QString actionLabel, QWidget* parent = nullptr);

    void setTimestamp(qint64 modifiedAt);
    // The label changes when what the card *is* changes: editing a note until
    // it is nothing but a URL turns "Copy text" into "Copy link".
    void setActionLabel(const QString& label);
    // Shown when the card cannot display all of its content.
    void setClipped(bool clipped);

    // Shows a message in place of the age for a moment, then reverts. The only
    // way a user can tell that a copy happened, or that an edit was written.
    void flash(const QString& message);
    bool isFlashing() const { return !flash_.isEmpty(); }
    // Acknowledges the ACTION on the action itself: "Copy text" reads "Copied"
    // for a moment. It used to replace the age at the other end of the footer,
    // away from where the user had just clicked. "Saved" stays over there —
    // it is about the note, not the button.
    void acknowledgeAction(const QString& message);
    QString shownActionLabel() const { return actionFlash_.isEmpty() ? label_ : actionFlash_; }
    void refreshTimestamp();

signals:
    void actionTriggered();

protected:
    void changeEvent(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    QRect actionRect() const;

    QString label_;
    qint64  modifiedAt_ = 0;
    QString age_;
    bool    hoveringAction_ = false;
    bool    pressed_ = false;
    bool    clipped_ = false;
    QString flash_;
    QTimer* flashTimer_ = nullptr;
    QString actionFlash_;
    QTimer* actionFlashTimer_ = nullptr;
};

}  // namespace napkin
