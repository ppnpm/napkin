#pragma once
#include "../domain/Item.h"
#include "../domain/Types.h"
#include <vector>
#include <QListView>

class QMovie;

namespace napkin {

class BufferCardDelegate;
class BlobStore;
class Thumbnailer;
class BufferEditor;

// The buffer stack. Virtualized by QListView; the one expanded row gets a real
// editor positioned over its content area, so editing happens in place rather
// than in a pane or a dialog (SPEC.md §7).
class BufferListView : public QListView {
    Q_OBJECT
public:
    BufferListView(Thumbnailer& thumbs, BlobStore& blobs, QWidget* parent = nullptr);

    BufferEditor* editor() const { return editor_; }


    void expandRow(int row, const std::vector<Item>& items);
    void collapse();

    int      expandedRow() const { return expandedRow_; }

    bool     isEditing() const { return expandedRow_ >= 0; }

signals:
    void rowActivated(int row);
    void collapseRequested();
    void editorTextChanged();

    // List-scope commands. Emitted only when the list itself has focus, so they
    // can never fire while the user is typing (SPEC.md §7).
    void pinToggleRequested(int row);
    void keepToggleRequested(int row);
    void trashRequested(int row);
    void contextMenuRequested(int row, const QPoint& globalPos);
    void imagePasted(const QByteArray& bytes, const QString& mime);
    void imageItemActivated(ItemId id);
    void itemRemoveRequested(ItemId id);

protected:
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    void repositionEditor();
    void syncExpandedHeight();

    void updateHoverAnimation(const QModelIndex& index);
    void stopHoverAnimation();

    BufferCardDelegate* delegate_ = nullptr;
    BlobStore*          blobs_    = nullptr;
    QMovie*             hoverMovie_ = nullptr;
    int                 hoverRow_ = -1;
    BufferEditor*       editor_   = nullptr;
    int                 expandedRow_ = -1;
};

}  // namespace napkin
