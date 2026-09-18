#pragma once
#include "../domain/Item.h"
#include <QWidget>

class QLabel;

class QPlainTextEdit;

namespace napkin {

class BlobStore;
class Thumbnailer;

// One item in the canvas, as a selectable object.
//
// The hard part of this layout is that a text block must be BOTH a selectable
// object and an editable field. The resolution:
//
//   single click       -> SELECT the block, whatever it is
//   double click       -> edit it (text) or open the lightbox (image)
//   Enter on selected  -> edit it
//   Ctrl/Shift+click   -> extend the selection
//   Esc while editing  -> leave the text, keep the block selected
//   click empty canvas -> clear the selection, focus the composer
//
// Selection is uniform: every item answers a click the same way, so "click it,
// then delete it" works on text exactly as it works on an image. An earlier
// build put the caret straight into text on a single click, which made a text
// block the one thing in the canvas the mouse could not select or delete.
//
// The trailing composer is the exception: it is empty and has no row, so
// selecting it would mean nothing. Clicking it just starts writing.
class ItemCard : public QWidget {
    Q_OBJECT
public:
    explicit ItemCard(const Item& item, QWidget* parent = nullptr);

    ItemId itemId() const { return item_.id; }
    const Item& item() const { return item_; }

    bool isSelected() const { return selected_; }
    void setSelected(bool selected);

    virtual QString asPlainText() const { return {}; }

signals:
    void selectRequested(ItemId id, Qt::KeyboardModifiers modifiers);
    void activated(ItemId id);
    void escaped();

public:
    // True while a caret is inside this block, which is a different state from
    // "selected" and must look different.
    virtual bool hasEditFocus() const { return false; }

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

    Item item_;

private:
    bool selected_ = false;
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
    void beginEditing();
    void focusTextInteraction();
    void endEditing();
    bool isComposer() const { return itemId() == kNoItem; }
    int desiredHeight() const;
    bool textHasFocus() const;
    // Editing MODE, not window focus: a block being edited must still look
    // edited when the window is inactive, and window focus is not something a
    // headless test can grant.
    bool hasEditFocus() const override;

signals:
    void edited();
    void imagePasted(const QByteArray& bytes, const QString& mime);
    void heightChanged();

signals:
    void editingStarted(ItemId id);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    QPlainTextEdit* edit_ = nullptr;
    bool dirty_ = false;
};

class ImageItemCard : public ItemCard {
    Q_OBJECT
public:
    ImageItemCard(const Item& item, Thumbnailer& thumbs, BlobStore& blobs,
                  QWidget* parent = nullptr);

    QString asPlainText() const override;
    int desiredHeight() const;

protected:
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void rescale();

    Thumbnailer& thumbs_;
    QPixmap      source_;
    QLabel*      view_ = nullptr;
    QLabel*      caption_ = nullptr;
};

}  // namespace napkin
