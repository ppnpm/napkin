#include "GuiFixture.h"
#include "../src/ui/WelcomeView.h"
#include "../src/media/BlobGc.h"

#include <QBuffer>
#include <QLabel>
#include <QPushButton>
#include <QtTest>

using namespace napkin;

namespace {
QByteArray png(int w, int h, QColor c)
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

// The three problems the user reported: a buffer with several images showed
// only the first; a single click opened the editor so cards could not be
// selected and acted on; and the expanded editor showed a text box only, with
// no sign of the images the buffer actually held.
class TestMultiItem : public QObject {
    Q_OBJECT
private:
    BufferId seedMixed(GuiFixture& f, int images)
    {
        const auto id = f.buffers.create();
        f.service.appendTo(id, Item::makeText(QStringLiteral("Investigate this bug")));
        for (int i = 0; i < images; ++i) {
            const auto stored = f.blobs.store(png(60 + i, 40 + i, QColor::fromHsv(i * 40, 200, 220)));
            Q_ASSERT(stored.ok);
            f.service.appendTo(id, Item::makeImage(stored.hash, stored.size.width(),
                                                   stored.size.height(), stored.byteSize,
                                                   QStringLiteral("shot%1.png").arg(i),
                                                   stored.mime));
        }
        f.model()->reload();
        return id;
    }

private slots:
    // --- (a) several images are all visible ----------------------------------
    void aCardShowsSeveralThumbnailsNotJustTheFirst()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 4);
        const int row = f.model()->rowForId(id);

        QCOMPARE(f.model()->index(row, 0).data(BufferListModel::ImageCountRole).toInt(), 4);
        QCOMPARE(f.model()->index(row, 0).data(BufferListModel::ThumbCountRole).toInt(),
                 kMaxCardThumbs);
        QCOMPARE(int(f.model()->thumbsAt(row).size()), kMaxCardThumbs);

        // Distinct images, not the same one three times.
        const auto thumbs = f.model()->thumbsAt(row);
        QVERIFY(thumbs[0].hash != thumbs[1].hash);
        QVERIFY(thumbs[1].hash != thumbs[2].hash);
    }

    void aSingleImageStillGetsOneThumbnail()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 1);
        const int row = f.model()->rowForId(id);
        QCOMPARE(f.model()->index(row, 0).data(BufferListModel::ThumbCountRole).toInt(), 1);
        QCOMPARE(f.model()->index(row, 0).data(BufferListModel::ImageCountRole).toInt(), 1);
    }

    // --- (b) selecting a card no longer opens it -----------------------------
    void clickingSelectsTheRow()
    {
        GuiFixture f;
        const auto id = f.seed("selectable");
        const int row = f.model()->rowForId(id);

        const QRect rect = f.view()->visualRect(f.model()->index(row, 0));
        QTest::mouseClick(f.view()->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());

        QCOMPARE(f.view()->currentIndex().row(), row);
    }

    void aSelectedCardCanBeActedOnWithTheMouseAlone()
    {
        GuiFixture f;
        const auto id = f.seed("act on me");
        const int row = f.model()->rowForId(id);

        const QRect rect = f.view()->visualRect(f.model()->index(row, 0));
        QTest::mouseClick(f.view()->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());

        // This is the workflow that was impossible: click, then pin/keep/delete.
        f.window.togglePin(f.view()->currentIndex().row());
        QVERIFY(f.buffers.find(id)->pinned);
        f.window.toggleKeep(f.model()->rowForId(id));
        QVERIFY(f.buffers.find(id)->kept);
    }

    void selectingABufferShowsItInTheCanvas()
    {
        GuiFixture f;
        const auto id = f.seed("show me");
        f.select(id);

        const auto texts = f.canvas()->findChildren<TextItemCard*>();
        QCOMPARE(texts.size(), 1);
        QCOMPARE(texts[0]->text(), QStringLiteral("show me"));
    }

    void enterPutsTheCaretInTheCanvas()
    {
        GuiFixture f;
        const auto id = f.seed("keyboard");
        f.view()->setCurrentIndex(f.model()->index(f.model()->rowForId(id), 0));
        QTest::keyClick(f.view(), Qt::Key_Return);
        QVERIFY(f.window.focusWidget());
        QVERIFY(f.canvas()->isAncestorOf(f.window.focusWidget()));
    }

    // --- (c) the expanded editor shows every item ----------------------------
    void theExpandedEditorRendersEveryItem()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 3);   // 1 text + 3 images
        f.select(id);

        auto* editor = f.canvas();
        QVERIFY(editor);
        const auto images = editor->findChildren<ImageItemCard*>();
        const auto texts = editor->findChildren<TextItemCard*>();

        QCOMPARE(images.size(), 3);          // all three, not just the first
        QCOMPARE(texts.size(), 1);
        QCOMPARE(texts[0]->text(), QStringLiteral("Investigate this bug"));
    }

    void anImageCanBeRemovedFromInsideTheBuffer()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 2);
        f.select(id);

        const auto before = f.items.listForBuffer(id);
        QCOMPARE(int(before.size()), 3);
        const ItemId imageId = before[1].id;

        f.window.removeItems({imageId});

        const auto after = f.items.listForBuffer(id);
        QCOMPARE(int(after.size()), 2);
        for (const auto& item : after) QVERIFY(item.id != imageId);
        // And the editor redrew without it.
        QCOMPARE(f.canvas()->findChildren<ImageItemCard*>().size(), 1);
    }

    void theSweepReclaimsABlobOnceNothingReferencesIt()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 1);
        f.select(id);

        Item item;
        for (const auto& i : f.items.listForBuffer(id))
            if (i.type == ItemType::Image) item = i;
        QVERIFY(f.blobs.exists(item.blobHash, item.mime));

        f.window.removeItems({item.id});
        QVERIFY(f.blobs.exists(item.blobHash, item.mime));   // undo may still need it

        // A deleted item is in the trash, so a sweep must leave its image alone.
        reconcileBlobs(f.items, f.blobs, f.thumbsDir());
        QVERIFY(f.blobs.exists(item.blobHash, item.mime));

        // Emptying the trash is what finally lets it go.
        f.service.emptyTrash();
        reconcileBlobs(f.items, f.blobs, f.thumbsDir());
        QVERIFY(!f.blobs.exists(item.blobHash, item.mime));
    }

    void ctrlTAddsATextItem()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 1);
        f.select(id);

        QTest::keyClicks(f.newTextCard(), "a second thought");
        QTRY_VERIFY_WITH_TIMEOUT(f.items.countForBuffer(id) == 3, 2000);

        const auto items = f.items.listForBuffer(id);
        QCOMPARE(items.front().type, ItemType::Text);
        QCOMPARE(items.front().text, QStringLiteral("a second thought"));
    }

    void editingAnExistingTextItemDoesNotCreateADuplicate()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 0);
        f.select(id);

        // A block is read-only until you ask to edit it, so that a click can
        // select it. Double-click is the gesture; this is its keyboard twin.
        auto* card = f.canvas()->findChildren<TextItemCard*>()[0];
        card->beginEditing();
        QTest::keyClicks(card->findChild<QPlainTextEdit*>(), " - amended");
        QTest::qWait(600);

        QCOMPARE(f.items.countForBuffer(id), 1);   // updated in place
        QVERIFY(f.items.listForBuffer(id)[0].text.endsWith(QStringLiteral(" - amended")));
    }

    void continuedTypingAfterADraftCommitsLandsAtTheCaretNotTheStart()
    {
        // Regression: rebuilding the editor widgets after the first write reset
        // the caret to position 0, so the next keystrokes were prepended.
        GuiFixture f;
        f.trigger("newBufferAction");
        auto* edit = f.newTextCard();
        QTest::keyClicks(edit, "first");
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 2000);

        QTest::keyClicks(edit, " and second");
        QTest::qWait(600);

        const auto id = f.buffers.listLive(10).front().id;
        QCOMPARE(f.items.listForBuffer(id).front().text, QStringLiteral("first and second"));
        QCOMPARE(f.items.countForBuffer(id), 1);
    }

    // --- deleting items goes to the trash (usability test, 2026-09-19) -------
    // A new user deleted a note, missed the 8-second undo toast, and lost it:
    // napkins went to the trash but items went nowhere.
    QString toastText(GuiFixture& f)
    {
        for (auto* label : f.toast()->findChildren<QLabel*>())
            if (!label->text().isEmpty()) return label->text();
        return {};
    }

    void aDeletedItemIsInTheTrashAfterTheToastIsGone()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 2);
        f.select(id);
        const auto before = f.items.listForBuffer(id);
        const ItemId gone = before[1].id;

        f.window.removeItems({gone});
        QCOMPARE(int(f.items.listForBuffer(id).size()), 2);
        QCOMPARE(toastText(f), QStringLiteral("Item moved to trash"));

        // The toast expiring must not matter: the item is in a trashed napkin.
        const auto trash = f.buffers.listTrash();
        QCOMPARE(int(trash.size()), 1);
        const auto held = f.items.listForBuffer(trash.front().id);
        QCOMPARE(int(held.size()), 1);
        QCOMPARE(held.front().id, gone);

        // And restoring that napkin gives it back.
        f.service.restore(trash.front().id);
        QCOMPARE(f.buffers.countTrash(), 0);
        QCOMPARE(f.items.find(gone)->bufferId, trash.front().id);
    }

    void undoPutsTheItemBackWhereItWasAndLeavesNothingInTheTrash()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 2);
        f.select(id);
        const auto before = f.items.listForBuffer(id);

        f.window.removeItems({before[1].id});
        f.toast()->findChild<QPushButton*>()->click();

        const auto after = f.items.listForBuffer(id);
        QCOMPARE(int(after.size()), 3);
        for (size_t i = 0; i < before.size(); ++i) {
            QCOMPARE(after[i].id, before[i].id);             // same order
            QCOMPARE(after[i].position, before[i].position);
        }
        QCOMPARE(f.buffers.countTrash(), 0);                  // no stray holder
        QCOMPARE(f.buffers.countLive(), 1);
    }

    void deletingEveryItemTrashesTheNapkinWithItsContent()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 1);
        f.select(id);
        QList<ItemId> all;
        for (const auto& i : f.items.listForBuffer(id)) all << i.id;

        f.window.removeItems(all);
        QCOMPARE(toastText(f), QStringLiteral("Napkin moved to trash"));
        QCOMPARE(f.buffers.countTrash(), 1);
        QCOMPARE(f.buffers.listTrash().front().id, id);          // the napkin itself
        QCOMPARE(int(f.items.listForBuffer(id).size()), 2);      // not an empty shell
    }

    void cutDoesNotAnnounceItselfAsADeletion()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 1);
        f.select(id);
        f.window.removeItems({f.items.listForBuffer(id)[1].id}, /*cut=*/true);
        QCOMPARE(toastText(f), QStringLiteral("Item cut"));
    }

    // --- typing on an empty napkin starts a note --------------------------------
    void typingAfterCtrlNStartsANote()
    {
        GuiFixture f;
        f.trigger("newBufferAction");
        QTest::keyClicks(f.canvas(), "Call the dentist");
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 3000);
        const auto items = f.items.listForBuffer(f.buffers.listLive(5).front().id);
        QCOMPARE(int(items.size()), 1);
        QCOMPARE(items.front().text, QStringLiteral("Call the dentist"));
    }

    void typingWithTheListFocusedOnAnEmptyNapkinIsNotAListCommand()
    {
        // "P" and "K" are list commands; on an empty napkin they are letters.
        GuiFixture f;
        f.trigger("newBufferAction");
        QTest::keyClicks(f.view(), "Pack keys");
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 3000);
        const auto napkin = f.buffers.listLive(5).front();
        QVERIFY(!napkin.pinned);
        QVERIFY(!napkin.kept);
        QCOMPARE(f.items.listForBuffer(napkin.id).front().text, QStringLiteral("Pack keys"));
    }

    void pAndKStillPinAndKeepAFullNapkin()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 0);
        f.select(id);
        QTest::keyClick(f.view(), Qt::Key_P);
        QVERIFY(f.buffers.find(id)->pinned);
    }

    void doubleClickingAnEmptyBoardStartsANote()
    {
        GuiFixture f;
        f.trigger("newBufferAction");
        auto* viewport = f.canvas()->viewport();
        QTest::mouseDClick(viewport, Qt::LeftButton, {}, QPoint(viewport->width() / 2, viewport->height() - 20));
        auto* edit = f.editor();
        QVERIFY(edit);
        QTest::keyClicks(edit, "hello");
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 3000);
    }


    void typingOnTheWelcomeScreenStartsANote()
    {
        // The start page says "dump text here". Its first row is a focused
        // button, so letters vanished until a Space "clicked" it.
        GuiFixture f;
        auto* welcome = f.window.findChild<WelcomeView*>();
        QVERIFY(welcome);
        auto* row = welcome->findChild<QPushButton*>();
        QVERIFY(row);
        // The first letter is what reaches the start page; after it the
        // keyboard is in the new note, which is where the rest must land.
        QTest::keyClick(row, 'C', Qt::ShiftModifier);
        auto* note = f.editor();
        QVERIFY2(note, "the first letter did not start a note");
        QTest::keyClicks(note, "all the dentist");
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 3000);
        const auto items = f.items.listForBuffer(f.buffers.listLive(5).front().id);
        QCOMPARE(int(items.size()), 1);
        QCOMPARE(items.front().text, QStringLiteral("Call the dentist"));
    }

};

QTEST_MAIN(TestMultiItem)
#include "test_multiitem.moc"
