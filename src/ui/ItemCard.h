#pragma once
#include "../domain/Item.h"
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QTextDocument;

namespace napkin {

class BlobStore;
class CardFooter;
class Thumbnailer;

// One item on the board, as a card.
//
// Selection is uniform — every item answers a click the same way, so "click it,
// then delete it" works on a paragraph exactly as it works on a picture:
//
//   single click       -> select
//   double click       -> edit it (text) or open the lightbox (image)
//   Enter on selected  -> edit it
//   Ctrl/Shift+click   -> extend the selection
//   Esc while editing  -> stop editing, keep the card selected
//   click empty board  -> clear the selection
class ItemCard : public QWidget {
    Q_OBJECT
public:
    explicit ItemCard(const Item& item, QWidget* parent = nullptr);

    ItemId itemId() const { return item_.id; }
    const Item& item() const { return item_; }
    bool isComposer() const { return item_.id == kNoItem; }

    bool isSelected() const { return selected_; }
    void setSelected(bool selected);

    // The keyboard cursor. Distinct from selection: you can move the cursor
    // across cards with the arrow keys and the card under it draws a focus ring
    // whether or not it is part of the selection.
    bool isCurrent() const { return current_; }
    void setCurrent(bool current);

    virtual QString asPlainText() const { return {}; }

    // Natural height at this column width, clamped between the minimum a card
    // is allowed to be and the maximum it may grow to. A card's size depends
    // only on its own content — never on what its neighbours are doing.
    int heightForColumn(int width) const;
    bool isClipped() const { return clipped_; }

    virtual bool hasEditFocus() const { return false; }

signals:
    void selectRequested(ItemId id, Qt::KeyboardModifiers modifiers);
    void activated(ItemId id);
    void escaped();
    void copyRequested(ItemId id);

protected:
    // Subclasses call this once, with the widget that fills the content area.
    void setContent(QWidget* content, const QString& copyLabel);
    virtual int contentHeightForWidth(int innerWidth) const = 0;
    int chromeHeight() const;

    void mousePressEvent(QMouseEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

    Item item_;
    mutable bool clipped_ = false;

private:
    CardFooter* footer_ = nullptr;
    bool selected_ = false;
    bool current_ = false;
    bool hovered_ = false;
};

class TextItemCard : public ItemCard {
    Q_OBJECT
public:
    explicit TextItemCard(const Item& item, QWidget* parent = nullptr);

    QString text() const;
    QString asPlainText() const override { return text(); }
    bool isDirty() const { return dirty_; }
    void markClean() { dirty_ = false; }
    void setItemId(ItemId id) { item_.id = id; }
    void focusText();
    void beginEditing(bool moveToEnd = true);
    void selectAllText();
    void focusTextInteraction();
    void endEditing();
    bool textHasFocus() const;
    bool hasEditFocus() const override;
    void updateAccessibleName();

signals:
    void edited();
    void imagePasted(const QByteArray& bytes, const QString& mime);
    void heightChanged();
    void editingStarted(ItemId id);

protected:
    int contentHeightForWidth(int innerWidth) const override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    QPlainTextEdit* edit_ = nullptr;
    bool dirty_ = false;

    // Measuring happens against our own document, not the editor's. See the
    // note on contentHeightForWidth.
    mutable QTextDocument* measure_ = nullptr;
    mutable QString measured_;
};

class ImageItemCard : public ItemCard {
    Q_OBJECT
public:
    ImageItemCard(const Item& item, Thumbnailer& thumbs, BlobStore& blobs,
                  QWidget* parent = nullptr);

    QString asPlainText() const override;

protected:
    int contentHeightForWidth(int innerWidth) const override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void rescale();

    QPixmap source_;
    QLabel* view_ = nullptr;
    QLabel* caption_ = nullptr;
};

}  // namespace napkin
