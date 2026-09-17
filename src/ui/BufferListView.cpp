#include "BufferListView.h"
#include "BufferCardDelegate.h"
#include "BufferListModel.h"
#include "InlineEditor.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QKeyEvent>
#include <QScrollBar>

namespace napkin {

BufferListView::BufferListView(QWidget* parent) : QListView(parent)
{
    delegate_ = new BufferCardDelegate(this);
    setItemDelegate(delegate_);

    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);          // for the subtle hover state
    setUniformItemSizes(false);      // the expanded row is taller
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_MacShowFocusRect, false);

    editor_ = new InlineEditor(viewport());
    editor_->hide();
    connect(editor_, &InlineEditor::textEdited, this, &BufferListView::editorTextChanged);
    connect(editor_, &InlineEditor::collapseRequested, this, &BufferListView::collapseRequested);
    connect(editor_, &InlineEditor::heightChanged, this, &BufferListView::syncExpandedHeight);
    connect(editor_, &InlineEditor::imagePasted, this, &BufferListView::imagePasted);

    connect(this, &QAbstractItemView::clicked, this, [this](const QModelIndex& i) {
        if (i.row() == expandedRow_) return;
        // Clicking the thumbnail opens the image; clicking the rest of the card
        // opens the buffer for editing.
        const QRect item = visualRect(i);
        const QRect content = delegate_->contentRect(item, i);
        const QRect thumb(content.left(), content.top(),
                          BufferCardDelegate::kThumbSize, BufferCardDelegate::kThumbSize);
        const bool hasThumb = !i.data(BufferListModel::ThumbHashRole).toString().isEmpty();
        if (hasThumb && thumb.contains(mapFromGlobal(QCursor::pos()))) {
            emit imageActivated(i.row());
            return;
        }
        emit rowActivated(i.row());
    });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { repositionEditor(); });
}

QString BufferListView::editorText() const { return editor_->text(); }

void BufferListView::setThumbnailer(Thumbnailer* thumbnailer)
{
    delegate_->setThumbnailer(thumbnailer);
}

void BufferListView::expandRow(int row, const QString& initialText)
{
    expandedRow_ = row;
    if (auto* m = qobject_cast<BufferListModel*>(model())) m->setExpandedRow(row);

    editor_->setText(initialText);
    editor_->show();
    syncExpandedHeight();
    repositionEditor();
    editor_->focusEditor();

    setCurrentIndex(model()->index(row, 0));
    scrollTo(model()->index(row, 0), QAbstractItemView::EnsureVisible);
}

void BufferListView::collapse()
{
    if (expandedRow_ < 0) return;
    expandedRow_ = -1;
    editor_->hide();
    if (auto* m = qobject_cast<BufferListModel*>(model())) m->setExpandedRow(-1);
    setFocus(Qt::OtherFocusReason);
}

void BufferListView::syncExpandedHeight()
{
    if (expandedRow_ < 0) return;
    const int before = delegate_->expandedHeight();
    delegate_->setExpandedHeight(editor_->desiredHeight());
    if (delegate_->expandedHeight() != before) {
        // Guarded: only a real change triggers relayout, so growing the
        // document cannot feed back into itself.
        scheduleDelayedItemsLayout();
    }
    repositionEditor();
}

void BufferListView::repositionEditor()
{
    if (expandedRow_ < 0 || !model()) return;
    const QModelIndex index = model()->index(expandedRow_, 0);
    if (!index.isValid()) return;

    const QRect item = visualRect(index);
    if (item.isNull()) { editor_->hide(); return; }

    editor_->show();
    editor_->setGeometry(delegate_->contentRect(item, index));
}

void BufferListView::contextMenuEvent(QContextMenuEvent* e)
{
    const QModelIndex index = indexAt(e->pos());
    if (!index.isValid()) return;
    setCurrentIndex(index);
    emit contextMenuRequested(index.row(), e->globalPos());
}

void BufferListView::resizeEvent(QResizeEvent* e)
{
    QListView::resizeEvent(e);
    repositionEditor();
}

void BufferListView::keyPressEvent(QKeyEvent* e)
{
    // List-scope keys only fire when the list itself has focus, so they can
    // never collide with typing (SPEC.md §7).
    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (currentIndex().isValid() && currentIndex().row() != expandedRow_) {
            emit rowActivated(currentIndex().row());
            return;
        }
        break;
    case Qt::Key_Escape:
        emit collapseRequested();
        return;
    default:
        break;
    }

    // Bare letters are safe as commands precisely because this handler only
    // runs with list focus; Delete rather than Ctrl+D, which sits next to
    // Ctrl+N and is far too easy to hit by accident.
    if (!isEditing() && currentIndex().isValid() && e->modifiers() == Qt::NoModifier) {
        const int row = currentIndex().row();
        switch (e->key()) {
        case Qt::Key_P:      emit pinToggleRequested(row);  return;
        case Qt::Key_K:      emit keepToggleRequested(row); return;
        case Qt::Key_Delete: emit trashRequested(row);      return;
        default: break;
        }
    }
    QListView::keyPressEvent(e);
}

}  // namespace napkin
