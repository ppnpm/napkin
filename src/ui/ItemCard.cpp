#include "ItemCard.h"
#include "../domain/Preview.h"
#include "../media/BlobStore.h"
#include "../media/ClipboardContent.h"
#include "../media/Thumbnailer.h"
#include "Tokens.h"

#include <QAbstractTextDocumentLayout>
#include <QFile>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QVBoxLayout>
#include <functional>

namespace napkin {
namespace {

using namespace tokens;

// "image/png" -> "PNG", for a pasted image that has no filename of its own.
QString formats_upper(const QString& mime)
{
    const int slash = mime.indexOf(QLatin1Char('/'));
    QString suffix = slash >= 0 ? mime.mid(slash + 1) : mime;
    if (suffix.startsWith(QLatin1String("svg"))) suffix = QStringLiteral("svg");
    return suffix.toUpper();
}
constexpr int kCardPadding = 10;

// QPlainTextEdit would otherwise drop an image and paste whatever text came
// alongside it — the exact inversion of the §4 preference order.
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
}

void ItemCard::setSelected(bool selected)
{
    if (selected_ == selected) return;
    selected_ = selected;
    update();
}

void ItemCard::mousePressEvent(QMouseEvent* e)
{
    emit selectRequested(item_.id, e->modifiers());
    // Accept it. QWidget::mousePressEvent ignores the event, which would let it
    // bubble up to the canvas — whose own handler clears the selection this
    // click just made.
    e->accept();
}

// A text item must not look like a form field, because it is not one — it is the
// content. So there is no border and no fill at rest; the block is just text on
// the paper. What makes a borderless block read as an *object* is the gutter
// rail: a 3px mark to the left, empty at rest, solid when the block is selected
// or being edited. Nothing else in the canvas uses that gutter.
void ItemCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPalette& pal = palette();
    const bool editing = hasEditFocus();

    const QRect body = rect().adjusted(kRailOffset, 0, 0, 0);
    QPainterPath path;
    path.addRoundedRect(QRectF(body).adjusted(0.5, 0.5, -0.5, -0.5),
                        kRadiusSelection, kRadiusSelection);

    // Editing deliberately has no fill: a wash behind text you are reading and
    // typing degrades it, and the rail, border and caret are three signals
    // already.
    if (selected_ && !editing)
        p.fillPath(path, highlight(pal, isLightTheme(pal) ? kFillSelectedLight
                                                          : kFillSelectedDark));
    else if (hovered_)
        p.fillPath(path, text(pal, 10));

    if (editing)        p.setPen(QPen(highlight(pal, kBorderEditing), 1));
    else if (selected_) p.setPen(QPen(highlight(pal, kBorderSelected), 1));
    else if (hovered_)  p.setPen(QPen(text(pal, kItemHover), 1));
    else                p.setPen(Qt::NoPen);
    if (p.pen() != Qt::NoPen) p.drawPath(path);

    // Each state changes exactly two things, never three.
    if (selected_ || editing || hovered_) {
        const QColor rail = (selected_ || editing) ? highlight(pal, 255)
                                                   : text(pal, kRailHover);
        QPainterPath railPath;
        railPath.addRoundedRect(QRectF(0, 3, kRailWidth, std::max(0, height() - 6)),
                                kRailWidth / 2.0, kRailWidth / 2.0);
        p.fillPath(railPath, rail);
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
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kRailOffset + kSelectionBleed, kCardPadding,
                               kSelectionBleed, kCardPadding);

    auto* edit = new PasteAwareTextEdit;
    edit->setPlainText(item.text);
    edit->moveCursor(QTextCursor::End);
    edit->setFrameShape(QFrame::NoFrame);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setPlaceholderText(tr("Write something…"));
    // The canvas is content and the list is chrome; two points of size is the
    // cheapest way to say so (SPEC.md §7: content must dominate).
    QFont body = scaled(edit->font(), 2.0);
    edit->setFont(body);
    QTextOption option = edit->document()->defaultTextOption();
    edit->document()->setDefaultTextOption(option);
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
    // Read-only until you ask to edit, so a click lands on the block rather than
    // in the text. The composer is born editable: it has nothing to select.
    edit_->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(edit_);

    connect(edit_, &QPlainTextEdit::textChanged, this, [this] {
        dirty_ = true;
        emit edited();
        emit heightChanged();
    });
    edit_->installEventFilter(this);
    edit_->viewport()->installEventFilter(this);
}

QString TextItemCard::text() const { return edit_->toPlainText(); }
bool TextItemCard::textHasFocus() const { return edit_->hasFocus(); }

bool TextItemCard::hasEditFocus() const
{
    return edit_->textInteractionFlags() & Qt::TextEditorInteraction;
}

void TextItemCard::focusText()
{
    edit_->setTextInteractionFlags(Qt::TextEditorInteraction);
    edit_->setFocus(Qt::OtherFocusReason);
    edit_->moveCursor(QTextCursor::End);
}

int TextItemCard::desiredHeight() const
{
    const qreal doc = edit_->document()->documentLayout()->documentSize().height();
    return std::max(34, int(doc)) + kCardPadding * 2 + 6;
}

void TextItemCard::beginEditing()
{
    if (edit_->textInteractionFlags() & Qt::TextEditorInteraction) return;
    edit_->setTextInteractionFlags(Qt::TextEditorInteraction);
    edit_->setFocus(Qt::MouseFocusReason);
    edit_->moveCursor(QTextCursor::End);
    emit editingStarted(itemId());
    update();
}

// The composer is born editable: there is nothing there to select.
void TextItemCard::focusTextInteraction()
{
    edit_->setTextInteractionFlags(Qt::TextEditorInteraction);
}

void TextItemCard::endEditing()
{
    if (isComposer()) return;   // the composer is always ready to be written in
    edit_->setTextInteractionFlags(Qt::NoTextInteraction);
    update();
}

void TextItemCard::mouseDoubleClickEvent(QMouseEvent* e)
{
    beginEditing();
    e->accept();
}

bool TextItemCard::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        beginEditing();
        return true;
    }
    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        // While read-only the press means "select me"; once editing, it belongs
        // to the caret.
        if (!(edit_->textInteractionFlags() & Qt::TextEditorInteraction)) {
            emit selectRequested(itemId(), mouse->modifiers());
            return true;
        }
    }
    if (watched == edit_ && event->type() == QEvent::KeyPress) {
        // Esc leaves the text and selects the block it belongs to, so a block
        // is always reachable as an object without a special hit target.
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
    : ItemCard(item, parent), thumbs_(thumbs)
{
    setToolTip(tr("Double-click to view full size"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kRailOffset + kSelectionBleed, kCardPadding,
                               kSelectionBleed, kCardPadding);
    layout->setSpacing(kGapTight);
    layout->setAlignment(Qt::AlignLeft);

    view_ = new QLabel;
    view_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    view_->setMinimumHeight(60);

    const QString path = blobs.pathFor(item.blobHash, item.mime);
    if (QFile::exists(path)) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        // Decode at a sane ceiling: the canvas never shows more than this, and
        // a 100-megapixel HEIC should not land in memory whole to be shrunk.
        if (const QSize full = reader.size(); full.isValid()) {
            QSize target = full;
            target.scale(2200, kImageMaxHeight * 2, Qt::KeepAspectRatio);
            if (target.width() < full.width()) reader.setScaledSize(target);
        }
        source_ = QPixmap::fromImage(reader.read());
    }
    if (source_.isNull()) {
        // Say so rather than showing an empty box: silently blank content is
        // indistinguishable from empty content.
        view_->setText(tr("This image is no longer on disk."));
        view_->setEnabled(false);
        view_->setMinimumHeight(120);
    }
    layout->addWidget(view_);

    // One tertiary line is not dense metadata; it is what a person who pastes
    // screenshots actually needs: which one is this, how big, can I use it.
    QStringList facts;
    if (!item.sourceName.isEmpty()) facts << item.sourceName;
    else facts << formats_upper(item.mime);
    if (item.width > 0 && item.height > 0)
        facts << QStringLiteral("%1 × %2").arg(item.width).arg(item.height);
    if (item.byteSize > 0) facts << formatBytes(item.byteSize);
    if (item.animated) facts << tr("animated");

    caption_ = new QLabel(facts.join(QStringLiteral(" · ")));
    QPalette pal = caption_->palette();
    pal.setColor(QPalette::WindowText, text(palette(), kTextTertiary));   // measured AA floor
    caption_->setPalette(pal);
    caption_->setFont(scaled(caption_->font(), -1.5));
    layout->addWidget(caption_);

    setAccessibleName(item.sourceName.isEmpty() ? tr("Image") : item.sourceName);
    setAccessibleDescription(facts.join(QStringLiteral(", ")));
    rescale();
}

QString ImageItemCard::asPlainText() const
{
    return item_.sourceName.isEmpty() ? tr("[image]") : item_.sourceName;
}

int ImageItemCard::desiredHeight() const
{
    if (source_.isNull()) return 190;
    const int available = std::max(200, width() - kRailOffset - kSelectionBleed * 2);
    const int drawn = source_.height() * std::min(available, source_.width())
                      / std::max(1, source_.width());
    return std::min(drawn, kImageMaxHeight) + caption_->sizeHint().height()
           + kCardPadding * 2 + kGapTight;
}

void ImageItemCard::rescale()
{
    if (source_.isNull()) return;
    const int available = std::max(80, width() - kRailOffset - kSelectionBleed * 2);
    // Never upscaled. A 200x140 favicon draws at 200x140; stretching a small
    // image to fill a column is the fastest way to make a UI look cheap.
    const QSize target = source_.size()
                             .scaled(available, kImageMaxHeight, Qt::KeepAspectRatio)
                             .boundedTo(source_.size());
    view_->setPixmap(source_.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    view_->setFixedHeight(target.height());
}

void ImageItemCard::resizeEvent(QResizeEvent* e)
{
    ItemCard::resizeEvent(e);
    rescale();
}

void ImageItemCard::mouseDoubleClickEvent(QMouseEvent*) { emit activated(itemId()); }

}  // namespace napkin
