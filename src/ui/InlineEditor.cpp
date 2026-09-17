#include "InlineEditor.h"
#include "BufferCardDelegate.h"
#include "../media/ClipboardContent.h"

#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPlainTextEdit>
#include <functional>
#include <QTextDocument>
#include <QVBoxLayout>

namespace napkin {
namespace {

// QPlainTextEdit would otherwise drop an image on the floor and paste whatever
// text came alongside it — the exact inversion of the §4 preference order.
class PasteAwareTextEdit : public QPlainTextEdit {
public:
    using QPlainTextEdit::QPlainTextEdit;
    std::function<bool(const QMimeData*)> onPaste;

protected:
    bool canInsertFromMimeData(const QMimeData* source) const override
    {
        return (source && source->hasImage()) || QPlainTextEdit::canInsertFromMimeData(source);
    }

    void insertFromMimeData(const QMimeData* source) override
    {
        if (onPaste && onPaste(source)) return;   // handled as an image
        QPlainTextEdit::insertFromMimeData(source);
    }
};

}  // namespace

InlineEditor::InlineEditor(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* edit = new PasteAwareTextEdit;
    edit->onPaste = [this](const QMimeData* source) {
        const auto content = readClipboard(source);
        if (content.kind != ClipboardContent::Kind::Image) return false;
        emit imagePasted(content.imageBytes, content.imageMime);
        return true;
    };
    edit_ = edit;
    edit_->setFrameShape(QFrame::NoFrame);
    edit_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    edit_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit_->setPlaceholderText(tr("Type or paste something…"));
    edit_->setTabChangesFocus(true);
    edit_->viewport()->setAutoFillBackground(false);
    edit_->setStyleSheet(QStringLiteral("QPlainTextEdit { background: transparent; }"));
    edit_->installEventFilter(this);
    layout->addWidget(edit_);

    connect(edit_, &QPlainTextEdit::textChanged, this, [this] {
        emit textEdited();
        emit heightChanged();
    });
}

void InlineEditor::setText(const QString& text)
{
    QSignalBlocker block(edit_);
    edit_->setPlainText(text);
    edit_->moveCursor(QTextCursor::End);
}

QString InlineEditor::text() const { return edit_->toPlainText(); }

void InlineEditor::focusEditor()
{
    edit_->setFocus(Qt::OtherFocusReason);
    edit_->moveCursor(QTextCursor::End);
}

int InlineEditor::desiredHeight() const
{
    const qreal docHeight = edit_->document()->documentLayout()->documentSize().height();
    return int(docHeight) + BufferCardDelegate::kPadding * 2 + 12;
}

bool InlineEditor::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == edit_ && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        // Esc collapses. Everything else belongs to the text — an editor that
        // swallows keys is worse than one that does too little.
        if (key->key() == Qt::Key_Escape) {
            emit collapseRequested();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace napkin
