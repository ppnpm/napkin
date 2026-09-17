#pragma once
#include <QDialog>

class QLabel;
class QMovie;

namespace napkin {

// Full-size image view, and where an animation actually plays. The answer to
// the one real problem the master-detail mockup exposed (§7): a 2560x1440
// screenshot is cramped in a card, but a permanent second pane makes every
// text buffer pay for that.
class Lightbox : public QDialog {
    Q_OBJECT
public:
    Lightbox(const QString& imagePath, bool animated, QString caption, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void rescale();

    QPixmap source_;
    QMovie* movie_ = nullptr;
    QLabel* view_  = nullptr;
};

}  // namespace napkin
