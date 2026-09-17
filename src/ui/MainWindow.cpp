#include "MainWindow.h"
#include "Autosave.h"
#include "BufferListModel.h"
#include "BufferListView.h"
#include "InlineEditor.h"

#include "../data/BufferRepository.h"
#include "../data/Database.h"
#include "../data/ItemRepository.h"
#include "../domain/BufferService.h"
#include "../domain/Preview.h"
#include "../domain/TimeFormat.h"

#include <QCloseEvent>
#include <QLabel>
#include <QAction>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace napkin {

MainWindow::MainWindow(Database& db, BufferRepository& buffers, ItemRepository& items,
                       BufferService& service, QWidget* parent)
    : QMainWindow(parent), db_(db), buffers_(buffers), items_(items), service_(service)
{
    buildUi();
}

void MainWindow::buildUi()
{
    model_ = new BufferListModel(buffers_, items_, this);
    view_  = new BufferListView;
    view_->setModel(model_);

    // --- empty state (SPEC.md §7) -------------------------------------------
    auto* empty = new QWidget;
    auto* emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel(tr("Napkin"));
    QFont tf = title->font();
    tf.setPointSizeF(tf.pointSizeF() + 9);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);

    auto* line1 = new QLabel(tr("Put something here."));
    line1->setAlignment(Qt::AlignCenter);

    auto* line2 = new QLabel(tr("Ctrl+N to begin"));
    line2->setAlignment(Qt::AlignCenter);
    QPalette dim = line2->palette();
    QColor c = dim.color(QPalette::Text);
    c.setAlpha(130);
    dim.setColor(QPalette::WindowText, c);
    line2->setPalette(dim);

    emptyLayout->addWidget(title);
    emptyLayout->addSpacing(10);
    emptyLayout->addWidget(line1);
    emptyLayout->addSpacing(4);
    emptyLayout->addWidget(line2);

    stack_ = new QStackedWidget;
    stack_->addWidget(view_);
    stack_->addWidget(empty);
    setCentralWidget(stack_);

    // --- autosave ------------------------------------------------------------
    autosave_ = new Autosave(this);
    autosave_->setFlushHandler([this] { flushEditor(); });

    connect(view_, &BufferListView::rowActivated, this, &MainWindow::openRow);
    connect(view_, &BufferListView::collapseRequested, this, &MainWindow::collapseEditor);
    connect(view_, &BufferListView::editorTextChanged, this, [this] {
        autosave_->noteChange();
        if (editingBuffer_ == kNoBuffer) {
            // Keep the draft card honest before it has ever been written.
            Draft d;
            d.setText(view_->editorText());
            model_->setDraftPreview(derivePreview(d.items(), int(d.items().size())));
        }
    });
    connect(model_, &BufferListModel::countChanged, this, [this] { updateEmptyState(); });

    // --- actions --------------------------------------------------------------
    // An action rather than a bare shortcut: it carries its own label and key
    // hint, so the binding is discoverable and can be surfaced in a menu later
    // without rewiring anything.
    auto* newBufferAction = new QAction(tr("New buffer"), this);
    newBufferAction->setObjectName(QStringLiteral("newBufferAction"));
    newBufferAction->setShortcut(QKeySequence::New);
    newBufferAction->setShortcutContext(Qt::WindowShortcut);
    connect(newBufferAction, &QAction::triggered, this, &MainWindow::newDraft);
    addAction(newBufferAction);

    // Relative labels go stale silently, so repaint them on a slow tick.
    timeRefresh_ = new QTimer(this);
    timeRefresh_->setInterval(kTimeRefreshMs);
    connect(timeRefresh_, &QTimer::timeout, this, [this] { model_->refreshTimestamps(); });
    timeRefresh_->start();

    setWindowTitle(tr("Napkin"));
    resize(560, 760);
    updateEmptyState();
}

void MainWindow::updateEmptyState()
{
    const bool empty = model_->rowCount() == 0;
    stack_->setCurrentIndex(empty ? 1 : 0);
}

void MainWindow::newDraft()
{
    if (view_->isEditing()) {
        autosave_->flushNow();
        collapseEditor();
    }
    stack_->setCurrentIndex(0);  // leave the empty state immediately

    const int row = model_->insertDraftRow();
    editingBuffer_ = kNoBuffer;
    editingItem_   = kNoItem;
    view_->expandRow(row, QString());
}

void MainWindow::openRow(int row)
{
    if (view_->isEditing()) {
        autosave_->flushNow();
        collapseEditor();
    }

    const BufferId id = model_->idAt(row);
    if (id == kNoBuffer) {  // re-opening an uncommitted draft
        editingBuffer_ = kNoBuffer;
        editingItem_   = kNoItem;
        view_->expandRow(row, QString());
        return;
    }

    // Phase 2 edits the buffer's first text item; image items are untouched.
    QString text;
    editingItem_ = kNoItem;
    for (const auto& item : items_.listForBuffer(id)) {
        if (item.type == ItemType::Text) { text = item.text; editingItem_ = item.id; break; }
    }
    editingBuffer_ = id;
    view_->expandRow(row, text);
}

void MainWindow::collapseEditor()
{
    if (!view_->isEditing()) return;
    autosave_->flushNow();

    // An abandoned draft evaporates because it was never written (invariant 5).
    if (editingBuffer_ == kNoBuffer && view_->editorText().trimmed().isEmpty())
        model_->removeDraftRow();

    view_->collapse();
    editingBuffer_ = kNoBuffer;
    editingItem_   = kNoItem;
    updateEmptyState();
}

void MainWindow::flushEditor()
{
    if (!view_->isEditing()) return;
    const QString text = view_->editorText();

    try {
        if (editingBuffer_ == kNoBuffer) {
            if (text.trimmed().isEmpty()) return;  // invariant 5: no row yet

            Draft draft;
            draft.setText(text);
            editingBuffer_ = service_.commitDraft(draft);
            if (editingBuffer_ == kNoBuffer) return;

            const auto head = items_.previewHead(editingBuffer_, 1);
            if (!head.empty()) editingItem_ = head.front().id;
            model_->promoteDraft(editingBuffer_);
        } else if (editingItem_ == kNoItem) {
            if (text.trimmed().isEmpty()) return;
            editingItem_ = service_.appendTo(editingBuffer_, Item::makeText(text));
        } else {
            service_.updateTextItem(editingBuffer_, editingItem_, text);
        }
        model_->invalidatePreview(editingBuffer_);
    } catch (const std::exception&) {
        // SPEC.md §14: never silently discard content. The text stays in the
        // editor, and the next flush will try again.
        autosave_->noteChange();
    }
}

bool MainWindow::event(QEvent* e)
{
    // Losing the window is one of the moments where waiting would be
    // indefensible (SPEC.md §8).
    if (e->type() == QEvent::WindowDeactivate) autosave_->flushNow();
    return QMainWindow::event(e);
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    autosave_->flushNow();
    QMainWindow::closeEvent(e);
}

void MainWindow::raiseFromOtherInstance()
{
    showNormal();
    raise();
    activateWindow();
}

}  // namespace napkin
