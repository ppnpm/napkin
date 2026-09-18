#pragma once
#include <QLayout>
#include <QList>

namespace napkin {

// Column-balanced flow: each card goes into whichever column is currently
// shortest. Cards keep a uniform width and their own natural height, which is
// what lets a two-word note and a tall screenshot sit side by side without
// either being padded out or cropped to a common size.
//
// A plain vertical list gave every card the full pane width, so a three-word
// paste occupied a 780px row. A fixed grid would force a common height and clip
// the tall ones. This is the layout the content actually wants.
class MasonryLayout : public QLayout {
    Q_OBJECT
public:
    explicit MasonryLayout(QWidget* parent = nullptr);
    ~MasonryLayout() override;

    void setColumnWidth(int min, int max);
    void setSpacingBetween(int spacing);

    void addItem(QLayoutItem* item) override;
    // Newest-first means a new card belongs at the top the moment it exists,
    // not after the next reload.
    void insertWidgetAt(int index, QWidget* widget);
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    int count() const override;

    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect& rect) override;

    int columnCount(int width) const;
    int columnWidth(int width) const;

private:
    int layoutInto(const QRect& rect, bool apply) const;

    QList<QLayoutItem*> items_;
    int minColumn_ = 280;
    int maxColumn_ = 420;
    int gap_ = 16;
};

}  // namespace napkin
