#include "MainWindow.h"
#include "Autosave.h"
#include "BufferListModel.h"
#include "BufferListView.h"
#include "InlineEditor.h"
#include "Lightbox.h"
#include "UndoToast.h"

#include "../data/BufferRepository.h"
#include "../data/Database.h"
#include "../data/ItemRepository.h"
#include "../domain/BufferService.h"
#include "../domain/Preview.h"
#include "../domain/TimeFormat.h"
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
#include <QMimeData>
#include <QMessageBox>
#include <QPushButton>
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
    view_  = new BufferListView;
    view_->setModel(model_);
    view_->setThumbnailer(&thumbs_);
    view_->setBlobStore(&blobs_);

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

    stack_ = new QStackedWidget;
    stack_->addWidget(view_);
    stack_->addWidget(empty);

    auto* central = new QWidget;
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(buildHeaderWidget());
    rootLayout->addWidget(stack_, 1);
    setCentralWidget(central);

    toast_ = new UndoToast(central);
    connect(toast_, &UndoToast::undoRequested, this, [this](BufferId id) {
        service_.restore(id);
        reloadPreservingSelection();
    });

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
    connect(view_, &BufferListView::imagePasted, this,
            [this](const QByteArray& bytes, const QString& mime) {
                addImageToCurrent(bytes, mime, QString());
            });

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

    auto* addImageAction = new QAction(tr("Add image…"), this);
    addImageAction->setObjectName(QStringLiteral("addImageAction"));
    addImageAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+I")));
    addImageAction->setShortcutContext(Qt::WindowShortcut);
    connect(addImageAction, &QAction::triggered, this, &MainWindow::addImageFromFile);
    addAction(addImageAction);

    connect(view_, &BufferListView::imageActivated, this, &MainWindow::openImage);
    connect(view_, &BufferListView::pinToggleRequested,  this, &MainWindow::togglePin);
    connect(view_, &BufferListView::keepToggleRequested, this, &MainWindow::toggleKeep);
    connect(view_, &BufferListView::trashRequested,      this, &MainWindow::trashRow);
    connect(view_, &BufferListView::contextMenuRequested, this, &MainWindow::showContextMenu);

    // Relative labels go stale silently, so repaint them on a slow tick.
    timeRefresh_ = new QTimer(this);
    timeRefresh_->setInterval(kTimeRefreshMs);
    connect(timeRefresh_, &QTimer::timeout, this, [this] { model_->refreshTimestamps(); });
    timeRefresh_->start();

    setWindowTitle(tr("Napkin"));
    resize(560, 760);
    updateEmptyState();
}

QWidget* MainWindow::buildHeaderWidget()
{
    auto* header = new QWidget;
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(16, 10, 12, 10);

    auto* name = new QLabel(tr("Napkin"));
    QFont nf = name->font();
    nf.setBold(true);
    name->setFont(nf);
    layout->addWidget(name);
    layout->addStretch();

    // Phase 5 puts the search field here, between the name and the trash
    // toggle — the placement adopted from the §7 mockup review.
    auto* trashButton = new QPushButton(tr("Trash"));
    trashButton->setFlat(true);
    trashButton->setCheckable(true);
    trashButton->setCursor(Qt::PointingHandCursor);
    trashButton->setObjectName(QStringLiteral("trashToggle"));
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

void MainWindow::showTrash(bool trash)
{
    collapseEditor();
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

    if (model_->mode() == BufferListModel::Mode::Trash) {   // restore instead
        service_.restore(id);
        reloadPreservingSelection();
        return;
    }

    // The repository refuses a kept buffer outright, so the confirmation cannot
    // be skipped by a UI path that forgets to ask (SPEC.md §6).
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
    toast_->offer(tr("Buffer moved to trash"), id);
}

void MainWindow::showContextMenu(int row, const QPoint& globalPos)
{
    const BufferId id = model_->idAt(row);
    if (id == kNoBuffer) return;
    const auto buffer = buffers_.find(id);
    if (!buffer) return;

    QMenu menu(this);
    if (model_->mode() == BufferListModel::Mode::Trash) {
        menu.addAction(tr("Restore"), this, [this, row] { trashRow(row); });
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
        if (view_->isEditing()) {
            autosave_->flushNow();                       // the text lands first
            if (editingBuffer_ == kNoBuffer) {           // an empty draft gets promoted
                Draft draft;
                draft.add(Item::makeImage(stored.hash, stored.size.width(), stored.size.height(),
                                          stored.byteSize, sourceName, stored.mime,
                                          stored.animated));
                editingBuffer_ = service_.commitDraft(draft);
                model_->promoteDraft(editingBuffer_);
            } else {
                service_.appendTo(editingBuffer_,
                    Item::makeImage(stored.hash, stored.size.width(), stored.size.height(),
                                    stored.byteSize, sourceName, stored.mime, stored.animated));
            }
            model_->invalidatePreview(editingBuffer_);
        } else {
            Draft draft;
            draft.add(Item::makeImage(stored.hash, stored.size.width(), stored.size.height(),
                                      stored.byteSize, sourceName, stored.mime, stored.animated));
            const BufferId id = service_.commitDraft(draft);
            model_->reload();
            if (const int row = model_->rowForId(id); row >= 0)
                view_->setCurrentIndex(model_->index(row, 0));
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
    // While an editor is focused the editor handles its own paste, so this
    // only fires for a paste onto the stack itself.
    if (view_->isEditing()) return;

    const auto content = readClipboard(QApplication::clipboard()->mimeData());
    switch (content.kind) {
    case ClipboardContent::Kind::Image:
        addImageToCurrent(content.imageBytes, content.imageMime, QString());
        break;
    case ClipboardContent::Kind::Text: {
        if (model_->mode() != BufferListModel::Mode::Live) return;
        Draft draft;
        draft.setText(content.text);
        const BufferId id = service_.commitDraft(draft);
        if (id == kNoBuffer) return;
        model_->reload();
        if (const int row = model_->rowForId(id); row >= 0)
            view_->setCurrentIndex(model_->index(row, 0));
        updateEmptyState();
        break;
    }
    case ClipboardContent::Kind::None:
        break;  // nothing usable; say nothing rather than nag
    }
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

void MainWindow::openImage(int row)
{
    const BufferId id = model_->idAt(row);
    if (id == kNoBuffer) return;

    for (const auto& item : items_.listForBuffer(id)) {
        if (item.type != ItemType::Image) continue;

        const QString path = blobs_.pathFor(item.blobHash, item.mime);
        if (!QFile::exists(path)) {
            reportProblem(tr("The image is missing"),
                          tr("Napkin can no longer find the file for this image. "
                             "The rest of the buffer is unchanged."));
            return;
        }
        Lightbox box(path, item.animated, item.sourceName, this);
        box.exec();
        return;
    }
}

void MainWindow::emptyTrash()
{
    const int count = buffers_.countTrash();
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
    reconcileBlobs(items_, blobs_);   // rows are gone; now reclaim their blobs
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
