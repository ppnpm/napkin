#pragma once
#include "../domain/Item.h"
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QToolButton;

namespace napkin {

class BlobStore;
class Thumbnailer;

// One editable text item inside an expanded buffer.
class TextItemWidget : public QWidget {
    Q_OBJECT
public:
    explicit TextItemWidget(ItemId id, const QString& text, QWidget* parent = nullptr);

    ItemId itemId() const { return id_; }
    void setItemId(ItemId id) { id_ = id; }
    QString text() const;
    bool isDirty() const { return dirty_; }
    void markClean() { dirty_ = false; }
    void focusText();
    int desiredHeight() const;

signals:
    void edited();
    void imagePasted(const QByteArray& bytes, const QString& mime);
    void collapseRequested();
    void heightChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ItemId id_;
    QPlainTextEdit* edit_ = nullptr;
    bool dirty_ = false;
};

// One image item: thumbnail, name, dimensions, size, and a remove control.
// Napkin stores and shows captured material; it does not edit it (SPEC.md §1).
class ImageItemWidget : public QWidget {
    Q_OBJECT
public:
    ImageItemWidget(const Item& item, Thumbnailer& thumbs, BlobStore& blobs,
                    QWidget* parent = nullptr);

    ItemId itemId() const { return id_; }

signals:
    void activated(ItemId id);
    void removeRequested(ItemId id);

protected:
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    ItemId id_;
    QToolButton* remove_ = nullptr;
};

}  // namespace napkin
