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

    // Narrows the board to the items that matched, and marks the term inside
    // them. An empty query restores everything. `showAll` keeps the filter's
    // marking but stops hiding the rest, for when the surrounding items are the
    // context you actually wanted.
    void setSearch(const QString& query, const std::vector<ItemId>& matching);
    void setShowAll(bool showAll);
    bool isFiltered() const { return !query_.isEmpty() && !showAll_; }
    int  matchCount() const { return int(matching_.size()); }
    int  totalCount() const { return int(allItems_.size()); }
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
    // Tells the cards that just saved to say so.
    void acknowledgeSaved(const QList<ItemId>& ids, Timestamp when);
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

    // Ends any card that is being edited. Called whenever attention moves
    // elsewhere — another card, the empty board, another buffer — because
    // leaving a card is a commit.
    void commitEditing();
    bool isEditing() const;

signals:
    void edited();
    // A card stopped being edited. leftEmpty says whether it now holds nothing,
    // which is the ONLY moment an empty card is removed.
    void editingFinished(ItemId id, bool leftEmpty);
    void imagePasted(const QByteArray& bytes, const QString& mime);
    void imageActivated(ItemId id);
    void removeRequested(const QList<ItemId>& ids);
    void selectionChanged();
    void filterChanged();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private slots:
    // Handing a URL to the desktop is the one thing in Napkin that leaves the
    // machine, so it goes through one function that re-checks the scheme rather
    // than trusting that whatever produced the chip got it right.
    void openUrl(const QString& url);

private:
    void wireCard(ItemCard* card);
    void applySelection(ItemId id, Qt::KeyboardModifiers modifiers);
    void relayout();
    int  stableWidth() const;

    // Builds widgets for the visible band and destroys the rest. A buffer with
    // 1000 text items used to construct 1000 live QPlainTextEdits: 365 MB peak
    // and 270 ms on every resize event. Only what you can see exists.
    void syncVisibleCards();
    void applyFilter();
    void syncCardText(TextItemCard* card);
    ItemCard* cardFor(const Item& item);

    Thumbnailer& thumbs_;
    BlobStore&   blobs_;
    QWidget*     body_ = nullptr;
    BoardLayout    board_;
    std::vector<Item> allItems_;              // everything in the buffer
    std::vector<Item> items_;                 // what the board is showing
    QString           query_;
    QSet<ItemId>      matching_;
    bool              showAll_ = false;
    QHash<ItemId, ItemCard*> live_;           // the cards that currently exist
    QLabel*      placeholder_ = nullptr;

    std::vector<ItemCard*>     cards_;
    std::vector<TextItemCard*> textCards_;
    QSet<ItemId> selected_;
    ItemId       anchor_ = kNoItem;   // for Shift+click ranges
    int          cursor_ = -1;        // the keyboard cursor
};

}  // namespace napkin
