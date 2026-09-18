#include "MainWindow.h"
#include "Autosave.h"
#include "BufferListModel.h"
#include "BufferListView.h"
#include "ItemCanvas.h"
#include "Lightbox.h"
#include "UndoToast.h"

#include "../data/BufferRepository.h"
#include "../data/Database.h"
#include "../data/ItemRepository.h"
#include "../domain/BufferService.h"
#include "../domain/Preview.h"
#include "../domain/TimeFormat.h"
#include "../app/Paths.h"
#include "../media/BlobGc.h"
#include "../media/BlobStore.h"
#include "../media/ClipboardContent.h"
#include "../media/ImageFormats.h"
#include "../media/Thumbnailer.h"

#include <QCloseEvent>
#include <QLabel>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMenu>
#include <QToolButton>
#include <QMimeData>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace napkin {

MainWindow::MainWindow(Database& db, BufferRepository& buffers, ItemRepository& items,
                       BufferService& service, BlobStore& blobs, Thumbnailer& thumbs,
                       QWidget* parent)
    : QMainWindow(parent), db_(db), buffers_(buffers), items_(items), service_(service),
      blobs_(blobs), thumbs_(thumbs)
{
    buildUi();
}

void MainWindow::buildUi()
{
    model_ = new BufferListModel(buffers_, items_, this);
    view_  = new BufferListView(thumbs_, blobs_);
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

    emptyTitle_ = title; emptyLine1_ = line1; emptyLine2_ = line2;

    canvas_ = new ItemCanvas(thumbs_, blobs_);

    // The list keeps its own width; the canvas takes the rest. Below ~820px the
    // splitter lets the user collapse either side rather than cramming both.
    splitter_ = new QSplitter(Qt::Horizontal);
    splitter_->addWidget(view_);
    splitter_->addWidget(canvas_);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setChildrenCollapsible(false);
    view_->setMinimumWidth(260);
    view_->setMaximumWidth(520);
    canvas_->setMinimumWidth(320);
    splitter_->setSizes({340, 660});

    stack_ = new QStackedWidget;
    stack_->addWidget(splitter_);
    stack_->addWidget(empty);

    auto* central = new QWidget;
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(buildHeaderWidget());
    rootLayout->addWidget(stack_, 1);
    setCentralWidget(central);

    toast_ = new UndoToast(central);


    // --- autosave ------------------------------------------------------------
    autosave_ = new Autosave(this);
    autosave_->setFlushHandler([this] { flushAndReportFailure(); });

    connect(view_, &BufferListView::rowActivated, this, &MainWindow::openRow);
    connect(view_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) {
                selectBuffer(current.isValid() ? current.row() : -1);
            });
    connect(canvas_, &ItemCanvas::imageActivated, this, &MainWindow::openImageItem);
    connect(canvas_, &ItemCanvas::removeRequested, this, &MainWindow::removeItems);
    connect(canvas_, &ItemCanvas::imagePasted, this,
            [this](const QByteArray& bytes, const QString& mime) {
                addImageToCurrent(bytes, mime, QString());
            });
    connect(canvas_, &ItemCanvas::edited, this, [this] {
        // Freeze on the first keystroke: autosave is about to bump modified_at,
        // and the card you are typing into must not leap to the top.
        model_->freezeOrder(true);
        autosave_->noteChange();
        if (editingBuffer_ == kNoBuffer) {
            Draft d;
            for (const auto& dirty : canvas_->dirtyText()) d.setText(dirty.text);
            model_->setDraftPreview(derivePreview(d.items(), int(d.items().size()), 0));
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

    auto* pasteAction = new QAction(tr("Paste"), this);
    pasteAction->setObjectName(QStringLiteral("pasteAction"));
    pasteAction->setShortcut(QKeySequence::Paste);
    pasteAction->setShortcutContext(Qt::WindowShortcut);
    connect(pasteAction, &QAction::triggered, this, &MainWindow::pasteFromClipboard);
    addAction(pasteAction);

    auto* addTextAction = new QAction(tr("New text block"), this);
    addTextAction->setObjectName(QStringLiteral("addTextAction"));
    addTextAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    addTextAction->setShortcutContext(Qt::WindowShortcut);
    connect(addTextAction, &QAction::triggered, this, [this] { appendTextBlock(); });
    addAction(addTextAction);

    auto* addImageAction = new QAction(tr("Add image…"), this);
    addImageAction->setObjectName(QStringLiteral("addImageAction"));
    addImageAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+I")));
    addImageAction->setShortcutContext(Qt::WindowShortcut);
    connect(addImageAction, &QAction::triggered, this, &MainWindow::addImageFromFile);
    addAction(addImageAction);

    connect(view_, &BufferListView::pinToggleRequested,  this, &MainWindow::togglePin);
    connect(view_, &BufferListView::keepToggleRequested, this, &MainWindow::toggleKeep);
    connect(view_, &BufferListView::trashRequested,      this, &MainWindow::trashRow);
    connect(view_, &BufferListView::restoreRequested,    this, &MainWindow::restoreRow);
    connect(view_, &BufferListView::contextMenuRequested, this, &MainWindow::showContextMenu);

    // Relative labels go stale silently, so repaint them on a slow tick.
    timeRefresh_ = new QTimer(this);
    timeRefresh_->setInterval(kTimeRefreshMs);
    connect(timeRefresh_, &QTimer::timeout, this, [this] { model_->refreshTimestamps(); });
    timeRefresh_->start();

    view_->setAccessibleName(tr("Buffers"));
    view_->setAccessibleDescription(
        tr("Your buffers, newest first. Enter opens one; P pins, K keeps, Delete trashes."));

    setWindowTitle(tr("Napkin"));
    resize(560, 760);
    updateEmptyState();

    // A keyboard user arriving with focus on the Trash button and no row
    // selected could press P, K, Delete or Enter and have nothing happen at all.
    if (model_->rowCount() > 0) view_->setCurrentIndex(model_->index(0, 0));
    view_->setFocus(Qt::OtherFocusReason);
}

QWidget* MainWindow::buildHeaderWidget()
{
    auto* header = new QWidget;
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(16, 10, 12, 10);

    // No wordmark: the window title already says Napkin, and §7 asks for
    // content to dominate. The header carries actions, not branding.
    layout->addStretch();

    // Phase 5 puts the search field here, between the name and the trash
    // toggle — the placement adopted from the §7 mockup review.
    auto* trashButton = new QPushButton(tr("Trash"));
    trashButton->setAccessibleName(tr("Show trash"));
    trashButton->setToolTip(tr("Show deleted buffers"));
    trashButton->setFlat(true);
    trashButton->setCheckable(true);
    trashButton->setCursor(Qt::PointingHandCursor);
    trashButton->setObjectName(QStringLiteral("trashToggle"));
    // SPEC §16 justified using QAction over QShortcut because an action
    // "carries its own label and key hint" — but they were attached to no menu
    // and no button, so they were invisible shortcuts wearing a label. This
    // collects that payoff: every binding is now readable somewhere.
    auto* newButton = new QPushButton(tr("＋ New"));
    newButton->setFlat(true);
    newButton->setCursor(Qt::PointingHandCursor);
    newButton->setObjectName(QStringLiteral("newButton"));
    newButton->setToolTip(tr("New buffer (Ctrl+N)"));
    newButton->setAccessibleName(tr("New buffer"));
    connect(newButton, &QPushButton::clicked, this, &MainWindow::newDraft);
    layout->addWidget(newButton);

    auto* menuButton = new QToolButton;
    menuButton->setText(QStringLiteral("⋯"));
    menuButton->setAutoRaise(true);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    menuButton->setObjectName(QStringLiteral("overflowButton"));
    menuButton->setToolTip(tr("More actions"));
    menuButton->setAccessibleName(tr("More actions"));
    menuButton->setMenu(buildOverflowMenu());
    layout->addWidget(menuButton);

    emptyTrashButton_ = new QPushButton(tr("Empty trash"));
    emptyTrashButton_->setFlat(true);
    emptyTrashButton_->setCursor(Qt::PointingHandCursor);
    emptyTrashButton_->setObjectName(QStringLiteral("emptyTrashButton"));
    emptyTrashButton_->hide();   // only meaningful while looking at the trash
    connect(emptyTrashButton_, &QPushButton::clicked, this, &MainWindow::emptyTrash);
    layout->addWidget(emptyTrashButton_);

    connect(trashButton, &QPushButton::toggled, this, &MainWindow::showTrash);
    layout->addWidget(trashButton);

    return header;
}

QMenu* MainWindow::buildOverflowMenu()
{
    auto* menu = new QMenu(this);
    for (QAction* action : actions()) menu->addAction(action);
    menu->addSeparator();
    auto* help = menu->addAction(tr("Keyboard shortcuts…"));
    connect(help, &QAction::triggered, this, &MainWindow::showShortcuts);
    return menu;
}

void MainWindow::showShortcuts()
{
    QMessageBox::information(
        this, tr("Keyboard shortcuts"),
        tr("<table cellpadding='4'>"
           "<tr><td><b>Ctrl+N</b></td><td>New buffer</td></tr>"
           "<tr><td><b>Ctrl+T</b></td><td>New text block in this buffer</td></tr>"
           "<tr><td><b>Ctrl+V</b></td><td>Paste into this buffer</td></tr>"
           "<tr><td><b>Ctrl+Shift+I</b></td><td>Add an image from a file</td></tr>"
           "<tr><td colspan='2'>&nbsp;</td></tr>"
           "<tr><td colspan='2'><i>In the canvas:</i></td></tr>"
           "<tr><td><b>Click</b></td><td>Select an item</td></tr>"
           "<tr><td><b>Double-click</b></td><td>Edit text, or open an image</td></tr>"
           "<tr><td><b>Ctrl</b> / <b>Shift</b> + click</td><td>Extend the selection</td></tr>"
           "<tr><td><b>Ctrl+A</b></td><td>Select every item</td></tr>"
           "<tr><td><b>Ctrl+C</b> / <b>Ctrl+X</b> / <b>Delete</b></td>"
           "<td>Copy, cut or delete the selection</td></tr>"
           "<tr><td><b>Esc</b></td><td>Stop editing, then clear the selection</td></tr>"
           "<tr><td colspan='2'>&nbsp;</td></tr>"
           "<tr><td colspan='2'><i>With the list focused:</i></td></tr>"
           "<tr><td><b>P</b></td><td>Pin — keeps it at the top</td></tr>"
           "<tr><td><b>K</b></td><td>Keep — never removed by a sweep</td></tr>"
           "<tr><td><b>Delete</b></td><td>Move to trash</td></tr>"
           "<tr><td><b>R</b></td><td>Restore (in the trash)</td></tr>"
           "</table>"));
}

void MainWindow::undoLastTrashForTest(BufferId id, bool wasKept, Timestamp modifiedAt)
{
    service_.restore(id);
    if (wasKept) service_.setKept(id, true);
    buffers_.setModifiedAt(id, modifiedAt);
    lastTrashed_ = {};
    reloadPreservingSelection();
}

void MainWindow::restoreRow(int row)
{
    if (model_->mode() != BufferListModel::Mode::Trash) return;
    const BufferId id = model_->idAt(row);
    if (id == kNoBuffer) return;
    service_.restore(id);
    reloadPreservingSelection();
    emptyTrashButton_->setVisible(model_->rowCount() > 0);
}

void MainWindow::showTrash(bool trash)
{
    flushAndReportFailure();
    toast_->dismiss();
    model_->setMode(trash ? BufferListModel::Mode::Trash : BufferListModel::Mode::Live);
    emptyTrashButton_->setVisible(trash && model_->rowCount() > 0);
    updateEmptyState();
}

void MainWindow::reloadPreservingSelection()
{
    const BufferId current = model_->idAt(view_->currentIndex().row());
    model_->reload();
    if (const int row = model_->rowForId(current); row >= 0)
        view_->setCurrentIndex(model_->index(row, 0));
    updateEmptyState();
}

void MainWindow::togglePin(int row)
{
    const BufferId id = model_->idAt(row);
    if (id == kNoBuffer || model_->mode() != BufferListModel::Mode::Live) return;

    const auto buffer = buffers_.find(id);
    if (!buffer) return;
    service_.setPinned(id, !buffer->pinned);
    reloadPreservingSelection();   // pinning moves the card; that is the point
}

void MainWindow::toggleKeep(int row)
{
    const BufferId id = model_->idAt(row);
    if (id == kNoBuffer || model_->mode() != BufferListModel::Mode::Live) return;

    const auto buffer = buffers_.find(id);
    if (!buffer) return;
    service_.setKept(id, !buffer->kept);
    model_->refreshRow(id);        // keeping changes nothing about placement
}

void MainWindow::trashRow(int row)
{
    const BufferId id = model_->idAt(row);
    if (id == kNoBuffer) return;

    if (model_->mode() == BufferListModel::Mode::Trash) {
        // Delete used to mean *restore* here, which is the opposite of what it
        // means in every file manager and mail client. Restore is its own
        // action; Delete destroys, with a confirmation because it is final.
        QMessageBox box(this);
        box.setWindowTitle(tr("Delete permanently?"));
        box.setText(tr("Delete this buffer permanently?"));
        box.setInformativeText(tr("This cannot be undone."));
        box.setIcon(QMessageBox::Warning);
        box.addButton(QMessageBox::Cancel);
        auto* confirm = box.addButton(tr("Delete permanently"), QMessageBox::DestructiveRole);
        box.setDefaultButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() != confirm) return;

        buffers_.hardDeleteEvenIfKept(id);
        reconcileBlobs(items_, blobs_, paths::thumbsDir());
        reloadPreservingSelection();
        emptyTrashButton_->setVisible(model_->rowCount() > 0);
        return;
    }

    // The repository refuses a kept buffer outright, so the confirmation cannot
    // be skipped by a UI path that forgets to ask (SPEC.md §6).
    if (const auto before = buffers_.find(id))
        lastTrashed_ = {id, before->kept, before->modifiedAt};

    if (!service_.trash(id)) {
        QMessageBox box(this);
        box.setWindowTitle(tr("Delete kept buffer?"));
        box.setText(tr("This buffer is kept."));
        box.setInformativeText(
            tr("Kept buffers are never removed by a sweep. Deleting it now "
               "releases that protection and moves it to the trash, where it "
               "stays for %1 days.").arg(kTrashRetentionDays));
        box.setIcon(QMessageBox::Warning);
        box.addButton(QMessageBox::Cancel);
        auto* del = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
        box.setDefaultButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() != del) return;
        service_.trashConfirmed(id);
    }

    reloadPreservingSelection();

    // Confirming the deletion of a kept buffer releases the keep, so undo has to
    // put it back — otherwise the user recovers a buffer that quietly lost the
    // protection they asked for, and the next sweep offers it up.
    const auto state = lastTrashed_;
    toast_->offer(tr("Buffer moved to trash"), [this, state] {
        service_.restore(state.id);
        if (state.kept) service_.setKept(state.id, true);
        buffers_.setModifiedAt(state.id, state.modifiedAt);
        reloadPreservingSelection();
    });
}

void MainWindow::showContextMenu(int row, const QPoint& globalPos)
{
    const BufferId id = model_->idAt(row);
    if (id == kNoBuffer) return;
    const auto buffer = buffers_.find(id);
    if (!buffer) return;

    QMenu menu(this);
    if (model_->mode() == BufferListModel::Mode::Trash) {
        menu.addAction(tr("Restore\tR"), this, [this, row] { restoreRow(row); });
        menu.addSeparator();
        menu.addAction(tr("Delete permanently\tDel"), this, [this, row] { trashRow(row); });
    } else {
        // Only actions that apply: no greyed-out rows, no giant toolbar.
        menu.addAction(buffer->pinned ? tr("Unpin") : tr("Pin\tP"),
                       this, [this, row] { togglePin(row); });
        menu.addAction(buffer->kept ? tr("Release keep\tK") : tr("Keep\tK"),
                       this, [this, row] { toggleKeep(row); });
        menu.addSeparator();
        menu.addAction(tr("Delete\tDel"), this, [this, row] { trashRow(row); });
    }
    menu.exec(globalPos);
}

void MainWindow::updateEmptyState()
{
    const bool empty = model_->rowCount() == 0;
    stack_->setCurrentIndex(empty ? 1 : 0);
    if (!empty) return;

    const bool trash = model_->mode() == BufferListModel::Mode::Trash;
    emptyTitle_->setVisible(!trash);
    emptyLine1_->setText(trash ? tr("Nothing in the trash.") : tr("Put something here."));
    emptyLine2_->setText(trash ? tr("Deleted buffers stay here for %1 days.").arg(kTrashRetentionDays)
                               : tr("Ctrl+N to begin"));
}

void MainWindow::reportProblem(const QString& title, const QString& detail)
{
    // SPEC.md §14: plain language, and the user's content is accounted for.
    QMessageBox box(QMessageBox::Warning, title, detail, QMessageBox::Ok, this);
    box.exec();
}

bool MainWindow::addImageToCurrent(const QByteArray& bytes, const QString& mime,
                                   const QString& sourceName)
{
    if (model_->mode() != BufferListModel::Mode::Live) return false;

    const auto stored = blobs_.store(bytes, mime);
    if (!stored.ok) {
        reportProblem(tr("Could not add the image"),
                      stored.error + tr("\n\nNothing else in the buffer was changed."));
        return false;
    }

    try {
        // The blob is already fsynced and renamed into place, so committing the
        // row now can only ever leave an orphan, never a dangling reference.
        {
            autosave_->flushNow();                       // the text lands first
            if (editingBuffer_ == kNoBuffer) {           // an empty draft gets promoted
                Draft draft;
                draft.add(Item::makeImage(stored.hash, stored.size.width(), stored.size.height(),
                                          stored.byteSize, sourceName, stored.mime,
                                          stored.animated));
                editingBuffer_ = service_.commitDraft(draft);
                if (model_->hasDraft()) {
                    model_->promoteDraft(editingBuffer_);
                } else {
                    // Pasted straight onto the stack with nothing selected:
                    // there is no draft card to promote, so the list has to
                    // learn about the new buffer the ordinary way.
                    model_->reload();
                    if (const int row = model_->rowForId(editingBuffer_); row >= 0)
                        view_->setCurrentIndex(model_->index(row, 0));
                }
            } else {
                service_.appendTo(editingBuffer_,
                    Item::makeImage(stored.hash, stored.size.width(), stored.size.height(),
                                    stored.byteSize, sourceName, stored.mime, stored.animated));
            }
            canvas_->setItems(items_.listForBuffer(editingBuffer_));
            model_->invalidatePreview(editingBuffer_);
        }
    } catch (const std::exception&) {
        reportProblem(tr("Could not add the image"),
                      tr("Napkin saved the image but could not record it. "
                         "Nothing else in the buffer was changed."));
        return false;
    }

    updateEmptyState();
    return true;
}

void MainWindow::pasteFromClipboard()
{
    if (model_->mode() != BufferListModel::Mode::Live) return;

    const auto content = readClipboard(QApplication::clipboard()->mimeData());
    if (content.kind == ClipboardContent::Kind::None) return;

    // One rule for everything on the clipboard: a paste goes into the buffer
    // you are looking at, and makes a new one only when you are looking at
    // nothing. Text used to always create a buffer while images appended to the
    // current one, which meant the same gesture did two different things
    // depending on what you had copied.
    if (editingBuffer_ == kNoBuffer && !model_->hasDraft()) newDraft();

    if (content.kind == ClipboardContent::Kind::Image) {
        addImageToCurrent(content.imageBytes, content.imageMime, QString());
        return;
    }
    appendTextBlock(content.text);
}

// Ctrl+T, and the path every pasted text block takes.
void MainWindow::appendTextBlock(const QString& text)
{
    if (model_->mode() != BufferListModel::Mode::Live) return;
    if (editingBuffer_ == kNoBuffer && !model_->hasDraft()) { newDraft(); }

    if (!flushAndReportFailure()) return;

    if (editingBuffer_ == kNoBuffer) {
        if (text.trimmed().isEmpty()) { canvas_->focusComposer(); return; }
        Draft draft;
        draft.setText(text);
        editingBuffer_ = service_.commitDraft(draft);
        if (editingBuffer_ == kNoBuffer) return;
        if (model_->hasDraft()) model_->promoteDraft(editingBuffer_);
        else                    reloadPreservingSelection();
    } else if (!text.trimmed().isEmpty()) {
        service_.appendTo(editingBuffer_, Item::makeText(text));
    }

    canvas_->setItems(items_.listForBuffer(editingBuffer_));
    model_->invalidatePreview(editingBuffer_);
    canvas_->focusComposer();
    updateEmptyState();
}

void MainWindow::addImageFromFile()
{
    if (model_->mode() != BufferListModel::Mode::Live) return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Add image"), QString(),
        formats::pickerFilter());   // built from what this build can actually open
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        reportProblem(tr("Could not read that file"),
                      tr("Napkin could not open %1.").arg(QFileInfo(path).fileName()));
        return;
    }
    if (file.size() > BlobStore::kMaxBytes) {
        reportProblem(tr("That image is too large"),
                      tr("Napkin keeps images up to %1 MB.")
                          .arg(BlobStore::kMaxBytes / (1024 * 1024)));
        return;
    }
    addImageToCurrent(file.readAll(), QString(), QFileInfo(path).fileName());
}

void MainWindow::emptyTrash()
{
    // What will actually be destroyed, not what is merely in the bin: a kept
    // row is skipped by the purge, so counting it here over-promises.
    const int count = buffers_.countPurgeable();
    if (count == 0) return;

    QMessageBox box(this);
    box.setWindowTitle(tr("Empty the trash?"));
    box.setText(tr("Delete %n buffer(s) permanently?", nullptr, count));
    box.setInformativeText(tr("This cannot be undone."));
    box.setIcon(QMessageBox::Warning);
    box.addButton(QMessageBox::Cancel);
    auto* confirm = box.addButton(tr("Delete permanently"), QMessageBox::DestructiveRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != confirm) return;

    toast_->dismiss();   // whatever it was offering no longer exists
    service_.emptyTrash();
    reconcileBlobs(items_, blobs_, paths::thumbsDir());  // reclaim blobs AND thumbnails
    reloadPreservingSelection();
    emptyTrashButton_->setVisible(model_->rowCount() > 0);
}

void MainWindow::resizeEvent(QResizeEvent* e)
{
    QMainWindow::resizeEvent(e);
    if (toast_ && toast_->isVisible()) toast_->reposition();
}

void MainWindow::newDraft()
{
    if (!flushAndReportFailure()) return;
    // Ctrl+N while looking at the bin used to create a live buffer and display
    // it under the TRASH header.
    if (model_->mode() != BufferListModel::Mode::Live) showTrash(false);
    stack_->setCurrentIndex(0);  // leave the empty state immediately

    const int row = model_->insertDraftRow();
    editingBuffer_ = kNoBuffer;
    editingItem_   = kNoItem;
    view_->setCurrentIndex(model_->index(row, 0));
    canvas_->setItems({});
    canvas_->focusComposer();
}

// Selecting a buffer in the list shows it in the canvas. There is no expand
// step any more: the pane is always there, so selection *is* opening.
void MainWindow::selectBuffer(int row)
{
    if (!flushAndReportFailure()) return;   // do not leave the old buffer's text behind
    model_->freezeOrder(false);             // the previous buffer is done; let it re-sort

    // An abandoned draft evaporates because it never had a row (invariant 5).
    if (editingBuffer_ == kNoBuffer && model_->hasDraft() && model_->draftRow() != row) {
        const int draft = model_->draftRow();
        model_->removeDraftRow();
        if (draft >= 0 && draft < row) --row;   // the list shifted under us
    }

    const BufferId id = model_->idAt(row);
    if (row < 0) {
        editingBuffer_ = kNoBuffer;
        canvas_->showNothingSelected();
        return;
    }

    editingBuffer_ = id;
    canvas_->setItems(id == kNoBuffer ? std::vector<Item>{} : items_.listForBuffer(id));
}

// Enter or a double-click puts the caret in the canvas, which is the "open it
// and start typing" gesture now that the pane is always visible.
void MainWindow::openRow(int row)
{
    if (view_->currentIndex().row() != row)
        view_->setCurrentIndex(model_->index(row, 0));
    canvas_->focusComposer();
}

void MainWindow::removeItems(const QList<ItemId>& ids)
{
    if (editingBuffer_ == kNoBuffer || ids.isEmpty()) return;

    // Capture before deleting: undo has to hand the content back, not an empty
    // shell. An earlier version deleted the rows and unlinked the blobs at once,
    // so undoing within the 8-second window restored a buffer with no items and
    // rows pointing at files that were already gone.
    std::vector<Item> removed;
    for (ItemId id : ids)
        if (const auto item = items_.find(id)) removed.push_back(*item);
    if (removed.empty()) return;

    const BufferId buffer = editingBuffer_;
    const int firstRemovedIndex = canvas_->indexOf(ids.first());
    for (ItemId id : ids) service_.removeItem(buffer, id);

    // Deliberately NOT reconciling blobs here. A blob whose last reference has
    // just gone is exactly the one undo is about to need. Orphans are collected
    // by the startup sweep, which is the drift-free form anyway.
    const bool emptied = items_.countForBuffer(buffer) == 0;
    std::optional<Buffer> before;
    if (emptied) {
        before = buffers_.find(buffer);
        // An item-level delete that leaves an empty husk behind is just litter.
        if (!service_.trash(buffer)) service_.trashConfirmed(buffer);
        editingBuffer_ = kNoBuffer;
        canvas_->showNothingSelected();
        reloadPreservingSelection();
    } else {
        // Keep working where you were: land on whatever now occupies the first
        // removed slot, or the last item if you deleted off the end.
        canvas_->setItems(items_.listForBuffer(buffer), firstRemovedIndex);
        model_->invalidatePreview(buffer);
    }

    const QString message = removed.size() == 1
        ? tr("Item deleted")
        : tr("%n items deleted", nullptr, int(removed.size()));

    toast_->offer(emptied ? tr("Buffer moved to trash") : message,
                  [this, buffer, removed, before, emptied] {
                      for (const auto& item : removed) items_.restoreAt(item);
                      if (emptied) {
                          service_.restore(buffer);
                          if (before && before->kept) service_.setKept(buffer, true);
                          if (before) buffers_.setModifiedAt(buffer, before->modifiedAt);
                      }
                      model_->invalidatePreview(buffer);
                      reloadPreservingSelection();
                      if (const int row = model_->rowForId(buffer); row >= 0)
                          view_->setCurrentIndex(model_->index(row, 0));
                  });
}

bool MainWindow::flushEditor()
{
    if (!canvas_) return true;
    auto* editor = canvas_;
    const auto dirty = editor->dirtyText();
    if (dirty.empty()) return true;

    // Whitespace is not content: a draft of blank text still writes no row.
    bool hasContent = false;
    for (const auto& d : dirty) if (!d.text.trimmed().isEmpty()) hasContent = true;

    try {
        if (editingBuffer_ == kNoBuffer) {
            if (!hasContent) return true;            // invariant 5

            Draft draft;
            for (const auto& d : dirty)
                if (!d.text.trimmed().isEmpty()) draft.add(Item::makeText(d.text));
            editingBuffer_ = service_.commitDraft(draft);
            if (editingBuffer_ == kNoBuffer) return true;

            model_->promoteDraft(editingBuffer_);
            // Rebind ids rather than rebuilding: the user may still be typing,
            // and recreating the widgets would move the caret to the start.
            if (!editor->rebindTextIds(items_.listForBuffer(editingBuffer_)))
                editor->setItems(items_.listForBuffer(editingBuffer_));
        } else {
            for (const auto& d : dirty) {
                if (d.id != kNoItem) {
                    service_.updateTextItem(editingBuffer_, d.id, d.text);
                } else if (!d.text.trimmed().isEmpty()) {
                    service_.appendTo(editingBuffer_, Item::makeText(d.text));
                    if (!editor->rebindTextIds(items_.listForBuffer(editingBuffer_)))
                        editor->setItems(items_.listForBuffer(editingBuffer_));
                    break;   // ids shifted; the rest of this pass is stale
                }
            }
        }
        editor->markClean();
        model_->invalidatePreview(editingBuffer_);
        saveFailures_ = 0;
        return true;
    } catch (const std::exception&) {
        // SPEC.md §14: never silently discard content. The text stays in the
        // editor, which is why the caller must not collapse on a false.
        ++saveFailures_;
        return false;
    }
}

// Flushes, and if the write failed tells the user rather than letting the text
// evaporate. Retries are bounded: an earlier build re-armed the debounce on
// every failure and spun at ~3 transactions a second, for ever, in silence.
bool MainWindow::flushAndReportFailure()
{
    if (flushEditor()) return true;

    if (saveFailures_ == 1 || saveFailures_ % 20 == 0) {
        reportProblem(tr("Napkin could not save this buffer"),
                      tr("Your text is still here and has not been changed. Napkin will keep "
                         "trying.\n\nThis usually means the disk is full, or the storage "
                         "folder is not writable."));
    }
    if (saveFailures_ < 60) autosave_->noteChange();   // bounded retry
    return false;
}

void MainWindow::openImageItem(ItemId id)
{
    const auto item = items_.find(id);
    if (!item || item->type != ItemType::Image) return;

    const QString path = blobs_.pathFor(item->blobHash, item->mime);
    if (!QFile::exists(path)) {
        reportProblem(tr("The image is missing"),
                      tr("Napkin can no longer find the file for this image. "
                         "The rest of the buffer is unchanged."));
        return;
    }
    Lightbox box(path, item->animated, item->sourceName, this);
    box.exec();
}

bool MainWindow::event(QEvent* e)
{
    // Losing the window is one of the moments where waiting would be
    // indefensible (SPEC.md §8).
    if (e->type() == QEvent::WindowDeactivate) {
        autosave_->flushNow();
        model_->freezeOrder(false);
    }
    return QMainWindow::event(e);
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    // Closing with unsaved text that cannot be written would destroy it with no
    // trace at all, which is the worst version of this failure.
    if (!flushAndReportFailure()) { e->ignore(); return; }
    QMainWindow::closeEvent(e);
}

void MainWindow::raiseFromOtherInstance()
{
    showNormal();
    raise();
    activateWindow();
}

}  // namespace napkin
