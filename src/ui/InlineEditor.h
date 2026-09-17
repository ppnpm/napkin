#pragma once
#include <QWidget>

class QPlainTextEdit;

namespace napkin {

// The editing surface for an expanded card. Plain text only — no Markdown
// preview, no rich text, no syntax highlighting (SPEC.md §1). It stores what
// you paste and gets out of the way.
class InlineEditor : public QWidget {
    Q_OBJECT
public:
    explicit InlineEditor(QWidget* parent = nullptr);

    void setText(const QString& text);
    QString text() const;
    void focusEditor();

    // Height the document wants, for the delegate's expanded sizeHint.
    int desiredHeight() const;

signals:
    // An image arrived on the clipboard while this editor had focus. The
    // editor does not store it — it hands it up to be turned into an item.
    void imagePasted(const QByteArray& bytes, const QString& mime);
    void textEdited();
    void collapseRequested();
    void heightChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPlainTextEdit* edit_ = nullptr;
};

}  // namespace napkin
