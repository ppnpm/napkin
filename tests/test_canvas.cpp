#include "GuiFixture.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QMimeData>
#include <QSplitter>
#include <QtTest>

using namespace napkin;

namespace {
QByteArray png(int w, int h, QColor c = Qt::red)
{
    QImage image(w, h, QImage::Format_RGB32);
    image.fill(c);
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return out;
}
}  // namespace

// The two-pane layout: a buffer list on the left, its items on the right, where
// items are selectable objects you can copy, cut and delete.
class TestCanvas : public QObject {
    Q_OBJECT
private:
    BufferId seedMixed(GuiFixture& f)
    {
        const auto id = f.buffers.create();
        f.service.appendTo(id, Item::makeText(QStringLiteral("Investigate this bug")));
        for (int i = 0; i < 2; ++i) {
            const auto stored = f.blobs.store(png(80 + i, 60, QColor::fromHsv(i * 90, 200, 220)));
            f.service.appendTo(id, Item::makeImage(stored.hash, stored.size.width(),
                                                   stored.size.height(), stored.byteSize,
                                                   QStringLiteral("shot%1.png").arg(i),
                                                   stored.mime));
        }
        f.service.appendTo(id, Item::makeText(QStringLiteral("and a closing note")));
        f.model()->reload();
        return id;
    }

private slots:
    void bothPanesExist()
    {
        GuiFixture f;
        auto* splitter = f.window.findChild<QSplitter*>();
        QVERIFY(splitter);
        QCOMPARE(splitter->count(), 2);
        QVERIFY(f.canvas());
    }

    void selectingABufferFillsTheCanvasWithEveryItem()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        QCOMPARE(f.canvas()->findChildren<ImageItemCard*>().size(), 2);
        QCOMPARE(f.canvas()->findChildren<TextItemCard*>().size(), 3);  // 2 + composer
    }

    void selectingNothingShowsAPlaceholder()
    {
        GuiFixture f;
        f.seed("something");
        f.select(f.buffers.listLive(10).front().id);
        QVERIFY(!f.canvas()->findChildren<TextItemCard*>().isEmpty());

        f.view()->setCurrentIndex({});
        f.window.selectBuffer(-1);
        QVERIFY(f.canvas()->findChildren<TextItemCard*>().isEmpty());
    }

    // --- selection -----------------------------------------------------------
    void clickingAnImageSelectsItAsAnObject()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        auto* image = f.canvas()->findChildren<ImageItemCard*>().first();
        QTest::mouseClick(image, Qt::LeftButton);

        QCOMPARE(f.canvas()->selection().size(), 1);
        QCOMPARE(f.canvas()->selection().first(), image->itemId());
        QVERIFY(image->isSelected());
    }

    void ctrlClickAddsToTheSelection()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        const auto images = f.canvas()->findChildren<ImageItemCard*>();
        QTest::mouseClick(images[0], Qt::LeftButton);
        QTest::mouseClick(images[1], Qt::LeftButton, Qt::ControlModifier);

        QCOMPARE(f.canvas()->selection().size(), 2);
    }

    void selectionIsReturnedInDocumentOrder()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        const auto images = f.canvas()->findChildren<ImageItemCard*>();
        QTest::mouseClick(images[1], Qt::LeftButton);                      // later first
        QTest::mouseClick(images[0], Qt::LeftButton, Qt::ControlModifier);

        const auto order = f.canvas()->selection();
        QCOMPARE(order.size(), 2);
        QCOMPARE(order[0], images[0]->itemId());   // still in document order
    }

    void selectAllSkipsTheUnwrittenComposer()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        f.canvas()->selectAll();

        // 4 real items; the composer has no row and is not an object.
        QCOMPARE(f.canvas()->selection().size(), 4);
    }

    void escapeClearsTheSelection()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        f.canvas()->selectAll();
        QVERIFY(f.canvas()->hasSelection());

        QTest::keyClick(f.canvas(), Qt::Key_Escape);
        QVERIFY(!f.canvas()->hasSelection());
    }

    // --- copy / cut / delete -------------------------------------------------
    void copyingASingleImagePutsAnImageOnTheClipboard()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        QApplication::clipboard()->clear();

        auto* image = f.canvas()->findChildren<ImageItemCard*>().first();
        QTest::mouseClick(image, Qt::LeftButton);
        f.canvas()->copySelection();

        const auto* mime = QApplication::clipboard()->mimeData();
        QVERIFY(mime);               // null after a clear() on some platforms
        QVERIFY(mime->hasImage());   // pasteable into anything, not just Napkin
    }

    void copyingAMixedSelectionPutsTextOnTheClipboard()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        f.canvas()->selectAll();
        f.canvas()->copySelection();

        const QString text = QApplication::clipboard()->text();
        QVERIFY(text.contains(QStringLiteral("Investigate this bug")));
        QVERIFY(text.contains(QStringLiteral("shot0.png")));
        QVERIFY(text.contains(QStringLiteral("and a closing note")));
    }

    void deletingSelectedItemsRemovesThemFromTheBuffer()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        QCOMPARE(f.items.countForBuffer(id), 4);

        auto* image = f.canvas()->findChildren<ImageItemCard*>().first();
        QTest::mouseClick(image, Qt::LeftButton);
        f.canvas()->deleteSelection();

        QCOMPARE(f.items.countForBuffer(id), 3);
        QCOMPARE(f.canvas()->findChildren<ImageItemCard*>().size(), 1);
    }

    void cutCopiesThenRemoves()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        QApplication::clipboard()->clear();

        auto* image = f.canvas()->findChildren<ImageItemCard*>().first();
        QTest::mouseClick(image, Qt::LeftButton);
        f.canvas()->cutSelection();

        const auto* mime = QApplication::clipboard()->mimeData();
        QVERIFY(mime && mime->hasImage());
        QCOMPARE(f.items.countForBuffer(id), 3);
    }

    void deletingEveryItemTrashesTheBufferRatherThanLeavingAHusk()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        f.canvas()->selectAll();
        f.canvas()->deleteSelection();

        QCOMPARE(f.buffers.countLive(), 0);
        QCOMPARE(f.buffers.countTrash(), 1);
        QVERIFY(f.toast()->isVisible());     // and it is undoable
    }

    void removingAnImageReclaimsItsBlob()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        const auto image = f.items.listForBuffer(id)[1];
        QVERIFY(f.blobs.exists(image.blobHash, image.mime));

        f.window.removeItems({image.id});
        QVERIFY(!f.blobs.exists(image.blobHash, image.mime));
    }

    // --- editing -------------------------------------------------------------
    void typingIntoTheComposerAppendsANewTextItem()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        f.canvas()->focusComposer();
        QTest::keyClicks(f.editor(), "one more thought");
        QTRY_VERIFY_WITH_TIMEOUT(f.items.countForBuffer(id) == 5, 2000);
        QCOMPARE(f.items.listForBuffer(id).back().text, QStringLiteral("one more thought"));
    }

    void switchingBufferSavesTheOneYouAreLeaving()
    {
        GuiFixture f;
        const auto first = f.seed("first buffer");
        const auto second = f.seed("second buffer");

        f.select(first);
        f.canvas()->focusComposer();
        QTest::keyClicks(f.editor(), "an unsaved addition");
        f.select(second);   // inside the debounce window

        QTRY_VERIFY_WITH_TIMEOUT(f.items.countForBuffer(first) == 2, 2000);
        QCOMPARE(f.items.listForBuffer(first).back().text,
                 QStringLiteral("an unsaved addition"));
    }

    void theListDoesNotResortWhileYouTypeInTheCanvas()
    {
        GuiFixture f;
        const auto older = f.seed("older");
        const auto newer = f.seed("newer");
        QCOMPARE(f.model()->idAt(0), newer);

        f.select(older);
        f.canvas()->focusComposer();
        QTest::keyClicks(f.editor(), "edited");
        QTRY_VERIFY_WITH_TIMEOUT(f.items.countForBuffer(older) == 2, 2000);

        QCOMPARE(f.model()->idAt(0), newer);   // held while typing
        QVERIFY(f.model()->orderFrozen());
    }
};

QTEST_MAIN(TestCanvas)
#include "test_canvas.moc"
