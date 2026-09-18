#pragma once
#include <QWidget>

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
    void refreshTimestamp();

signals:
    void actionTriggered();

protected:
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
};

}  // namespace napkin
