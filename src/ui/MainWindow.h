#pragma once
#include "../domain/Types.h"
#include <QMainWindow>

class QAction;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace napkin {

class Autosave;
class UndoToast;
class BufferListModel;
class BufferListView;
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
    void collapseEditor();
    void togglePin(int row);
    void toggleKeep(int row);
    void trashRow(int row);
    void showTrash(bool trash);
    void emptyTrash();
    void pasteFromClipboard();
    void addImageFromFile();
    void openImageItem(ItemId id);
    void openRow(int row);
    void removeItemFromBuffer(ItemId id);

protected:
    void closeEvent(QCloseEvent* e) override;
    bool event(QEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void buildUi();
    QWidget* buildHeaderWidget();
    void flushEditor();
    void updateEmptyState();
    void showContextMenu(int row, const QPoint& globalPos);
    void reloadPreservingSelection();
    bool addImageToCurrent(const QByteArray& bytes, const QString& mime, const QString& sourceName);
    void reportProblem(const QString& title, const QString& detail);

    Database&         db_;
    BufferRepository& buffers_;
    ItemRepository&   items_;
    BufferService&    service_;
    BlobStore&        blobs_;
    Thumbnailer&      thumbs_;

    BufferListModel* model_  = nullptr;
    BufferListView*  view_   = nullptr;
    QStackedWidget*  stack_  = nullptr;
    Autosave*        autosave_ = nullptr;
    UndoToast*       toast_ = nullptr;
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
