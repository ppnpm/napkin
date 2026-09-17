#pragma once
#include "../domain/Types.h"
#include <QMainWindow>

class QAction;
class QLabel;
class QStackedWidget;
class QTimer;

namespace napkin {

class Autosave;
class UndoToast;
class BufferListModel;
class BufferListView;
class BufferRepository;
class BufferService;
class Database;
class ItemRepository;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(Database& db, BufferRepository& buffers, ItemRepository& items,
               BufferService& service, QWidget* parent = nullptr);

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

protected:
    void closeEvent(QCloseEvent* e) override;
    bool event(QEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void buildUi();
    QWidget* buildHeaderWidget();
    void openRow(int row);
    void flushEditor();
    void updateEmptyState();
    void showContextMenu(int row, const QPoint& globalPos);
    void reloadPreservingSelection();

    Database&         db_;
    BufferRepository& buffers_;
    ItemRepository&   items_;
    BufferService&    service_;

    BufferListModel* model_  = nullptr;
    BufferListView*  view_   = nullptr;
    QStackedWidget*  stack_  = nullptr;
    Autosave*        autosave_ = nullptr;
    UndoToast*       toast_ = nullptr;
    QTimer*          timeRefresh_ = nullptr;
    QAction*         trashAction_ = nullptr;
    QLabel*          emptyTitle_ = nullptr;
    QLabel*          emptyLine1_ = nullptr;
    QLabel*          emptyLine2_ = nullptr;

    // What the open editor is bound to. kNoBuffer means an uncommitted draft,
    // which by invariant 5 has no row in the database yet.
    BufferId editingBuffer_ = kNoBuffer;
    ItemId   editingItem_   = kNoItem;
};

}  // namespace napkin
