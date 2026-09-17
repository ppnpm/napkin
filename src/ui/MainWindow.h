#pragma once
#include "../domain/Types.h"
#include <QMainWindow>

class QLabel;
class QStackedWidget;
class QTimer;

namespace napkin {

class Autosave;
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

protected:
    void closeEvent(QCloseEvent* e) override;
    bool event(QEvent* e) override;

private:
    void buildUi();
    void openRow(int row);
    void flushEditor();
    void updateEmptyState();

    Database&         db_;
    BufferRepository& buffers_;
    ItemRepository&   items_;
    BufferService&    service_;

    BufferListModel* model_  = nullptr;
    BufferListView*  view_   = nullptr;
    QStackedWidget*  stack_  = nullptr;
    Autosave*        autosave_ = nullptr;
    QTimer*          timeRefresh_ = nullptr;

    // What the open editor is bound to. kNoBuffer means an uncommitted draft,
    // which by invariant 5 has no row in the database yet.
    BufferId editingBuffer_ = kNoBuffer;
    ItemId   editingItem_   = kNoItem;
};

}  // namespace napkin
