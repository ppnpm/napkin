#include "ItemCard.h"
#include "../domain/Preview.h"
#include "../media/BlobStore.h"
#include "../media/ClipboardContent.h"
#include "../media/Thumbnailer.h"

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

constexpr int kCardRadius   = 8;
constexpr int kCardPadding  = 14;
constexpr int kImageMaxHigh = 420;   // a screenshot gets room; a canvas is not a gallery

QColor dim(const QPalette& pal, int alpha)
{
    QColor c = pal.color(QPalette::Text);
    c.setAlpha(alpha);
    return c;
}

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

void ItemCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QPainterPath path;
    path.addRoundedRect(box, kCardRadius, kCardRadius);

    if (selected_) {
        QColor tint = palette().color(QPalette::Highlight);
        tint.setAlpha(28);
        p.fillPath(path, tint);
    }

    // 128 alpha is the measured 3:1 floor for a non-text affordance; a resting
    // border below that makes the block invisible as an object.
    QColor border = selected_ ? palette().color(QPalette::Highlight) : dim(palette(), 128);
    p.setPen(QPen(border, selected_ ? 2.0 : 1.0));
    p.drawPath(path);
}

// --- text --------------------------------------------------------------------

TextItemCard::TextItemCard(const Item& item, QWidget* parent) : ItemCard(item, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kCardPadding, kCardPadding - 2, kCardPadding, kCardPadding - 2);

    auto* edit = new PasteAwareTextEdit;
    edit->setPlainText(item.text);
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
    edit_->viewport()->installEventFilter(this);
}

QString TextItemCard::text() const { return edit_->toPlainText(); }
bool TextItemCard::textHasFocus() const { return edit_->hasFocus(); }

void TextItemCard::focusText()
{
    edit_->setFocus(Qt::OtherFocusReason);
    edit_->moveCursor(QTextCursor::End);
}

int TextItemCard::desiredHeight() const
{
    const qreal doc = edit_->document()->documentLayout()->documentSize().height();
    return std::max(34, int(doc)) + (kCardPadding - 2) * 2 + 6;
}

bool TextItemCard::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        // A modified click means "select this block", not "put the caret here".
        if (mouse->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) {
            emit selectRequested(itemId(), mouse->modifiers());
            return true;
        }
        emit selectRequested(itemId(), Qt::NoModifier);
    }
    if (watched == edit_ && event->type() == QEvent::KeyPress) {
        // Esc leaves the text and selects the block it belongs to, so a block
        // is always reachable as an object without a special hit target.
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
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
    layout->setContentsMargins(kCardPadding, kCardPadding, kCardPadding, kCardPadding - 4);
    layout->setSpacing(8);

    view_ = new QLabel;
    view_->setAlignment(Qt::AlignCenter);
    view_->setMinimumHeight(80);

    const QString path = blobs.pathFor(item.blobHash, item.mime);
    if (QFile::exists(path)) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        // Decode at a sane ceiling: the canvas never shows more than this, and
        // a 100-megapixel HEIC should not land in memory whole to be shrunk.
        if (const QSize full = reader.size(); full.isValid()) {
            QSize target = full;
            target.scale(2200, kImageMaxHigh * 2, Qt::KeepAspectRatio);
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

    QStringList facts;
    if (!item.sourceName.isEmpty()) facts << item.sourceName;
    if (item.width > 0 && item.height > 0)
        facts << QStringLiteral("%1 × %2").arg(item.width).arg(item.height);
    if (item.byteSize > 0) facts << formatBytes(item.byteSize);
    if (item.animated) facts << tr("animated");

    caption_ = new QLabel(facts.join(QStringLiteral("   ·   ")));
    QPalette pal = caption_->palette();
    pal.setColor(QPalette::WindowText, dim(palette(), 161));   // measured AA floor
    caption_->setPalette(pal);
    QFont cf = caption_->font();
    cf.setPointSizeF(std::max(7.5, cf.pointSizeF() - 1.0));
    caption_->setFont(cf);
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
    const int available = std::max(200, width() - kCardPadding * 2);
    const int scaled = source_.height() * available / std::max(1, source_.width());
    return std::min(scaled, kImageMaxHigh) + caption_->sizeHint().height() + kCardPadding * 2 + 6;
}

void ImageItemCard::rescale()
{
    if (source_.isNull()) return;
    const int available = std::max(80, width() - kCardPadding * 2);
    // Fit to the pane width but never upscale: a blurry enlargement of a small
    // screenshot is worse than honest small.
    QSize target = source_.size().scaled(available, kImageMaxHigh, Qt::KeepAspectRatio)
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
