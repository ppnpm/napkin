#pragma once
#include <QDialog>

class QLabel;

namespace napkin {

// Full-size image view. The answer to the one real problem the master-detail
// mockup exposed (§7): a 2560x1440 screenshot is cramped in a card. A lightbox
// buys the room on demand, instead of making every text buffer pay for a
// permanent second pane.
class Lightbox : public QDialog {
    Q_OBJECT
public:
    Lightbox(const QPixmap& image, QString caption, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void rescale();

    QPixmap source_;
    QLabel* view_ = nullptr;
};

}  // namespace napkin
