#include "ItemCard.h"
#include "CardFooter.h"
#include "Tokens.h"
#include "../domain/Clock.h"
#include "../domain/Preview.h"
#include "../domain/TimeFormat.h"
#include "../media/BlobStore.h"
#include "../media/ClipboardContent.h"
#include "../media/ImageFormats.h"
#include "../media/Thumbnailer.h"

#include <QAbstractTextDocumentLayout>
#include <QFile>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextDocument>
#include <QVBoxLayout>
#include <functional>

namespace napkin {
namespace {

using namespace tokens;

// "image/png" -> "PNG", for a pasted image with no filename of its own.
QString formatLabel(const QString& mime)
{
    const int slash = mime.indexOf(QLatin1Char('/'));
    QString suffix = slash >= 0 ? mime.mid(slash + 1) : mime;
    if (suffix.startsWith(QLatin1String("svg"))) suffix = QStringLiteral("svg");
    return suffix.toUpper();
}

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

}  // namespace

// --- base --------------------------------------------------------------------

ItemCard::ItemCard(const Item& item, QWidget* parent) : QWidget(parent), item_(item)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);
}

void ItemCard::setContent(QWidget* content, const QString& copyLabel)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kCardPad, kCardPad, kCardPad, kCardPad - 6);
    layout->setSpacing(kGapTight);
    layout->addWidget(content, 1);

    if (!isComposer()) {
        footer_ = new CardFooter(copyLabel, this);
        footer_->setTimestamp(item_.modifiedAt ? item_.modifiedAt : item_.createdAt);
        connect(footer_, &CardFooter::actionTriggered, this,
                [this] { emit copyRequested(item_.id); });
        layout->addWidget(footer_);
    }
}

void ItemCard::setSelected(bool selected)
{
    if (selected_ == selected) return;
    selected_ = selected;
    update();
}

void ItemCard::setClipped(bool clipped)
{
    clipped_ = clipped;
    if (footer_) footer_->setClipped(clipped);
}

void ItemCard::setCurrent(bool current)
{
    if (current_ == current) return;
    current_ = current;
    update();
}

int ItemCard::chromeHeight() const
{
    return kCardPad * 2 - 6 + (footer_ ? kCardFooterH + kGapTight : 0);
}

int ItemCard::heightForColumn(int width) const
{
    const int inner = std::max(40, width - kCardPad * 2);
    const int natural = contentHeightForWidth(inner) + chromeHeight();

    clipped_ = natural > kCardMaxHeight;
    if (footer_) footer_->setClipped(clipped_);
    return std::clamp(natural, kCardMinHeight, kCardMaxHeight);
}

void ItemCard::mousePressEvent(QMouseEvent* e)
{
    emit selectRequested(item_.id, e->modifiers());
    // Accept it. QWidget::mousePressEvent ignores the event, which would let it
    // bubble up to the canvas — whose handler clears the selection this click
    // just made.
    e->accept();
}

void ItemCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPalette& pal = palette();
    const bool editing = hasEditFocus();

    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(box, kCardRadius, kCardRadius);

    // A card is a piece of paper on the desk: its own surface, its own edge.
    // An earlier build drew text cards with no border and no fill at all, which
    // made the board read as loose text rather than as things you can pick up.
    p.fillPath(path, pal.color(QPalette::Base));

    // Selection tints the card; EDITING deliberately does not. A wash behind
    // text you are actively reading and typing makes it hard to read, and the
    // border plus the caret are two signals already.
    if (selected_ && !editing)
        p.fillPath(path, highlight(pal, isLightTheme(pal) ? 20 : 34));

    // The accent is darkened until it actually meets 3:1 against the card. The
    // raw Highlight at alpha 160 measured 1.74:1 in Breeze Light — FAINTER than
    // the 3.10:1 resting border it replaced, so selecting a card made its edge
    // harder to see and the state ended up carried by hue alone.
    const QColor border = selected_ || editing
        ? readableAccent(pal, editing ? 1.0 : 0.82)
        : text(pal, cardBorderAlpha(pal, hovered_));
    p.setPen(QPen(border, selected_ || editing ? 2.0 : 1.0));
    p.drawPath(path);

    // Keyboard focus is its own signal, drawn inside the border so it never
    // collides with it. Without this a focused card was pixel-identical to its
    // neighbours and the board could not be operated by keyboard at all.
    if (current_) {
        QPainterPath ring;
        ring.addRoundedRect(box.adjusted(3, 3, -3, -3), kCardRadius - 3, kCardRadius - 3);
        QPen focusPen(readableAccent(pal, 1.0), 2.0, Qt::DotLine);
        p.setPen(focusPen);
        p.drawPath(ring);
    }

}

void ItemCard::enterEvent(QEnterEvent* e)
{
    hovered_ = true;
    update();
    QWidget::enterEvent(e);
}

void ItemCard::leaveEvent(QEvent* e)
{
    hovered_ = false;
    update();
    QWidget::leaveEvent(e);
}

// --- text --------------------------------------------------------------------

TextItemCard::TextItemCard(const Item& item, QWidget* parent) : ItemCard(item, parent)
{
    auto* edit = new PasteAwareTextEdit;
    edit->setPlainText(item.text);
    // Start at the beginning, not the end. moveCursor(End) here left the
    // viewport scrolled — horizontally as well as vertically — so the first few
    // pixels of every line were clipped off the left edge. A card is read from
    // the top anyway; the caret only needs to be at the end when you are about
    // to append to it.
    edit->moveCursor(QTextCursor::Start);
    edit->setFrameShape(QFrame::NoFrame);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setPlaceholderText(tr("Write something…"));
    edit->setTabChangesFocus(true);
    edit->viewport()->setAutoFillBackground(false);
    edit->setStyleSheet(QStringLiteral("QPlainTextEdit { background: transparent; }"));
    edit->document()->setDocumentMargin(1);
    // Read-only until you ask to edit, so a click lands on the card rather than
    // in the text. The composer is born editable: it has nothing to select.
    edit->setTextInteractionFlags(Qt::NoTextInteraction);
    edit->onPaste = [this](const QMimeData* source) {
        const auto content = readClipboard(source);
        if (content.kind != ClipboardContent::Kind::Image) return false;
        emit imagePasted(content.imageBytes, content.imageMime);
        return true;
    };
    edit_ = edit;

    // Belt and braces: with the horizontal bar off, a stray scroll offset is
    // invisible but still clips the text.
    edit_->horizontalScrollBar()->setValue(0);
    edit_->verticalScrollBar()->setValue(0);

    setContent(edit_, tr("Copy text"));

    connect(edit_, &QPlainTextEdit::textChanged, this, [this] {
        dirty_ = true;
        updateAccessibleName();
        emit edited();
        emit heightChanged();
    });
    updateAccessibleName();
    edit_->installEventFilter(this);
    edit_->viewport()->installEventFilter(this);
}

QString TextItemCard::text() const { return edit_->toPlainText(); }

// A screen reader gets the same thing a sighted user does: the first line, and
// when it happened. Six of nine cards announced nothing at all before this.
void TextItemCard::updateAccessibleName()
{
    const QString body = firstLine(edit_->toPlainText());
    setAccessibleName(body.isEmpty() ? tr("Empty note") : body.left(80));
    const Timestamp when = item_.modifiedAt ? item_.modifiedAt : item_.createdAt;
    setAccessibleDescription(when ? tr("Note, %1").arg(relativeTime(when, nowMs()))
                                  : tr("New note"));
}
bool TextItemCard::textHasFocus() const { return edit_->hasFocus(); }

// Editing MODE, not window focus: a card being edited must still look edited
// when the window is inactive, and window focus is not something a headless
// test can grant.
bool TextItemCard::hasEditFocus() const
{
    return edit_->textInteractionFlags() & Qt::TextEditorInteraction;
}

void TextItemCard::focusTextInteraction()
{
    edit_->setTextInteractionFlags(Qt::TextEditorInteraction);
}

void TextItemCard::focusText()
{
    focusTextInteraction();
    edit_->setFocus(Qt::OtherFocusReason);
    edit_->moveCursor(QTextCursor::End);
}

void TextItemCard::beginEditing(bool moveToEnd)
{
    if (hasEditFocus()) return;
    focusTextInteraction();
    edit_->setFocus(Qt::MouseFocusReason);
    if (moveToEnd) edit_->moveCursor(QTextCursor::End);
    emit editingStarted(itemId());
    update();
}

void TextItemCard::endEditing()
{
    if (isComposer()) return;   // the composer is always ready to be written in
    edit_->setTextInteractionFlags(Qt::NoTextInteraction);
    update();
}

int TextItemCard::contentHeightForWidth(int innerWidth) const
{
    // Measured against a document of our own, never the editor's.
    //
    // QPlainTextEdit uses QPlainTextDocumentLayout, which ignores setTextWidth
    // and wraps to the *viewport's* current width instead — and reports its
    // height in LINES rather than pixels. A card measured at construction, when
    // the viewport has no width yet, therefore came back as one line. That is
    // why a freshly pasted paragraph appeared as a single scrollable line.
    //
    // A plain QTextDocument honours setTextWidth and answers in pixels, so it
    // gives the right height before the widget has ever been shown.
    if (!measure_) {
        measure_ = new QTextDocument(const_cast<TextItemCard*>(this));
        measure_->setDocumentMargin(1);   // match the editor's, or heights drift
    }
    const QString current = edit_->toPlainText();
    if (current != measured_) {
        measure_->setDefaultFont(edit_->font());
        measure_->setPlainText(current.isEmpty() ? edit_->placeholderText() : current);
        measured_ = current;
    }
    measure_->setTextWidth(innerWidth);
    return int(std::ceil(measure_->size().height())) + 2;
}

void TextItemCard::mouseDoubleClickEvent(QMouseEvent* e)
{
    beginEditing();
    e->accept();
}

void TextItemCard::selectAllText()
{
    edit_->selectAll();
}

bool TextItemCard::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        // Turn interaction on, then let the editor handle the click itself, so
        // the caret lands on the word you double-clicked instead of jumping to
        // the end of the text.
        const bool wasReadOnly = !hasEditFocus();
        beginEditing(/*moveToEnd=*/false);
        return !wasReadOnly ? false : (edit_->setFocus(Qt::MouseFocusReason), false);
    }
    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        // While read-only the press means "select me"; once editing, it belongs
        // to the caret.
        if (!hasEditFocus()) {
            emit selectRequested(itemId(), mouse->modifiers());
            return true;
        }
    }
    if (watched == edit_ && event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            endEditing();
            emit escaped();
            return true;
        }
    }
    return ItemCard::eventFilter(watched, event);
}

// --- image -------------------------------------------------------------------

ImageItemCard::ImageItemCard(const Item& item, Thumbnailer& thumbs, BlobStore& blobs,
                             QWidget* parent)
    : ItemCard(item, parent)
{
    setToolTip(tr("Double-click to view full size"));

    auto* holder = new QWidget;
    auto* layout = new QVBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    view_ = new QLabel;
    view_->setAlignment(Qt::AlignCenter);

    // Through the Thumbnailer, which caches to disk and negative-caches
    // failures. The card used to decode the original blob itself, on the UI
    // thread, in its constructor: Qt's PNG handler ignores setScaledSize and
    // decodes in full, so a 208 KB 8000x8000 screenshot cost 288 MB and 410 ms
    // — per card, every time the buffer was opened. The Thumbnailer pays that
    // once, then reads a small cached file.
    const bool fileExists = QFile::exists(blobs.pathFor(item.blobHash, item.mime));
    if (fileExists)
        source_ = thumbs.forBlob(item.blobHash, item.mime, kCardMaxWidth * 2);

    if (source_.isNull()) {
        // Two different failures, and telling them apart matters: one means the
        // file is gone, the other means it is right there and unreadable. The
        // card used to claim "no longer on disk" for both.
        view_->setText(fileExists ? tr("This image is too large to display.")
                                  : tr("This image is no longer on disk."));
        view_->setEnabled(false);
    }
    layout->addWidget(view_);

    QStringList facts;
    facts << (item.sourceName.isEmpty() ? formatLabel(item.mime) : item.sourceName);
    if (item.width > 0 && item.height > 0)
        facts << QStringLiteral("%1 × %2").arg(item.width).arg(item.height);
    if (item.byteSize > 0) facts << formatBytes(item.byteSize);
    if (item.animated) facts << tr("animated");

    caption_ = new QLabel(facts.join(QStringLiteral(" · ")));
    caption_->setFont(scaled(caption_->font(), -1.5));
    QPalette capPal = caption_->palette();
    capPal.setColor(QPalette::WindowText, text(palette(), kTextTertiary));
    caption_->setPalette(capPal);
    layout->addWidget(caption_);

    setContent(holder, tr("Copy image"));
    setAccessibleName(item.sourceName.isEmpty() ? tr("Image") : item.sourceName);
    setAccessibleDescription(facts.join(QStringLiteral(", ")));
}

QString ImageItemCard::asPlainText() const
{
    return item_.sourceName.isEmpty() ? tr("[image]") : item_.sourceName;
}

int ImageItemCard::contentHeightForWidth(int innerWidth) const
{
    const int captionH = caption_ ? caption_->sizeHint().height() + 6 : 0;
    if (source_.isNull()) return 96 + captionH;

    // Never upscaled: a 200x140 favicon draws at 200x140. Stretching a small
    // image to fill a column is the fastest way to make a UI look cheap.
    const int drawn = source_.height() * std::min(innerWidth, source_.width())
                      / std::max(1, source_.width());
    return drawn + captionH;
}

void ImageItemCard::rescale()
{
    if (source_.isNull()) return;
    // Must use the same chrome arithmetic as heightForColumn, or the caption
    // ends up painted over the bottom of the picture.
    const int available = std::max(60, width() - kCardPad * 2);
    const int captionH = caption_ ? caption_->sizeHint().height() + 6 : 0;
    const int room = std::max(60, height() - chromeHeight() - captionH);

    const QSize target = source_.size().scaled(available, room, Qt::KeepAspectRatio)
                             .boundedTo(source_.size());
    view_->setPixmap(source_.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ImageItemCard::resizeEvent(QResizeEvent* e)
{
    ItemCard::resizeEvent(e);
    rescale();
}

void ImageItemCard::mouseDoubleClickEvent(QMouseEvent*) { emit activated(itemId()); }

}  // namespace napkin
