
#include <QPushButton>
#include "GuiFixture.h"
#include <QtTest>

using namespace napkin;

// Phase 3 behaviour, driven through the real widget tree. Pin, Keep and Trash
// are where a mistake costs the user data, so these are the cases that matter
// most in the whole suite.
class TestLifecycle : public QObject {
    Q_OBJECT
private slots:
    void pinMovesTheCardToTheTop()
    {
        GuiFixture f;
        f.seed("older");
        const auto target = f.seed("newer");
        const auto third = f.seed("newest");
        QCOMPARE(f.model()->idAt(0), third);

        f.window.togglePin(f.model()->rowForId(target));

        QCOMPARE(f.model()->idAt(0), target);
        QVERIFY(f.buffers.find(target)->pinned);
        QCOMPARE(f.model()->index(0, 0).data(BufferListModel::SectionNameRole).toString(),
                 QStringLiteral("PINNED"));
    }

    void keepDoesNotMoveTheCard()
    {
        GuiFixture f;
        const auto first = f.seed("first");
        const auto second = f.seed("second");
        QCOMPARE(f.model()->idAt(0), second);

        f.window.toggleKeep(f.model()->rowForId(first));

        // Keep is lifecycle, not placement. The list must not reshuffle.
        QCOMPARE(f.model()->idAt(0), second);
        QCOMPARE(f.model()->idAt(1), first);
        QVERIFY(f.buffers.find(first)->kept);
        QVERIFY(!f.buffers.find(first)->pinned);
    }

    void pinAndKeepStayIndependentThroughTheUi()
    {
        GuiFixture f;
        const auto id = f.seed("both");

        f.window.togglePin(f.model()->rowForId(id));
        f.window.toggleKeep(f.model()->rowForId(id));
        QVERIFY(f.buffers.find(id)->pinned);
        QVERIFY(f.buffers.find(id)->kept);

        f.window.togglePin(f.model()->rowForId(id));
        QVERIFY(!f.buffers.find(id)->pinned);
        QVERIFY(f.buffers.find(id)->kept);   // unpinning never releases the keep
    }

    void deletingAnOrdinaryBufferIsSoftAndOffersUndo()
    {
        GuiFixture f;
        const auto id = f.seed("throwaway");

        f.window.trashRow(f.model()->rowForId(id));

        QCOMPARE(f.buffers.countLive(), 0);
        QCOMPARE(f.buffers.countTrash(), 1);
        QVERIFY(f.buffers.find(id).has_value());        // nothing was destroyed
        QVERIFY(f.toast()->isVisible());
        QCOMPARE(f.toast()->pendingId(), id);
    }

    void undoBringsItBack()
    {
        GuiFixture f;
        const auto id = f.seed("mistake");
        f.window.trashRow(f.model()->rowForId(id));

        auto* undo = f.toast()->findChild<QPushButton*>();
        QVERIFY(undo);
        undo->click();

        QCOMPARE(f.buffers.countLive(), 1);
        QCOMPARE(f.buffers.countTrash(), 0);
        QCOMPARE(f.model()->rowCount(), 1);
        QVERIFY(!f.toast()->isVisible());
        QCOMPARE(f.items.listForBuffer(id).front().text, QStringLiteral("mistake"));
    }

    void deletingAKeptBufferIsRefusedWithoutConfirmation()
    {
        GuiFixture f;
        const auto id = f.seed("important");
        f.window.toggleKeep(f.model()->rowForId(id));

        // trashRow would raise a modal here, so exercise the layer it delegates
        // to: the repository refuses outright, which is what makes the
        // confirmation impossible to skip.
        QCOMPARE(f.service.trash(id), false);
        QCOMPARE(f.buffers.countTrash(), 0);
        QVERIFY(f.buffers.find(id)->kept);
    }

    void confirmingReleasesTheKeepAndTrashes()
    {
        GuiFixture f;
        const auto id = f.seed("important");
        f.window.toggleKeep(f.model()->rowForId(id));

        f.service.trashConfirmed(id);
        f.model()->reload();

        QVERIFY(f.buffers.find(id)->inTrash());
        QVERIFY(!f.buffers.find(id)->kept);
        QCOMPARE(f.model()->rowCount(), 0);
    }

    void trashViewListsDeletedBuffersAndRestores()
    {
        GuiFixture f;
        const auto id = f.seed("deleted thing");
        f.window.trashRow(f.model()->rowForId(id));
        QCOMPARE(f.model()->rowCount(), 0);      // gone from the live stack

        f.window.showTrash(true);
        QCOMPARE(f.model()->mode(), BufferListModel::Mode::Trash);
        QCOMPARE(f.model()->rowCount(), 1);
        QCOMPARE(f.model()->index(0, 0).data(BufferListModel::SectionNameRole).toString(),
                 QStringLiteral("TRASH"));

        // Delete in the trash now means delete, as it does in every file
        // manager; Restore is its own action, bound to R.
        f.window.restoreRow(0);
        QCOMPARE(f.model()->rowCount(), 0);
        f.window.showTrash(false);
        QCOMPARE(f.model()->rowCount(), 1);
    }

    void restoreIsItsOwnActionSeparateFromDelete()
    {
        GuiFixture f;
        const auto id = f.seed("recoverable");
        f.window.trashRow(f.model()->rowForId(id));
        f.window.showTrash(true);
        QCOMPARE(f.model()->rowCount(), 1);

        f.window.restoreRow(0);

        QCOMPARE(f.buffers.countTrash(), 0);
        QCOMPARE(f.buffers.countLive(), 1);
        QVERIFY(!f.buffers.find(id)->inTrash());
    }

    void pinAndKeepAreInertInTheTrashView()
    {
        GuiFixture f;
        const auto id = f.seed("deleted");
        f.window.trashRow(f.model()->rowForId(id));
        f.window.showTrash(true);

        f.window.togglePin(0);
        f.window.toggleKeep(0);

        QVERIFY(!f.buffers.find(id)->pinned);
        QVERIFY(!f.buffers.find(id)->kept);
    }

    void accessibleLabelCarriesStateNotJustStyling()
    {
        GuiFixture f;
        const auto id = f.seed("labelled");
        f.window.togglePin(f.model()->rowForId(id));
        f.window.toggleKeep(f.model()->rowForId(id));

        const QString label =
            f.model()->index(f.model()->rowForId(id), 0).data(Qt::AccessibleTextRole).toString();
        QVERIFY(label.contains(QStringLiteral("labelled")));
        QVERIFY(label.contains(QStringLiteral("Pinned")));
        QVERIFY(label.contains(QStringLiteral("Kept")));
    }

    void anotherDeleteReplacesTheStandingUndoOffer()
    {
        GuiFixture f;
        const auto first = f.seed("first");
        const auto second = f.seed("second");

        f.window.trashRow(f.model()->rowForId(first));
        QCOMPARE(f.toast()->pendingId(), first);
        f.window.trashRow(f.model()->rowForId(second));
        QCOMPARE(f.toast()->pendingId(), second);  // the most recent wins

        f.toast()->findChild<QPushButton*>()->click();
        QCOMPARE(f.buffers.countLive(), 1);
        QVERIFY(f.buffers.find(second)->deletedAt == std::nullopt);
    }
};

QTEST_MAIN(TestLifecycle)
#include "test_lifecycle.moc"
