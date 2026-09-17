#pragma once
#include "../domain/Types.h"
#include <QListView>

namespace napkin {

class BufferCardDelegate;
class InlineEditor;

// The buffer stack. Virtualized by QListView; the one expanded row gets a real
// editor positioned over its content area, so editing happens in place rather
// than in a pane or a dialog (SPEC.md §7).
class BufferListView : public QListView {
    Q_OBJECT
public:
    explicit BufferListView(QWidget* parent = nullptr);

    void expandRow(int row, const QString& initialText);
    void collapse();

    int      expandedRow() const { return expandedRow_; }
    QString  editorText() const;
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

protected:
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    void repositionEditor();
    void syncExpandedHeight();

    BufferCardDelegate* delegate_ = nullptr;
    InlineEditor*       editor_   = nullptr;
    int                 expandedRow_ = -1;
};

}  // namespace napkin
