#pragma once
#include "../domain/Item.h"
#include <QWidget>
#include <vector>

class QVBoxLayout;

namespace napkin {

class BlobStore;
class TextItemWidget;
class Thumbnailer;

// The expanded card. Shows *every* item the buffer holds, in order, rather than
// a single text box — a buffer is a screenshot and a command and a URL kept
// together, and the editor has to make that visible or the concept is a lie.
class BufferEditor : public QWidget {
    Q_OBJECT
public:
    BufferEditor(Thumbnailer& thumbs, BlobStore& blobs, QWidget* parent = nullptr);

    // An empty list means an unwritten draft: one empty text box, no row yet.
    void setItems(const std::vector<Item>& items);
    void clear();

    struct DirtyText {
        ItemId  id;      // kNoItem => this text is new and has no row yet
        QString text;
    };
    std::vector<DirtyText> dirtyText() const;
    void markClean();

    // After a draft is first written, its text widgets correspond to real rows.
    // Rebinding their ids in place keeps the cursor, the selection and the focus
    // exactly where the user left them — rebuilding the widgets would move the
    // caret back to the start and the next keystrokes would land in the wrong
    // place. Returns false when the shape no longer matches and a full rebuild
    // is unavoidable.
    bool rebindTextIds(const std::vector<Item>& items);

    void focusFirstEditor();
    int  desiredHeight() const;

signals:
    void edited();
    void imagePasted(const QByteArray& bytes, const QString& mime);
    void imageActivated(ItemId id);
    void itemRemoveRequested(ItemId id);
    void collapseRequested();
    void heightChanged();

private:
    void addTextItem(ItemId id, const QString& text);
    void addTrailingComposer();

    Thumbnailer& thumbs_;
    BlobStore&   blobs_;
    QVBoxLayout* layout_ = nullptr;
    std::vector<TextItemWidget*> textWidgets_;
};

}  // namespace napkin
