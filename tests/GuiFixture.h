#pragma once
#include "../src/data/BufferRepository.h"
#include "../src/data/Database.h"
#include "../src/data/ItemRepository.h"
#include "../src/domain/BufferService.h"
#include "../src/media/BlobStore.h"
#include "../src/media/Thumbnailer.h"
#include "../src/ui/BufferListModel.h"
#include "../src/ui/BufferListView.h"
#include "../src/ui/MainWindow.h"
#include "../src/ui/UndoToast.h"

#include <QAction>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QtTest>

// Shared by the GUI suites. The database must be open before MainWindow is
// constructed — its model queries on construction — and a base class is
// initialized before members, so the ordering is guaranteed rather than merely
// observed. Blobs go to a temporary directory; no test touches real user data.
struct GuiFixture {
private:
    struct Base {
        QTemporaryDir dir;
        napkin::Database db;
        Base() { db.open(QStringLiteral(":memory:")); }
    };
    Base base_;

public:
    napkin::Database& db = base_.db;
    napkin::BufferRepository buffers{db};
    napkin::ItemRepository items{db};
    napkin::BufferService service{db, buffers, items};
    napkin::BlobStore blobs{base_.dir.path() + "/blobs"};
    napkin::Thumbnailer thumbs{base_.dir.path() + "/thumbs", blobs};
    napkin::MainWindow window{db, buffers, items, service, blobs, thumbs};

    GuiFixture() { window.show(); }

    napkin::BufferListModel* model() { return window.findChild<napkin::BufferListModel*>(); }
    napkin::BufferListView*  view()  { return window.findChild<napkin::BufferListView*>(); }
    napkin::UndoToast*       toast() { return window.findChild<napkin::UndoToast*>(); }
    QPlainTextEdit*          editor() { return window.findChild<QPlainTextEdit*>(); }

    napkin::BufferId seed(const char* text)
    {
        const auto id = buffers.create();
        service.appendTo(id, napkin::Item::makeText(QString::fromUtf8(text)));
        model()->reload();
        return id;
    }

    // Triggers the action a shortcut is bound to. Headless platforms never make
    // a window active, so key-chord delivery cannot be relied on here; the
    // action is the unit under test either way.
    void trigger(const char* actionName)
    {
        auto* action = window.findChild<QAction*>(QString::fromLatin1(actionName));
        QVERIFY(action);
        action->trigger();
    }
};
