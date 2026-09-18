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
//   click on text        -> place the caret (edit)
//   click on an image    -> select the block (images are not editable)
//   Ctrl/Shift+click     -> select the block, never place a caret
//   Esc while editing    -> leave the text, select the block
//   click empty canvas   -> clear the selection
//
// So a bare click always does the obvious thing for what is under it, and
// selection of a text block is always reachable without a special target.
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

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

    Item item_;

private:
    bool selected_ = false;
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
    int desiredHeight() const;
    bool textHasFocus() const;

signals:
    void edited();
    void imagePasted(const QByteArray& bytes, const QString& mime);
    void heightChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

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
