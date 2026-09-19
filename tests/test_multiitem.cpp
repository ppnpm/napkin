#include "GuiFixture.h"
#include "../src/ui/WelcomeView.h"
#include "../src/media/BlobGc.h"

#include <QBuffer>
#include <QLabel>
#include <QPushButton>
#include <QClipboard>
#include <QLineEdit>
#include <QMimeData>
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
    void aCardShowsItsThumbnailLimitAndCountsEveryImage()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 4);
        const int row = f.model()->rowForId(id);

        QCOMPARE(f.model()->index(row, 0).data(BufferListModel::ImageCountRole).toInt(), 4);
        QCOMPARE(f.model()->index(row, 0).data(BufferListModel::ThumbCountRole).toInt(),
                 kMaxCardThumbs);
        QCOMPARE(int(f.model()->thumbsAt(row).size()), kMaxCardThumbs);

        // Whatever is shown is distinct images, not one image repeated. Written
        // for any kMaxCardThumbs: this used to read thumbs[1] and thumbs[2]
        // outright, and kept doing so after the limit became 1 (8646fdb) —
        // reading past the end of the vector, silently, until an Arch build
        // with _GLIBCXX_ASSERTIONS aborted on it.
        const auto thumbs = f.model()->thumbsAt(row);
        for (size_t k = 1; k < thumbs.size(); ++k)
            QVERIFY(thumbs[k - 1].hash != thumbs[k].hash);
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

        // And restoring it puts the item back where it came from — not into a
        // napkin of its own, which the second usability test found confusing.
        const Item before1 = before[1];
        QCOMPARE(f.service.restore(trash.front().id), id);
        QCOMPARE(f.buffers.countTrash(), 0);
        QCOMPARE(f.buffers.countLive(), 1);                       // no stray holder
        QCOMPARE(f.items.find(gone)->bufferId, id);
        QCOMPARE(f.items.find(gone)->position, before1.position);
        QCOMPARE(int(f.items.listForBuffer(id).size()), 3);
    }

    void aDeletedItemWhoseNapkinIsGoneRestoresOnItsOwn()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 2);
        f.select(id);
        const ItemId gone = f.items.listForBuffer(id)[1].id;
        f.window.removeItems({gone});
        f.service.trash(id);                                      // the original goes too
        BufferId holder = kNoBuffer;
        for (const auto& b : f.buffers.listTrash()) if (b.id != id) holder = b.id;
        QVERIFY(holder != kNoBuffer);

        QCOMPARE(f.service.restore(holder), holder);              // nowhere to go back to
        QCOMPARE(f.items.find(gone)->bufferId, holder);
        QVERIFY(!f.buffers.restoresTo(holder));                   // and it is its own napkin now
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

    void typingOnAFullNapkinInTheListWritesOnItAndPinsNothing()
    {
        // Second usability test: typing "pack" into the list pinned AND kept
        // the napkin, silently. Letters are text now, everywhere.
        GuiFixture f;
        const auto id = seedMixed(f, 0);
        f.select(id);
        QTest::keyClick(f.view(), 'p');
        auto* note = f.editor();
        QVERIFY2(note, "typing did not start a note");
        QTest::keyClicks(note, "ack the charger");
        QTRY_COMPARE_WITH_TIMEOUT(int(f.items.listForBuffer(id).size()), 2, 3000);
        QVERIFY(!f.buffers.find(id)->pinned);
        QVERIFY(!f.buffers.find(id)->kept);
        bool found = false;
        for (const auto& i : f.items.listForBuffer(id)) found |= i.text == QStringLiteral("pack the charger");
        QVERIFY(found);
    }

    void pinAndKeepHaveShortcutsAToastAndUndo()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 0);
        f.select(id);
        f.trigger("pinAction");
        QVERIFY(f.buffers.find(id)->pinned);
        QCOMPARE(toastText(f), QStringLiteral("Pinned — it stays at the top"));
        f.trigger("undoAction");                     // Ctrl+Z
        QVERIFY(!f.buffers.find(id)->pinned);

        f.trigger("keepAction");
        QVERIFY(f.buffers.find(id)->kept);
        QVERIFY(toastText(f).startsWith(QStringLiteral("Kept")));
        auto* pinAction = f.window.findChild<QAction*>(QStringLiteral("pinAction"));
        auto* keepAction = f.window.findChild<QAction*>(QStringLiteral("keepAction"));
        QCOMPARE(pinAction->shortcut(), QKeySequence(QStringLiteral("Ctrl+P")));
        QCOMPARE(keepAction->shortcut(), QKeySequence(QStringLiteral("Ctrl+D")));
    }

    void ctrlZUndoesADeleteAfterTheToastWasMissed()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 1);
        f.select(id);
        const ItemId gone = f.items.listForBuffer(id)[1].id;
        f.window.removeItems({gone});
        f.trigger("undoAction");
        QCOMPARE(f.items.find(gone)->bufferId, id);
        QCOMPARE(f.buffers.countTrash(), 0);
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


    // --- a completed cut leaves nothing in the trash (second usability test) --
    void aPastedCutLeavesNothingInTheTrash()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 1);
        f.select(id);
        const Item note = f.items.listForBuffer(id).back();      // the text, oldest
        QCOMPARE(note.type, ItemType::Text);
        QApplication::clipboard()->setText(note.text);           // what the cut copied
        f.window.removeItems({note.id}, /*cut=*/true);
        QCOMPARE(f.buffers.countTrash(), 1);

        f.trigger("newBufferAction");
        f.trigger("pasteAction");
        QCOMPARE(f.buffers.countTrash(), 0);                     // the original is gone
        QCOMPARE(f.buffers.countLive(), 2);                      // and the copy is here
    }

    void aMixedCutKeepsItsOriginalsBecauseThePasteIsOnlyText()
    {
        // A selection of text AND an image copies as text alone. Discarding the
        // originals after that paste would lose the image.
        GuiFixture f;
        const auto id = seedMixed(f, 1);
        f.service.appendTo(id, Item::makeText(QStringLiteral("second note")));
        f.model()->reload();
        f.select(id);
        QList<ItemId> cut;
        for (const auto& i : f.items.listForBuffer(id))
            if (i.type == ItemType::Image || i.text == QStringLiteral("second note")) cut << i.id;
        QApplication::clipboard()->setText(QStringLiteral("second note"));
        f.window.removeItems(cut, /*cut=*/true);

        f.trigger("newBufferAction");
        f.trigger("pasteAction");
        QCOMPARE(f.buffers.countTrash(), 1);                     // the image is still recoverable
    }

    void aCutIsKeptIfTheClipboardHasMovedOn()
    {
        GuiFixture f;
        const auto id = seedMixed(f, 1);
        f.select(id);
        const Item note = f.items.listForBuffer(id).back();
        QApplication::clipboard()->setText(note.text);
        f.window.removeItems({note.id}, /*cut=*/true);
        QApplication::clipboard()->setText(QStringLiteral("copied elsewhere since"));
        f.trigger("newBufferAction");
        f.trigger("pasteAction");
        QCOMPARE(f.buffers.countTrash(), 1);                     // not what was cut: keep it
    }

    void pastingWhileLookingAtTheTrashMakesANewNapkin()
    {
        GuiFixture f;
        const auto id = f.seed("old");
        f.service.trash(id);
        f.model()->reload();
        f.window.showTrash(true);
        QApplication::clipboard()->setText(QStringLiteral("fresh thought"));
        f.trigger("pasteAction");
        QCOMPARE(f.model()->mode(), BufferListModel::Mode::Live);
        QCOMPARE(f.buffers.countLive(), 1);
    }


    // --- Ctrl+V with the search box focused ----------------------------------
    void pastingTextIntoAnEmptySearchBoxPutsItOnTheNapkin()
    {
        GuiFixture f;
        auto* search = f.window.findChild<QLineEdit*>(QStringLiteral("searchField"));
        QApplication::clipboard()->setText(QStringLiteral("Remember the milk"));
        QTest::keyClick(search, Qt::Key_V, Qt::ControlModifier);
        QVERIFY(search->text().isEmpty());                        // not a search for it
        QCOMPARE(f.buffers.countLive(), 1);
        const auto id = f.buffers.listLive(5).front().id;
        QCOMPARE(f.items.listForBuffer(id).front().text, QStringLiteral("Remember the milk"));
    }

    void pastingAnImageWithTheSearchBoxFocusedPutsItOnTheNapkin()
    {
        GuiFixture f;
        auto* search = f.window.findChild<QLineEdit*>(QStringLiteral("searchField"));
        auto* mime = new QMimeData;
        mime->setData(QStringLiteral("image/png"), png(40, 30, Qt::red));
        QApplication::clipboard()->setMimeData(mime);
        QTest::keyClick(search, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(f.buffers.countLive(), 1);                       // it used to vanish
        QCOMPARE(f.items.listForBuffer(f.buffers.listLive(5).front().id).front().type, ItemType::Image);
    }

    void pastingIntoAQueryBeingEditedStillEditsTheQuery()
    {
        GuiFixture f;
        f.seed("something");
        auto* search = f.window.findChild<QLineEdit*>(QStringLiteral("searchField"));
        search->setText(QStringLiteral("some"));
        QApplication::clipboard()->setText(QStringLiteral("thing"));
        QTest::keyClick(search, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(search->text(), QStringLiteral("something"));
        QCOMPARE(f.buffers.countLive(), 1);                       // nothing new was made
    }


    void ctrlShiftVPastesIntoTheSearchBox()
    {
        GuiFixture f;
        f.seed("buy more milk");
        auto* search = f.window.findChild<QLineEdit*>(QStringLiteral("searchField"));
        QApplication::clipboard()->setText(QStringLiteral("more\nmilk"));
        QTest::keyClick(search, Qt::Key_V, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(search->text(), QStringLiteral("more milk"));    // one line
        QCOMPARE(f.buffers.countLive(), 1);                       // nothing was pasted as an item
    }

};

QTEST_MAIN(TestMultiItem)
#include "test_multiitem.moc"
