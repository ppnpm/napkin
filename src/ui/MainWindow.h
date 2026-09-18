#pragma once
#include "../domain/Types.h"
#include <QList>
#include <functional>
#include <QMainWindow>

class QAction;
class QLabel;
class QPushButton;
class QStackedWidget;
class QMenu;
class QSplitter;
class QTimer;

namespace napkin {

class Autosave;
class UndoToast;
class BufferListModel;
class BufferListView;
class ItemCanvas;
class BufferRepository;
class BufferService;
class BlobStore;
class Database;
class ItemRepository;
class Thumbnailer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(Database& db, BufferRepository& buffers, ItemRepository& items,
               BufferService& service, BlobStore& blobs, Thumbnailer& thumbs,
               QWidget* parent = nullptr);

public slots:
    void raiseFromOtherInstance();

    // Public and named so it can be reached from a menu, a toolbar or a test,
    // rather than only through a key chord.
    void newDraft();
    void togglePin(int row);
    void toggleKeep(int row);
    void trashRow(int row);
    void showTrash(bool trash);
    void restoreRow(int row);
    void showShortcuts();
    void emptyTrashForTest();
    // The undo path normally runs from the toast; tests drive it directly.
    void undoLastTrashForTest(BufferId id, bool wasKept, Timestamp modifiedAt);
    void emptyTrash();
    void pasteFromClipboard();
    void addImageFromFile();
    void openImageItem(ItemId id);
    void openRow(int row);
    void selectBuffer(int row);
    void removeItems(const QList<ItemId>& ids);
    void appendTextBlock(const QString& text = {});

protected:
    void closeEvent(QCloseEvent* e) override;
    bool event(QEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void buildUi();
    QWidget* buildHeaderWidget();
    QMenu* buildOverflowMenu();
    // Returns false when the write failed. The caller must NOT collapse or close
    // on a false: doing so strands the text in a widget that is about to go
    // away, and the next flush returns early because nothing is being edited.
    bool flushEditor();
    bool flushAndReportFailure();
    void updateEmptyState();
    void showContextMenu(int row, const QPoint& globalPos);
    void reloadPreservingSelection();
    bool addImageToCurrent(const QByteArray& bytes, const QString& mime, const QString& sourceName);
    void reportProblem(const QString& title, const QString& detail);

    // Runs work that touches the database and turns a failure into a message
    // rather than a crash. An exception thrown inside a slot unwinds into Qt's
    // event loop, which calls std::terminate — so nothing that can throw may
    // reach it uncaught.
    bool guarded(const QString& title, const std::function<void()>& work);

    // editingBuffer_ names a row that may have been trashed or purged since it
    // was selected. Anything that writes to it must check first.
    bool currentBufferIsLive();

    Database&         db_;
    BufferRepository& buffers_;
    ItemRepository&   items_;
    BufferService&    service_;
    BlobStore&        blobs_;
    Thumbnailer&      thumbs_;

    BufferListModel* model_  = nullptr;
    BufferListView*  view_   = nullptr;
    ItemCanvas*      canvas_ = nullptr;
    QSplitter*       splitter_ = nullptr;
    QStackedWidget*  stack_  = nullptr;
    Autosave*        autosave_ = nullptr;
    UndoToast*       toast_ = nullptr;
    // What a trashed buffer looked like before it was trashed, so Undo can put
    // it back as it was rather than as a stripped copy of itself.
    struct TrashedState { BufferId id = kNoBuffer; bool kept = false; Timestamp modifiedAt = 0; };
    TrashedState lastTrashed_;
    int saveFailures_ = 0;
    QTimer*          timeRefresh_ = nullptr;
    QAction*         trashAction_ = nullptr;
    QPushButton*     emptyTrashButton_ = nullptr;
    QLabel*          emptyTitle_ = nullptr;
    QLabel*          emptyLine1_ = nullptr;
    QLabel*          emptyLine2_ = nullptr;

    // What the open editor is bound to. kNoBuffer means an uncommitted draft,
    // which by invariant 5 has no row in the database yet.
    BufferId editingBuffer_ = kNoBuffer;
    ItemId   editingItem_   = kNoItem;
};

}  // namespace napkin
