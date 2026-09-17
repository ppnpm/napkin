#include "../src/data/BufferRepository.h"
#include "../src/data/Database.h"
#include "../src/data/ItemRepository.h"
#include "../src/domain/BufferService.h"
#include "../src/ui/BufferListModel.h"
#include "../src/ui/BufferListView.h"
#include "../src/ui/MainWindow.h"
#include "../src/ui/UndoToast.h"

#include <QPushButton>
#include <QtTest>

using namespace napkin;

// Phase 3 behaviour, driven through the real widget tree. Pin, Keep and Trash
// are where a mistake costs the user data, so these are the cases that matter
// most in the whole suite.
class TestLifecycle : public QObject {
    Q_OBJECT
private:
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

        BufferListModel* model() { return window.findChild<BufferListModel*>(); }
        BufferListView*  view()  { return window.findChild<BufferListView*>(); }
        UndoToast*       toast() { return window.findChild<UndoToast*>(); }

        BufferId seed(const char* text)
        {
            const auto id = buffers.create();
            service.appendTo(id, Item::makeText(QString::fromUtf8(text)));
            model()->reload();
            return id;
        }
    };

private slots:
    void pinMovesTheCardToTheTop()
    {
        Fixture f;
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
        Fixture f;
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
        Fixture f;
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
        Fixture f;
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
        Fixture f;
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
        Fixture f;
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
        Fixture f;
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
        Fixture f;
        const auto id = f.seed("deleted thing");
        f.window.trashRow(f.model()->rowForId(id));
        QCOMPARE(f.model()->rowCount(), 0);      // gone from the live stack

        f.window.showTrash(true);
        QCOMPARE(f.model()->mode(), BufferListModel::Mode::Trash);
        QCOMPARE(f.model()->rowCount(), 1);
        QCOMPARE(f.model()->index(0, 0).data(BufferListModel::SectionNameRole).toString(),
                 QStringLiteral("TRASH"));

        f.window.trashRow(0);                    // in trash mode this restores
        QCOMPARE(f.model()->rowCount(), 0);
        f.window.showTrash(false);
        QCOMPARE(f.model()->rowCount(), 1);
    }

    void pinAndKeepAreInertInTheTrashView()
    {
        Fixture f;
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
        Fixture f;
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
        Fixture f;
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
