#pragma once
#include "../domain/Item.h"
#include "../domain/Types.h"
#include <QListView>

class QMovie;

namespace napkin {

class BlobStore;
class BufferCardDelegate;
class Thumbnailer;

// The left pane: the buffer stack. Editing happens in the canvas beside it, so
// this is now only a list — selection drives the canvas, and the inline editor
// and its expand/collapse machinery are gone.
class BufferListView : public QListView {
    Q_OBJECT
public:
    BufferListView(Thumbnailer& thumbs, BlobStore& blobs, QWidget* parent = nullptr);

signals:
    void rowActivated(int row);          // Enter or double-click
    void trashRequested(int row);
    void restoreRequested(int row);
    void contextMenuRequested(int row, const QPoint& globalPos);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    void updateHoverAnimation(const QModelIndex& index);
    void stopHoverAnimation();

    BufferCardDelegate* delegate_ = nullptr;
    BlobStore*          blobs_    = nullptr;
    QMovie*             hoverMovie_ = nullptr;
    int                 hoverRow_ = -1;
};

}  // namespace napkin
