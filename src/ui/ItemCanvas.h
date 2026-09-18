#pragma once
#include "../domain/Item.h"
#include "BoardLayout.h"
#include <QScrollArea>

#include <QHash>
#include <QSet>
#include <vector>

class QLabel;


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

    // selectIndex: which item to leave selected afterwards, so a delete does
    // not dump the user back to nothing selected. -1 selects none.
    void setItems(const std::vector<Item>& items, int selectIndex = -1);
    int  indexOf(ItemId id) const;
    // Board order, newest first. Not the same as the widget tree order, which
    // is creation order.
    QList<ItemId> itemOrder() const;
    void showNothingSelected();
    // A buffer that exists but holds nothing yet: Ctrl+N makes one of these and
    // it waits to be pasted into.
    void showEmptyBuffer();
    void clearItems();

    struct DirtyText {
        ItemId  id;      // kNoItem => new text with no row yet
        QString text;
    };
    std::vector<DirtyText> dirtyText() const;
    void markClean();
    // Binds the one unwritten card to the row that was just created for it.
    //
    // NOT by position. An earlier version paired textCards_[i] with the i-th
    // text row of listForBuffer, which assumes widget order equals database
    // order — and editing any card that is not the newest bumps its
    // modified_at, changing the database order while the board deliberately
    // stays put. The next append then rebound every card one slot out, so
    // typing into one note silently overwrote another. There is only ever one
    // composer, so identity is unambiguous and position is never consulted.
    bool bindComposer(ItemId newId);

    // --- keyboard ------------------------------------------------------------
    // The board is fully operable without a mouse: arrows move the cursor,
    // Shift+arrows extend, Space toggles, Enter edits, Home/End jump. Every
    // canvas verb acts on the selection, so without a keyboard route to the
    // selection a keyboard user could select all or nothing — which is how this
    // shipped until an audit drove it and found nothing worked.
    void moveCursor(int delta, Qt::KeyboardModifiers modifiers);
    void setCursorTo(int index, Qt::KeyboardModifiers modifiers);
    int  cursorIndex() const { return cursor_; }

    QList<ItemId> selection() const;
    bool hasSelection() const { return !selected_.isEmpty(); }
    void clearSelection();
    void selectAll();

    // Clipboard and editing verbs over the current selection.
    void copySelection() const;
    void cutSelection();       // copies, then asks for removal
    void deleteSelection();

    // Adds an unwritten text card at the top and puts the caret in it. It
    // becomes a real item when it has content, and evaporates if it does not.
    void addPendingTextCard();
    bool textHasFocus() const;

    // True when the keyboard is anywhere inside the board. Not hasFocus(),
    // which also requires the window to be active — so it answers false for a
    // background window, and always false under a headless platform.
    bool keyboardIsHere() const;

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
    void wireCard(ItemCard* card);
    void applySelection(ItemId id, Qt::KeyboardModifiers modifiers);
    void relayout();
    int  stableWidth() const;

    // Builds widgets for the visible band and destroys the rest. A buffer with
    // 1000 text items used to construct 1000 live QPlainTextEdits: 365 MB peak
    // and 270 ms on every resize event. Only what you can see exists.
    void syncVisibleCards();
    ItemCard* cardFor(const Item& item);

    Thumbnailer& thumbs_;
    BlobStore&   blobs_;
    QWidget*     body_ = nullptr;
    BoardLayout    board_;
    std::vector<Item> items_;                 // every item; widgets only for some
    QHash<ItemId, ItemCard*> live_;           // the cards that currently exist
    QLabel*      placeholder_ = nullptr;

    std::vector<ItemCard*>     cards_;
    std::vector<TextItemCard*> textCards_;
    QSet<ItemId> selected_;
    ItemId       anchor_ = kNoItem;   // for Shift+click ranges
    int          cursor_ = -1;        // the keyboard cursor
};

}  // namespace napkin
