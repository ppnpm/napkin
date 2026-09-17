#include "BufferListView.h"
#include "BufferCardDelegate.h"
#include "BufferListModel.h"
#include "BufferEditor.h"
#include "../media/BlobStore.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QMouseEvent>
#include <QMovie>
#include <QKeyEvent>
#include <QScrollBar>

namespace napkin {

BufferListView::BufferListView(Thumbnailer& thumbs, BlobStore& blobs, QWidget* parent)
    : QListView(parent), blobs_(&blobs)
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

    delegate_->setThumbnailer(&thumbs);

    editor_ = new BufferEditor(thumbs, blobs, viewport());
    editor_->hide();
    connect(editor_, &BufferEditor::edited, this, &BufferListView::editorTextChanged);
    connect(editor_, &BufferEditor::collapseRequested, this, &BufferListView::collapseRequested);
    connect(editor_, &BufferEditor::heightChanged, this, &BufferListView::syncExpandedHeight);
    connect(editor_, &BufferEditor::imagePasted, this, &BufferListView::imagePasted);
    connect(editor_, &BufferEditor::imageActivated, this, &BufferListView::imageItemActivated);
    connect(editor_, &BufferEditor::itemRemoveRequested, this,
            &BufferListView::itemRemoveRequested);


    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { repositionEditor(); });
}

void BufferListView::expandRow(int row, const std::vector<Item>& items)
{
    expandedRow_ = row;
    if (auto* m = qobject_cast<BufferListModel*>(model())) m->setExpandedRow(row);

    editor_->setItems(items);
    editor_->show();
    syncExpandedHeight();
    repositionEditor();
    editor_->focusFirstEditor();

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

// A single click only selects, so a card can be picked up and acted on — pinned,
// kept, deleted — without being opened first. Opening is a deliberate second
// gesture: double-click, or Enter.
void BufferListView::mouseDoubleClickEvent(QMouseEvent* e)
{
    const QModelIndex index = indexAt(e->pos());
    if (index.isValid() && index.row() != expandedRow_) {
        setCurrentIndex(index);
        emit rowActivated(index.row());
        return;
    }
    QListView::mouseDoubleClickEvent(e);
}

void BufferListView::mouseMoveEvent(QMouseEvent* e)
{
    QListView::mouseMoveEvent(e);
    updateHoverAnimation(indexAt(e->pos()));
}

void BufferListView::leaveEvent(QEvent* e)
{
    stopHoverAnimation();
    QListView::leaveEvent(e);
}

// An animated image plays while the pointer is over its card, so a GIF is
// visibly a GIF without twelve of them running at once (SPEC.md §12).
void BufferListView::updateHoverAnimation(const QModelIndex& index)
{
    if (!index.isValid() || !blobs_) { stopHoverAnimation(); return; }
    if (index.row() == hoverRow_) return;

    stopHoverAnimation();

    if (!index.data(BufferListModel::ThumbAnimatedRole).toBool()) return;
    const QString hash = index.data(BufferListModel::ThumbHashRole).toString();
    const QString mime = index.data(BufferListModel::ThumbMimeRole).toString();
    if (hash.isEmpty()) return;

    hoverRow_ = index.row();
    hoverMovie_ = new QMovie(blobs_->pathFor(hash, mime), QByteArray(), this);
    hoverMovie_->setCacheMode(QMovie::CacheNone);
    hoverMovie_->setScaledSize(QSize(BufferCardDelegate::kThumbSize * 2,
                                     BufferCardDelegate::kThumbSize * 2));
    connect(hoverMovie_, &QMovie::frameChanged, this, [this] {
        if (hoverRow_ < 0 || !hoverMovie_) return;
        delegate_->setAnimationFrame(hoverRow_, hoverMovie_->currentPixmap());
        update(model()->index(hoverRow_, 0));
    });
    hoverMovie_->start();
}

void BufferListView::stopHoverAnimation()
{
    if (hoverMovie_) {
        hoverMovie_->stop();
        hoverMovie_->deleteLater();
        hoverMovie_ = nullptr;
    }
    if (hoverRow_ >= 0) {
        const int row = hoverRow_;
        hoverRow_ = -1;
        delegate_->clearAnimationFrame();
        if (model() && row < model()->rowCount()) update(model()->index(row, 0));
    }
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
        case Qt::Key_R:      emit restoreRequested(row);    return;
        default: break;
        }
    }
    QListView::keyPressEvent(e);
}

}  // namespace napkin
