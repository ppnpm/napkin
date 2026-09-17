#include "../src/data/BufferRepository.h"
#include "../src/data/Database.h"
#include "../src/data/ItemRepository.h"
#include "../src/domain/BufferService.h"
#include "../src/ui/BufferListModel.h"
#include "../src/ui/BufferListView.h"
#include "../src/ui/MainWindow.h"
#include "../src/domain/Clock.h"

#include <QAbstractItemView>
#include <QAction>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QtTest>

using namespace napkin;

// Drives the real widget tree offscreen. These are the Phase 2 behaviours that
// only exist once the UI is wired up, so unit-testing the layers below would
// not have caught any of them.
class TestEditing : public QObject {
    Q_OBJECT
private:
    // The database must be open before MainWindow is constructed: its model
    // queries on construction. A base class is initialized before members, so
    // this ordering is guaranteed rather than merely observed.
    struct OpenDb {
        Database db;
        OpenDb() { db.open(QStringLiteral(":memory:")); }
    };

    struct Fixture : OpenDb {
        BufferRepository buffers{db};
        ItemRepository items{db};
        BufferService service{db, buffers, items};
        MainWindow window{db, buffers, items, service};

        Fixture() { window.show(); }
        QPlainTextEdit* editor() { return window.findChild<QPlainTextEdit*>(); }
        BufferListModel* model() { return window.findChild<BufferListModel*>(); }
    };

    // Triggers the same action Ctrl+N is bound to. Headless platforms never
    // make a window active, so key-chord delivery cannot be relied on here;
    // the action is the unit under test either way.
    static void pressCtrlN(QWidget* w)
    {
        auto* action = w->findChild<QAction*>(QStringLiteral("newBufferAction"));
        QVERIFY(action);
        QCOMPARE(action->shortcut(), QKeySequence(QKeySequence::New));
        action->trigger();
    }

private slots:
    void ctrlNOpensAnEditorButWritesNothing()
    {
        Fixture f;
        pressCtrlN(&f.window);

        QVERIFY(f.editor());
        QVERIFY(f.editor()->isVisible());
        QCOMPARE(f.model()->rowCount(), 1);   // a card is visible...
        QCOMPARE(f.buffers.countLive(), 0);   // ...but invariant 5 holds
    }

    void typingThenFlushingWritesExactlyOneBuffer()
    {
        Fixture f;
        pressCtrlN(&f.window);
        QTest::keyClicks(f.editor(), "systemctl restart nginx");

        // Wait past the debounce; the autosave must fire on its own.
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 2000);
        QCOMPARE(f.model()->rowCount(), 1);

        const auto id = f.buffers.listLive(10).front().id;
        QCOMPARE(f.items.countForBuffer(id), 1);
        QCOMPARE(f.items.listForBuffer(id).front().text, QStringLiteral("systemctl restart nginx"));
    }

    void continuedTypingUpdatesTheSameBufferRatherThanMakingMore()
    {
        Fixture f;
        pressCtrlN(&f.window);
        QTest::keyClicks(f.editor(), "first");
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 2000);

        QTest::keyClicks(f.editor(), " and second");
        QTest::qWait(600);

        QCOMPARE(f.buffers.countLive(), 1);  // still one
        const auto id = f.buffers.listLive(10).front().id;
        QCOMPARE(f.items.listForBuffer(id).front().text, QStringLiteral("first and second"));
    }

    void anAbandonedEmptyDraftEvaporates()
    {
        Fixture f;
        pressCtrlN(&f.window);
        QCOMPARE(f.model()->rowCount(), 1);

        QTest::keyClick(f.editor(), Qt::Key_Escape);

        QCOMPARE(f.model()->rowCount(), 0);  // the card is gone
        QCOMPARE(f.buffers.countLive(), 0);  // and nothing was ever written
    }

    void whitespaceOnlyIsNotContent()
    {
        Fixture f;
        pressCtrlN(&f.window);
        QTest::keyClicks(f.editor(), "   \t  ");
        QTest::qWait(600);

        QCOMPARE(f.buffers.countLive(), 0);  // invariant 5
    }

    void escapeFlushesBeforeCollapsing()
    {
        Fixture f;
        pressCtrlN(&f.window);
        QTest::keyClicks(f.editor(), "quick note");
        QTest::keyClick(f.editor(), Qt::Key_Escape);  // immediately, inside the debounce

        // Collapsing must not cost the user the last keystrokes (SPEC.md §8).
        QCOMPARE(f.buffers.countLive(), 1);
        QCOMPARE(f.items.listForBuffer(f.buffers.listLive(10).front().id).front().text,
                 QStringLiteral("quick note"));
    }

    void theListDoesNotResortWhileYouAreTyping()
    {
        qint64 clock = 1'700'000'000'000LL;
        setClockForTesting([&clock] { return clock; });

        Fixture f;
        const auto older = f.buffers.create();
        f.service.appendTo(older, Item::makeText(QStringLiteral("older buffer")));
        clock += 60'000;
        const auto newer = f.buffers.create();
        f.service.appendTo(newer, Item::makeText(QStringLiteral("newer buffer")));

        f.model()->reload();
        QCOMPARE(f.model()->idAt(0), newer);
        QCOMPARE(f.model()->idAt(1), older);

        // Open the older card and edit it. Autosave bumps modified_at past
        // newer's, so a naive reload would yank the card you are typing into
        // to the top of the list (SPEC.md §7).
        f.model()->setExpandedRow(1);
        clock += 60'000;
        const auto itemId = f.items.listForBuffer(older).front().id;
        f.service.updateTextItem(older, itemId, QStringLiteral("older buffer, edited"));
        f.model()->reload();

        QCOMPARE(f.model()->idAt(0), newer);   // order held while expanded
        QCOMPARE(f.model()->idAt(1), older);

        f.model()->setExpandedRow(-1);         // collapsing applies the reload
        QCOMPARE(f.model()->idAt(0), older);   // and only now does it move
        QCOMPARE(f.model()->idAt(1), newer);

        resetClock();
    }

    void closingTheWindowFlushesPendingText()
    {
        Fixture f;
        pressCtrlN(&f.window);
        QTest::keyClicks(f.editor(), "unsaved when closing");
        f.window.close();  // inside the debounce window

        QCOMPARE(f.buffers.countLive(), 1);
    }

    void emptyStateAppearsOnlyWhenThereIsNothing()
    {
        Fixture f;
        auto* stack = f.window.findChild<QStackedWidget*>();
        QVERIFY(stack);
        QCOMPARE(stack->currentIndex(), 1);  // empty state

        pressCtrlN(&f.window);
        QTest::keyClicks(f.editor(), "content");
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 2000);
        QCOMPARE(stack->currentIndex(), 0);  // the stack
    }
};

QTEST_MAIN(TestEditing)
#include "test_editing.moc"
