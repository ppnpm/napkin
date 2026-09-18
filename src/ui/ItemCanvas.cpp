#include "ItemCanvas.h"
#include "ItemCard.h"
#include "../media/BlobStore.h"

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QUrl>
#include <QMouseEvent>
#include <QScrollBar>
#include <QVBoxLayout>

namespace napkin {
namespace {
constexpr int kGutter  = 20;
constexpr int kSpacing = 12;
}  // namespace

ItemCanvas::ItemCanvas(Thumbnailer& thumbs, BlobStore& blobs, QWidget* parent)
    : QScrollArea(parent), thumbs_(thumbs), blobs_(blobs)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    body_ = new QWidget;
    layout_ = new QVBoxLayout(body_);
    layout_->setContentsMargins(kGutter, kGutter, kGutter, kGutter);
    layout_->setSpacing(kSpacing);
    setWidget(body_);

    placeholder_ = new QLabel;
    placeholder_->setAlignment(Qt::AlignCenter);
    placeholder_->setWordWrap(true);
    showNothingSelected();
}

void ItemCanvas::clearItems()
{
    cards_.clear();
    textCards_.clear();
    selected_.clear();
    anchor_ = kNoItem;
    while (QLayoutItem* child = layout_->takeAt(0)) {
        if (QWidget* w = child->widget()) {
            // Reparenting before deleteLater() takes the widget out of the tree
            // now; deferring alone leaves removed items live and drawn until the
            // next event-loop turn.
            if (w != placeholder_) { w->hide(); w->setParent(nullptr); w->deleteLater(); }
            else { w->hide(); }
        }
        delete child;
    }
}

void ItemCanvas::showNothingSelected()
{
    clearItems();
    placeholder_->setText(tr("Select a buffer to see what is in it."));
    placeholder_->setParent(body_);
    placeholder_->show();
    layout_->addWidget(placeholder_, 1);
}

void ItemCanvas::addCard(ItemCard* card)
{
    connect(card, &ItemCard::selectRequested, this, &ItemCanvas::applySelection);
    connect(card, &ItemCard::activated, this, &ItemCanvas::imageActivated);
    connect(card, &ItemCard::escaped, this, [this, card] {
        selected_ = {card->itemId()};
        for (auto* c : cards_) c->setSelected(c->itemId() == card->itemId());
        setFocus(Qt::OtherFocusReason);
        emit selectionChanged();
    });
    layout_->addWidget(card);
    cards_.push_back(card);
}

// There is always somewhere to type at the end. It costs nothing until it has
// content (invariant 5) and it is how a new text block gets added at all.
void ItemCanvas::addComposer()
{
    if (!textCards_.empty() && textCards_.back()->text().trimmed().isEmpty()) return;
    Item blank;
    blank.type = ItemType::Text;
    auto* card = new TextItemCard(blank, body_);
    connect(card, &TextItemCard::edited, this, &ItemCanvas::edited);
    connect(card, &TextItemCard::imagePasted, this, &ItemCanvas::imagePasted);
    connect(card, &TextItemCard::heightChanged, this, &ItemCanvas::relayout);
    addCard(card);
    textCards_.push_back(card);
}

void ItemCanvas::setItems(const std::vector<Item>& items)
{
    clearItems();
    placeholder_->hide();

    for (const auto& item : items) {
        if (item.type == ItemType::Text) {
            auto* card = new TextItemCard(item, body_);
            connect(card, &TextItemCard::edited, this, &ItemCanvas::edited);
            connect(card, &TextItemCard::imagePasted, this, &ItemCanvas::imagePasted);
            connect(card, &TextItemCard::heightChanged, this, &ItemCanvas::relayout);
            addCard(card);
            textCards_.push_back(card);
        } else {
            addCard(new ImageItemCard(item, thumbs_, blobs_, body_));
        }
    }
    addComposer();
    layout_->addStretch();
    relayout();
}

void ItemCanvas::relayout()
{
    for (auto* card : cards_) {
        if (auto* text = qobject_cast<TextItemCard*>(card))
            text->setFixedHeight(text->desiredHeight());
        else if (auto* image = qobject_cast<ImageItemCard*>(card))
            image->setFixedHeight(image->desiredHeight());
    }
}

void ItemCanvas::resizeEvent(QResizeEvent* e)
{
    QScrollArea::resizeEvent(e);
    relayout();
}

void ItemCanvas::applySelection(ItemId id, Qt::KeyboardModifiers modifiers)
{
    if (id == kNoItem) return;   // the unwritten composer is not a selectable object

    if (modifiers & Qt::ControlModifier) {
        if (selected_.contains(id)) selected_.remove(id);
        else { selected_.insert(id); anchor_ = id; }
    } else if ((modifiers & Qt::ShiftModifier) && anchor_ != kNoItem) {
        int from = -1, to = -1;
        for (size_t i = 0; i < cards_.size(); ++i) {
            if (cards_[i]->itemId() == anchor_) from = int(i);
            if (cards_[i]->itemId() == id) to = int(i);
        }
        if (from >= 0 && to >= 0) {
            selected_.clear();
            for (int i = std::min(from, to); i <= std::max(from, to); ++i)
                if (cards_[size_t(i)]->itemId() != kNoItem)
                    selected_.insert(cards_[size_t(i)]->itemId());
        }
    } else {
        selected_.clear();
        anchor_ = id;
        // A bare click on an IMAGE selects it — an image is not editable, so
        // there is no caret the click could mean instead. A bare click on text
        // places the caret; that block is the anchor but not a selection the
        // verbs act on, because Ctrl+C there should copy the selected words.
        for (auto* card : cards_)
            if (card->itemId() == id && !qobject_cast<TextItemCard*>(card))
                selected_.insert(id);
    }

    for (auto* card : cards_) card->setSelected(selected_.contains(card->itemId()));
    emit selectionChanged();
}

void ItemCanvas::mousePressEvent(QMouseEvent* e)
{
    clearSelection();
    QScrollArea::mousePressEvent(e);
}

void ItemCanvas::clearSelection()
{
    if (selected_.isEmpty()) return;
    selected_.clear();
    for (auto* card : cards_) card->setSelected(false);
    emit selectionChanged();
}

void ItemCanvas::selectAll()
{
    selected_.clear();
    for (auto* card : cards_)
        if (card->itemId() != kNoItem) selected_.insert(card->itemId());
    for (auto* card : cards_) card->setSelected(selected_.contains(card->itemId()));
    emit selectionChanged();
}

QList<ItemId> ItemCanvas::selection() const
{
    // In document order, so a copied multi-selection reads the way it looked.
    QList<ItemId> out;
    for (auto* card : cards_)
        if (selected_.contains(card->itemId())) out << card->itemId();
    return out;
}

void ItemCanvas::copySelection() const
{
    const auto ids = selection();
    if (ids.isEmpty()) return;

    // A single image goes to the clipboard as an image, so it can be pasted
    // into anything. Anything else goes as text, joined in document order.
    if (ids.size() == 1) {
        for (auto* card : cards_) {
            if (card->itemId() != ids.first()) continue;
            if (card->item().type == ItemType::Image) {
                const QString path = blobs_.pathFor(card->item().blobHash, card->item().mime);
                QImage image(path);
                if (!image.isNull()) {
                    auto* mime = new QMimeData;
                    mime->setImageData(image);
                    mime->setUrls({QUrl::fromLocalFile(path)});
                    QApplication::clipboard()->setMimeData(mime);
                    return;
                }
            }
        }
    }

    QStringList parts;
    for (auto* card : cards_)
        if (selected_.contains(card->itemId())) parts << card->asPlainText();
    QApplication::clipboard()->setText(parts.join(QStringLiteral("\n\n")));
}

void ItemCanvas::cutSelection()
{
    if (selected_.isEmpty()) return;
    copySelection();
    deleteSelection();
}

void ItemCanvas::deleteSelection()
{
    const auto ids = selection();
    if (ids.isEmpty()) return;
    emit removeRequested(ids);
}

void ItemCanvas::keyPressEvent(QKeyEvent* e)
{
    // These only fire when the canvas itself has focus, not while a caret is in
    // a text block, so they can never eat a keystroke meant for the text.
    if (e->matches(QKeySequence::Copy))      { copySelection(); return; }
    if (e->matches(QKeySequence::Cut))       { cutSelection(); return; }
    if (e->matches(QKeySequence::SelectAll)) { selectAll(); return; }
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        deleteSelection();
        return;
    }
    if (e->key() == Qt::Key_Escape) { clearSelection(); return; }
    QScrollArea::keyPressEvent(e);
}

std::vector<ItemCanvas::DirtyText> ItemCanvas::dirtyText() const
{
    std::vector<DirtyText> out;
    for (auto* card : textCards_)
        if (card->isDirty()) out.push_back({card->itemId(), card->text()});
    return out;
}

void ItemCanvas::markClean()
{
    for (auto* card : textCards_) card->markClean();
}

bool ItemCanvas::rebindTextIds(const std::vector<Item>& items)
{
    std::vector<ItemId> ids;
    for (const auto& item : items)
        if (item.type == ItemType::Text) ids.push_back(item.id);
    if (ids.size() > textCards_.size()) return false;
    for (size_t i = 0; i < ids.size(); ++i) textCards_[i]->setItemId(ids[i]);
    return true;
}

void ItemCanvas::focusComposer()
{
    if (!textCards_.empty()) textCards_.back()->focusText();
}

bool ItemCanvas::textHasFocus() const
{
    for (auto* card : textCards_) if (card->textHasFocus()) return true;
    return false;
}

}  // namespace napkin
