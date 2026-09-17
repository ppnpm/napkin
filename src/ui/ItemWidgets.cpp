#include "ItemWidgets.h"
#include "BufferCardDelegate.h"
#include "../domain/Preview.h"
#include "../media/BlobStore.h"
#include "../media/ClipboardContent.h"
#include "../media/Thumbnailer.h"

#include <QAbstractTextDocumentLayout>
#include <QFile>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>

namespace napkin {
namespace {

constexpr int kImageThumb = 84;

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
        if (onPaste && onPaste(source)) return;
        QPlainTextEdit::insertFromMimeData(source);
    }
};

QColor dimmed(const QPalette& pal, int alpha)
{
    QColor c = pal.color(QPalette::Text);
    c.setAlpha(alpha);
    return c;
}

}  // namespace

// --- text --------------------------------------------------------------------

TextItemWidget::TextItemWidget(ItemId id, const QString& text, QWidget* parent)
    : QWidget(parent), id_(id)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* edit = new PasteAwareTextEdit;
    edit->setPlainText(text);
    edit->moveCursor(QTextCursor::End);
    edit->setFrameShape(QFrame::NoFrame);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setPlaceholderText(tr("Type or paste something…"));
    edit->setTabChangesFocus(true);
    edit->viewport()->setAutoFillBackground(false);
    edit->setStyleSheet(QStringLiteral("QPlainTextEdit { background: transparent; }"));
    edit->onPaste = [this](const QMimeData* source) {
        const auto content = readClipboard(source);
        if (content.kind != ClipboardContent::Kind::Image) return false;
        emit imagePasted(content.imageBytes, content.imageMime);
        return true;
    };
    edit_ = edit;
    layout->addWidget(edit_);

    connect(edit_, &QPlainTextEdit::textChanged, this, [this] {
        dirty_ = true;
        emit edited();
        emit heightChanged();
    });

    edit_->installEventFilter(this);
}

QString TextItemWidget::text() const { return edit_->toPlainText(); }

void TextItemWidget::focusText()
{
    edit_->setFocus(Qt::OtherFocusReason);
    edit_->moveCursor(QTextCursor::End);
}

int TextItemWidget::desiredHeight() const
{
    const qreal doc = edit_->document()->documentLayout()->documentSize().height();
    return std::max(28, int(doc) + 8);
}

bool TextItemWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == edit_ && event->type() == QEvent::KeyPress) {
        // Esc collapses. Everything else belongs to the text — an editor that
        // swallows keys is worse than one that does too little.
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            emit collapseRequested();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// --- image -------------------------------------------------------------------

ImageItemWidget::ImageItemWidget(const Item& item, Thumbnailer& thumbs, BlobStore&,
                                 QWidget* parent)
    : QWidget(parent), id_(item.id)
{
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Double-click to view full size"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);

    auto* preview = new QLabel;
    preview->setFixedSize(kImageThumb, kImageThumb);
    preview->setAlignment(Qt::AlignCenter);

    const QPixmap thumb = thumbs.forBlob(item.blobHash, item.mime, kImageThumb * 2);
    if (thumb.isNull()) {
        // The blob is gone. Say so rather than showing an empty box — silently
        // blank content is indistinguishable from empty content (SPEC.md §8).
        preview->setText(tr("missing"));
        preview->setEnabled(false);
    } else {
        preview->setPixmap(thumb.scaled(preview->size(), Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation));
    }
    layout->addWidget(preview);

    auto* details = new QVBoxLayout;
    details->setSpacing(2);

    const QString name = item.sourceName.isEmpty()
        ? (item.animated ? tr("Animation") : tr("Image"))
        : item.sourceName;
    auto* title = new QLabel(name);
    title->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QStringList facts;
    if (item.width > 0 && item.height > 0)
        facts << QStringLiteral("%1 × %2").arg(item.width).arg(item.height);
    if (item.byteSize > 0) facts << formatBytes(item.byteSize);
    if (item.animated) facts << tr("animated");
    auto* meta = new QLabel(facts.join(QStringLiteral("  ·  ")));
    QPalette metaPal = meta->palette();
    metaPal.setColor(QPalette::WindowText, dimmed(palette(), 150));
    meta->setPalette(metaPal);

    details->addStretch();
    details->addWidget(title);
    details->addWidget(meta);
    details->addStretch();
    layout->addLayout(details, 1);

    remove_ = new QToolButton;
    remove_->setText(QStringLiteral("×"));
    remove_->setAutoRaise(true);
    remove_->setToolTip(tr("Remove this image from the buffer"));
    remove_->setAccessibleName(tr("Remove %1").arg(name));
    remove_->hide();   // revealed on hover; always reachable by keyboard
    remove_->setFocusPolicy(Qt::TabFocus);
    connect(remove_, &QToolButton::clicked, this, [this] { emit removeRequested(id_); });
    layout->addWidget(remove_, 0, Qt::AlignTop);

    setFocusPolicy(Qt::TabFocus);
    setAccessibleName(name);
    setAccessibleDescription(facts.join(QStringLiteral(", ")));
}

void ImageItemWidget::mouseDoubleClickEvent(QMouseEvent*) { emit activated(id_); }

void ImageItemWidget::enterEvent(QEnterEvent* e)
{
    remove_->show();
    QWidget::enterEvent(e);
}

void ImageItemWidget::leaveEvent(QEvent* e)
{
    if (!remove_->hasFocus()) remove_->hide();
    QWidget::leaveEvent(e);
}

void ImageItemWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor border = dimmed(palette(), hasFocus() ? 120 : 45);
    p.setPen(QPen(border, 1));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
}

}  // namespace napkin
