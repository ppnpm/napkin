
#include <QAbstractItemView>
#include <QAction>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include "GuiFixture.h"
#include "../src/domain/Clock.h"
#include <QtTest>

using namespace napkin;

// Drives the real widget tree offscreen. These are the Phase 2 behaviours that
// only exist once the UI is wired up, so unit-testing the layers below would
// not have caught any of them.
class TestEditing : public QObject {
    Q_OBJECT
private slots:
    void ctrlNOpensAnEditorButWritesNothing()
    {
        GuiFixture f;
        f.trigger("newBufferAction");

        QVERIFY(f.newTextCard());
        QCOMPARE(f.model()->rowCount(), 1);   // a card is visible...
        QCOMPARE(f.buffers.countLive(), 0);   // ...but invariant 5 holds
    }

    void typingThenFlushingWritesExactlyOneBuffer()
    {
        GuiFixture f;
        f.trigger("newBufferAction");
        QTest::keyClicks(f.newTextCard(), "systemctl restart nginx");

        // Wait past the debounce; the autosave must fire on its own.
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 2000);
        QCOMPARE(f.model()->rowCount(), 1);

        const auto id = f.buffers.listLive(10).front().id;
        QCOMPARE(f.items.countForBuffer(id), 1);
        QCOMPARE(f.items.listForBuffer(id).front().text, QStringLiteral("systemctl restart nginx"));
    }

    void continuedTypingUpdatesTheSameBufferRatherThanMakingMore()
    {
        GuiFixture f;
        f.trigger("newBufferAction");
        auto* edit = f.newTextCard();
        QTest::keyClicks(edit, "first");
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 2000);

        QTest::keyClicks(edit, " and second");   // the same card, not a new one
        QTest::qWait(600);

        QCOMPARE(f.buffers.countLive(), 1);  // still one
        const auto id = f.buffers.listLive(10).front().id;
        QCOMPARE(f.items.listForBuffer(id).front().text, QStringLiteral("first and second"));
    }

    void anAbandonedEmptyDraftEvaporates()
    {
        GuiFixture f;
        const auto existing = f.seed("something else");
        f.trigger("newBufferAction");
        QCOMPARE(f.model()->rowCount(), 2);   // the draft card is showing

        // Selecting away from an empty draft discards it: it never had a row.
        f.select(existing);

        QCOMPARE(f.model()->rowCount(), 1);
        QCOMPARE(f.buffers.countLive(), 1);
    }

    void whitespaceOnlyIsNotContent()
    {
        GuiFixture f;
        f.trigger("newBufferAction");
        QTest::keyClicks(f.newTextCard(), "   \t  ");
        QTest::qWait(600);

        QCOMPARE(f.buffers.countLive(), 0);  // invariant 5
    }

    void leavingABufferFlushesItFirst()
    {
        GuiFixture f;
        const auto other = f.seed("other");
        f.trigger("newBufferAction");
        QTest::keyClicks(f.newTextCard(), "quick note");
        f.select(other);   // immediately, inside the debounce window

        // Moving on must not cost the user the last keystrokes (SPEC.md §8).
        QCOMPARE(f.buffers.countLive(), 2);
        bool found = false;
        for (const auto& b : f.buffers.listLive(10))
            for (const auto& item : f.items.listForBuffer(b.id))
                if (item.text == QStringLiteral("quick note")) found = true;
        QVERIFY(found);
    }

    void theListDoesNotResortWhileYouAreTyping()
    {
        qint64 clock = 1'700'000'000'000LL;
        setClockForTesting([&clock] { return clock; });

        GuiFixture f;
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
        f.view()->setCurrentIndex(f.model()->index(1, 0));
        f.model()->freezeOrder(true);
        clock += 60'000;
        const auto itemId = f.items.listForBuffer(older).front().id;
        f.service.updateTextItem(older, itemId, QStringLiteral("older buffer, edited"));
        f.model()->reload();

        QCOMPARE(f.model()->idAt(0), newer);   // order held while expanded
        QCOMPARE(f.model()->idAt(1), older);

        f.model()->freezeOrder(false);         // releasing applies the reload
        QCOMPARE(f.model()->idAt(0), older);   // and only now does it move
        QCOMPARE(f.model()->idAt(1), newer);

        resetClock();
    }

    void closingTheWindowFlushesPendingText()
    {
        GuiFixture f;
        f.trigger("newBufferAction");
        QTest::keyClicks(f.newTextCard(), "unsaved when closing");
        f.window.close();  // inside the debounce window

        QCOMPARE(f.buffers.countLive(), 1);
    }

    void emptyStateAppearsOnlyWhenThereIsNothing()
    {
        GuiFixture f;
        auto* stack = f.window.findChild<QStackedWidget*>();
        QVERIFY(stack);
        QCOMPARE(stack->currentIndex(), 1);  // empty state

        f.trigger("newBufferAction");
        QTest::keyClicks(f.newTextCard(), "content");
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 2000);
        QCOMPARE(stack->currentIndex(), 0);  // the stack
    }
};

QTEST_MAIN(TestEditing)
#include "test_editing.moc"
