#include "ItemCanvas.h"
#include "ItemCard.h"
#include "../media/BlobStore.h"
#include "MasonryLayout.h"
#include "Tokens.h"

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
using namespace tokens;
}  // namespace

ItemCanvas::ItemCanvas(Thumbnailer& thumbs, BlobStore& blobs, QWidget* parent)
    : QScrollArea(parent), thumbs_(thumbs), blobs_(blobs)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    // The board is the desk; the cards are paper on it. Cards carry their own
    // Base fill and an edge, so the surface behind them has to differ or they
    // have nothing to sit against.
    setBackgroundRole(QPalette::Window);
    viewport()->setAutoFillBackground(true);
    viewport()->setBackgroundRole(QPalette::Window);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    body_ = new QWidget;
    layout_ = new MasonryLayout(body_);
    layout_->setContentsMargins(kPadX, kPadTop, kPadX, kPadTop);
    layout_->setColumnWidth(kCardMinWidth, kCardMaxWidth);
    layout_->setSpacingBetween(kGapTight * 2);
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

void ItemCanvas::showEmptyBuffer()
{
    clearItems();
    // Napkin is temporary storage, not an editor. An empty buffer is waiting to
    // be pasted into, so it says that rather than offering a blank page.
    placeholder_->setText(tr("Nothing here yet.\n\nPaste with Ctrl+V, or press Ctrl+T "
                             "to write something."));
    placeholder_->setParent(body_);
    placeholder_->show();
    layout_->addWidget(placeholder_);
    QPalette pal = placeholder_->palette();
    pal.setColor(QPalette::WindowText, text(pal, kTextTertiary));
    placeholder_->setPalette(pal);
}

void ItemCanvas::showNothingSelected()
{
    clearItems();
    placeholder_->setText(tr("Select a buffer to see what is in it."));
    QPalette pal = placeholder_->palette();
    pal.setColor(QPalette::WindowText, text(pal, kTextTertiary));
    placeholder_->setPalette(pal);
    placeholder_->setParent(body_);
    placeholder_->show();
    layout_->addWidget(placeholder_);
}

void ItemCanvas::addCard(ItemCard* card, int index)
{
    connect(card, &ItemCard::copyRequested, this, [this](ItemId id) {
        // One-click copy of exactly this card, independent of the selection.
        const auto keep = selected_;
        selected_ = {id};
        copySelection();
        selected_ = keep;
    });
    if (auto* text = qobject_cast<TextItemCard*>(card)) {
        // Only one block edits at a time: starting one ends the others, so the
        // canvas never has two carets or an ambiguous Ctrl+C.
        connect(text, &TextItemCard::editingStarted, this, [this](ItemId id) {
            for (auto* other : textCards_)
                if (other->itemId() != id) other->endEditing();
            clearSelection();
        });
    }
    connect(card, &ItemCard::selectRequested, this, &ItemCanvas::applySelection);
    connect(card, &ItemCard::activated, this, &ItemCanvas::imageActivated);
    connect(card, &ItemCard::escaped, this, [this, card] {
        selected_ = {card->itemId()};
        for (auto* c : cards_) c->setSelected(c->itemId() == card->itemId());
        setFocus(Qt::OtherFocusReason);
        emit selectionChanged();
    });
    if (index < 0) {
        layout_->addWidget(card);
        cards_.push_back(card);
    } else {
        layout_->insertWidgetAt(index, card);
        cards_.insert(cards_.begin() + std::min(size_t(index), cards_.size()), card);
    }
}

// Ctrl+T. One unwritten card at the top, focused, which becomes a real item the
// moment it has content and evaporates if it never does (invariant 5).
void ItemCanvas::addPendingTextCard()
{
    for (auto* existing : textCards_)
        if (existing->isComposer()) { existing->focusText(); return; }

    Item blank;
    blank.type = ItemType::Text;
    auto* card = new TextItemCard(blank, body_);
    card->focusTextInteraction();
    connect(card, &TextItemCard::edited, this, &ItemCanvas::edited);
    connect(card, &TextItemCard::imagePasted, this, &ItemCanvas::imagePasted);
    connect(card, &TextItemCard::heightChanged, this, &ItemCanvas::relayout);
    placeholder_->hide();
    addCard(card, 0);                       // newest first, from the moment it exists
    textCards_.insert(textCards_.begin(), card);
    relayout();
    card->focusText();
}

int ItemCanvas::indexOf(ItemId id) const
{
    for (size_t i = 0; i < cards_.size(); ++i)
        if (cards_[i]->itemId() == id) return int(i);
    return -1;
}

QList<ItemId> ItemCanvas::itemOrder() const
{
    QList<ItemId> out;
    for (auto* card : cards_) out << card->itemId();
    return out;
}

void ItemCanvas::setItems(const std::vector<Item>& items, int selectIndex)
{
    clearItems();
    if (items.empty()) { showEmptyBuffer(); return; }
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
    relayout();

    if (selectIndex < 0 || cards_.empty()) return;
    // Clamp: deleting the last card should land on the new last, not nowhere.
    const int target = std::min(selectIndex, int(cards_.size()) - 1);
    if (target >= 0 && target < int(cards_.size())
        && cards_[size_t(target)]->itemId() != kNoItem) {
        selected_ = {cards_[size_t(target)]->itemId()};
        anchor_ = *selected_.begin();
        cards_[size_t(target)]->setSelected(true);
        emit selectionChanged();
    }
}

void ItemCanvas::relayout()
{
    const int column = layout_->columnWidth(viewport()->width());
    for (auto* card : cards_) {
        card->setFixedWidth(column);
        card->setFixedHeight(card->heightForColumn(column));
    }
    layout_->invalidate();
    body_->adjustSize();
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
        // Uniform: a single click selects, whatever the item is.
        selected_ = {id};
        anchor_ = id;
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
    // Enter on a single selected text block starts editing it, the keyboard
    // equivalent of the double-click.
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
        && selected_.size() == 1) {
        for (auto* card : textCards_)
            if (card->itemId() == *selected_.begin()) { card->beginEditing(); return; }
    }
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



bool ItemCanvas::textHasFocus() const
{
    for (auto* card : textCards_) if (card->textHasFocus()) return true;
    return false;
}

}  // namespace napkin
