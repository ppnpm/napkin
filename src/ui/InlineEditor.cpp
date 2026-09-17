#include "InlineEditor.h"
#include "BufferCardDelegate.h"

#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QVBoxLayout>

namespace napkin {

InlineEditor::InlineEditor(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    edit_ = new QPlainTextEdit;
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
