#pragma once
#include "../domain/Item.h"
#include <QScrollArea>

class QLabel;
#include <QSet>
#include <vector>

class QVBoxLayout;

namespace napkin {

class BlobStore;
class ItemCard;
class TextItemCard;
class Thumbnailer;

// The right pane: the contents of whichever buffer is selected in the list.
//
// This replaces inline expansion. A card sized for a two-line preview could not
// host four images and item-level operations, and the attempt to make it do so
// is what produced "images do not look very good" — the canvas gives an image
// the width of the pane instead of a 52px square crop.
class ItemCanvas : public QScrollArea {
    Q_OBJECT
public:
    ItemCanvas(Thumbnailer& thumbs, BlobStore& blobs, QWidget* parent = nullptr);

    void setItems(const std::vector<Item>& items);
    void showNothingSelected();
    void clearItems();

    struct DirtyText {
        ItemId  id;      // kNoItem => new text with no row yet
        QString text;
    };
    std::vector<DirtyText> dirtyText() const;
    void markClean();
    bool rebindTextIds(const std::vector<Item>& items);

    QList<ItemId> selection() const;
    bool hasSelection() const { return !selected_.isEmpty(); }
    void clearSelection();
    void selectAll();

    // Clipboard and editing verbs over the current selection.
    void copySelection() const;
    void cutSelection();       // copies, then asks for removal
    void deleteSelection();

    void focusComposer();
    bool textHasFocus() const;

signals:
    void edited();
    void imagePasted(const QByteArray& bytes, const QString& mime);
    void imageActivated(ItemId id);
    void removeRequested(const QList<ItemId>& ids);
    void selectionChanged();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void addCard(ItemCard* card);
    void addComposer();
    void applySelection(ItemId id, Qt::KeyboardModifiers modifiers);
    void relayout();

    Thumbnailer& thumbs_;
    BlobStore&   blobs_;
    QWidget*     body_ = nullptr;
    QVBoxLayout* layout_ = nullptr;
    QLabel*      placeholder_ = nullptr;

    std::vector<ItemCard*>     cards_;
    std::vector<TextItemCard*> textCards_;
    QSet<ItemId> selected_;
    ItemId       anchor_ = kNoItem;   // for Shift+click ranges
};

}  // namespace napkin
