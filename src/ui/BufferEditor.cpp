#include "BufferEditor.h"
#include "ItemWidgets.h"

#include <QScrollArea>
#include <QVBoxLayout>

namespace napkin {

BufferEditor::BufferEditor(Thumbnailer& thumbs, BlobStore& blobs, QWidget* parent)
    : QWidget(parent), thumbs_(thumbs), blobs_(blobs)
{
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(8);
}

void BufferEditor::clear()
{
    textWidgets_.clear();
    while (QLayoutItem* child = layout_->takeAt(0)) {
        if (QWidget* w = child->widget()) {
            // Reparenting before deleteLater() takes the widget out of the tree
            // now. deleteLater() alone defers destruction to the next event-loop
            // turn, during which a removed image would still be a live child —
            // still findable, still able to receive events, still drawn.
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete child;
    }
}

void BufferEditor::addTextItem(ItemId id, const QString& text)
{
    auto* widget = new TextItemWidget(id, text, this);
    connect(widget, &TextItemWidget::edited, this, &BufferEditor::edited);
    connect(widget, &TextItemWidget::imagePasted, this, &BufferEditor::imagePasted);
    connect(widget, &TextItemWidget::collapseRequested, this, &BufferEditor::collapseRequested);
    connect(widget, &TextItemWidget::heightChanged, this, &BufferEditor::heightChanged);
    layout_->addWidget(widget);
    textWidgets_.push_back(widget);
}

// A buffer you can only read is not a scratch surface. There is always somewhere
// to type at the end, and it costs nothing until it has content (invariant 5).
void BufferEditor::addTrailingComposer()
{
    if (!textWidgets_.empty() && textWidgets_.back()->text().trimmed().isEmpty()) return;
    addTextItem(kNoItem, QString());
}

void BufferEditor::setItems(const std::vector<Item>& items)
{
    clear();

    for (const auto& item : items) {
        if (item.type == ItemType::Text) {
            addTextItem(item.id, item.text);
            continue;
        }
        auto* image = new ImageItemWidget(item, thumbs_, blobs_, this);
        connect(image, &ImageItemWidget::activated, this, &BufferEditor::imageActivated);
        connect(image, &ImageItemWidget::removeRequested, this,
                &BufferEditor::itemRemoveRequested);
        layout_->addWidget(image);
    }

    addTrailingComposer();
    layout_->addStretch();
}

bool BufferEditor::rebindTextIds(const std::vector<Item>& items)
{
    std::vector<ItemId> textIds;
    for (const auto& item : items)
        if (item.type == ItemType::Text) textIds.push_back(item.id);

    // The trailing composer has no row of its own until it gets content, so one
    // more widget than row is the normal case.
    if (textIds.size() > textWidgets_.size()) return false;

    for (size_t i = 0; i < textIds.size(); ++i) textWidgets_[i]->setItemId(textIds[i]);
    return true;
}

std::vector<BufferEditor::DirtyText> BufferEditor::dirtyText() const
{
    std::vector<DirtyText> out;
    for (auto* widget : textWidgets_) {
        if (!widget->isDirty()) continue;
        out.push_back({widget->itemId(), widget->text()});
    }
    return out;
}

void BufferEditor::markClean()
{
    for (auto* widget : textWidgets_) widget->markClean();
}

void BufferEditor::focusFirstEditor()
{
    if (!textWidgets_.empty()) textWidgets_.front()->focusText();
}

int BufferEditor::desiredHeight() const
{
    int height = layout_->contentsMargins().top() + layout_->contentsMargins().bottom();
    int count = 0;
    for (int i = 0; i < layout_->count(); ++i) {
        QWidget* w = layout_->itemAt(i)->widget();
        if (!w) continue;
        if (auto* text = qobject_cast<TextItemWidget*>(w)) height += text->desiredHeight();
        else height += w->sizeHint().height();
        ++count;
    }
    return height + std::max(0, count - 1) * layout_->spacing();
}

}  // namespace napkin
