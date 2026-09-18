#include "GuiFixture.h"
#include "../src/media/BlobGc.h"
#include "../src/ui/Tokens.h"
#include "../src/ui/CardFooter.h"

#include <QApplication>
#include <QScrollBar>
#include <QBuffer>
#include <QClipboard>
#include <QMimeData>
#include <QPushButton>
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
        QCOMPARE(f.canvas()->findChildren<TextItemCard*>().size(), 2);  // no composer
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

    void removingAnImageKeepsItsBlobUntilTheSweep()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        const auto image = f.items.listForBuffer(id)[1];
        QVERIFY(f.blobs.exists(image.blobHash, image.mime));

        f.window.removeItems({image.id});

        // Deliberately still there: a blob whose last reference just went is
        // exactly the one the 8-second undo is about to need. Unlinking here is
        // what made undo restore rows pointing at deleted files.
        QVERIFY(f.blobs.exists(image.blobHash, image.mime));

        // It is the startup sweep that reclaims it, once undo is no longer on
        // offer.
        reconcileBlobs(f.items, f.blobs, f.thumbsDir());
        QVERIFY(!f.blobs.exists(image.blobHash, image.mime));
    }

    // Removing an item hard-deletes its row and used to unlink its blob at once.
    // If that emptied the buffer, the buffer was trashed with an undo offer —
    // and undo handed back a buffer whose items no longer existed.
    void undoAfterRemovingEveryItemGivesTheContentBackNotAnEmptyShell()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        const auto blobBefore = f.items.listForBuffer(id)[1];
        f.select(id);

        f.canvas()->selectAll();
        f.canvas()->deleteSelection();
        QCOMPARE(f.buffers.countTrash(), 1);

        auto* undo = f.toast()->findChild<QPushButton*>();
        QVERIFY(undo);
        undo->click();

        QCOMPARE(f.buffers.countLive(), 1);
        QCOMPARE(f.items.countForBuffer(id), 4);          // the items came back
        QVERIFY(f.blobs.exists(blobBefore.blobHash, blobBefore.mime));   // and their blobs
    }

    void undoAfterRemovingOneItemPutsItBack()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        auto* image = f.canvas()->findChildren<ImageItemCard*>().first();
        const auto removed = *f.items.find(image->itemId());
        QTest::mouseClick(image, Qt::LeftButton);
        f.canvas()->deleteSelection();
        QCOMPARE(f.items.countForBuffer(id), 3);

        auto* undo = f.toast()->findChild<QPushButton*>();
        QVERIFY(undo);
        undo->click();

        QCOMPARE(f.items.countForBuffer(id), 4);
        QVERIFY(f.blobs.exists(removed.blobHash, removed.mime));
    }

    // --- the four inconsistencies the user reported --------------------------
    void pastingTextGoesIntoTheSelectedBufferJustLikeAnImage()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        QApplication::clipboard()->setText(QStringLiteral("pasted note"));

        f.trigger("pasteAction");

        // Text used to always make a NEW buffer while images appended to the
        // selected one: the same gesture doing two different things.
        QCOMPARE(f.buffers.countLive(), 1);
        QCOMPARE(f.items.countForBuffer(id), 5);
        // Newest first: the thing you just pasted is at the top of the board.
        QCOMPARE(f.items.listForBuffer(id).front().text, QStringLiteral("pasted note"));
    }

    void pastingTextWithNothingSelectedMakesOneBuffer()
    {
        GuiFixture f;
        QApplication::clipboard()->setText(QStringLiteral("a stray thought"));

        f.trigger("pasteAction");

        QCOMPARE(f.buffers.countLive(), 1);
        QCOMPARE(f.items.countForBuffer(f.buffers.listLive(10).front().id), 1);
    }

    void deletingAnItemLeavesTheNextOneSelected()
    {
        GuiFixture f;
        const auto id = seedMixed(f);   // text, image, image, text
        f.select(id);

        const auto items = f.items.listForBuffer(id);
        auto* second = f.canvas()->findChildren<ImageItemCard*>().first();
        QTest::mouseClick(second, Qt::LeftButton);
        f.canvas()->deleteSelection();

        // Selection lands on whatever now occupies that slot, so a run of
        // deletes does not require re-aiming the mouse each time.
        QCOMPARE(f.canvas()->selection().size(), 1);
        QCOMPARE(f.canvas()->selection().first(), items[2].id);
    }

    void deletingTheLastItemSelectsTheNewLast()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        // Delete the card at the end of the board.
        const auto ordered = f.items.listForBuffer(id);
        f.window.removeItems({ordered.back().id});

        QCOMPARE(f.canvas()->selection().size(), 1);
        QCOMPARE(f.canvas()->selection().first(), ordered[ordered.size() - 2].id);
    }

    void aSingleClickSelectsATextBlockRatherThanEditingIt()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        auto* text = f.canvas()->findChildren<TextItemCard*>().first();
        QTest::mouseClick(text->findChild<QPlainTextEdit*>(), Qt::LeftButton);

        QCOMPARE(f.canvas()->selection().size(), 1);
        QCOMPARE(f.canvas()->selection().first(), text->itemId());
        QVERIFY(!text->hasEditFocus());
    }

    void aSelectedTextBlockCanBeDeletedWithTheMouseAlone()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        const int before = f.items.countForBuffer(id);

        auto* text = f.canvas()->findChildren<TextItemCard*>().first();
        QTest::mouseClick(text->findChild<QPlainTextEdit*>(), Qt::LeftButton);
        f.canvas()->deleteSelection();

        QCOMPARE(f.items.countForBuffer(id), before - 1);
    }

    void doubleClickingATextBlockStartsEditingIt()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        auto* text = f.canvas()->findChildren<TextItemCard*>().first();
        QTest::mouseDClick(text->findChild<QPlainTextEdit*>(), Qt::LeftButton);

        QVERIFY(text->hasEditFocus());
        QVERIFY(!f.canvas()->hasSelection());   // editing and selecting are exclusive
    }

    void ctrlTAddsATextBlockToTheSelectedBuffer()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        f.trigger("addTextAction");
        QTest::keyClicks(f.editor(), "written after Ctrl+T");
        QTRY_VERIFY_WITH_TIMEOUT(f.items.countForBuffer(id) == 5, 2000);
        QCOMPARE(f.items.listForBuffer(id).front().text,
                 QStringLiteral("written after Ctrl+T"));
    }

    // --- crashes reported from real use --------------------------------------
    void pastingAfterTheSelectedBufferIsPurgedDoesNotCrash()
    {
        // editingBuffer_ kept naming a row that trashing, then Empty trash, had
        // removed. Appending to it violated the foreign key, and the DbError
        // unwound into Qt's event loop, which calls std::terminate.
        GuiFixture f;
        QApplication::clipboard()->setText(QStringLiteral("first"));
        f.trigger("pasteAction");
        const auto id = f.buffers.listLive(10).front().id;

        f.window.trashRow(f.model()->rowForId(id));
        f.window.emptyTrashForTest();
        QVERIFY(!f.buffers.find(id).has_value());

        QApplication::clipboard()->setText(QStringLiteral("second"));
        f.trigger("pasteAction");

        // A fresh buffer holding the new text, rather than a crash. (SQLite
        // reuses rowids after a delete, so the id may well be the same one.)
        QCOMPARE(f.buffers.countLive(), 1);
        const auto fresh = f.buffers.listLive(10).front().id;
        QCOMPARE(f.items.countForBuffer(fresh), 1);
        QCOMPARE(f.items.listForBuffer(fresh).front().text, QStringLiteral("second"));
    }

    void pastingAfterTheSelectedBufferIsTrashedStartsAFreshOne()
    {
        // Same stale reference, milder symptom: the paste landed inside the
        // trashed buffer, so the text vanished from view while quietly
        // accumulating somewhere the user could not see.
        GuiFixture f;
        QApplication::clipboard()->setText(QStringLiteral("first"));
        f.trigger("pasteAction");
        const auto id = f.buffers.listLive(10).front().id;
        const int itemsBefore = f.items.countForBuffer(id);

        f.window.trashRow(f.model()->rowForId(id));
        QApplication::clipboard()->setText(QStringLiteral("second"));
        f.trigger("pasteAction");

        QCOMPARE(f.items.countForBuffer(id), itemsBefore);   // the dead one is untouched
        QCOMPARE(f.buffers.countLive(), 1);
    }

    void aLiveBufferNeverHasZeroItems()
    {
        GuiFixture f;
        QApplication::clipboard()->setText(QStringLiteral("only item"));
        f.trigger("pasteAction");
        const auto id = f.buffers.listLive(10).front().id;

        f.canvas()->selectAll();
        f.canvas()->deleteSelection();

        // The card used to stay in the list with no items, still showing the
        // text it no longer contained.
        QCOMPARE(f.model()->rowCount(), 0);
        for (const auto& b : f.buffers.listLive(100))
            QVERIFY(f.items.countForBuffer(b.id) > 0);
        QVERIFY(f.buffers.find(id)->inTrash());
    }

    void deleteAllThenUndoThenDeleteAgainSurvives()
    {
        GuiFixture f;
        QApplication::clipboard()->setText(QStringLiteral("resilient"));
        f.trigger("pasteAction");
        const auto id = f.buffers.listLive(10).front().id;

        f.canvas()->selectAll();
        f.canvas()->deleteSelection();
        f.toast()->findChild<QPushButton*>()->click();
        QCOMPARE(f.items.countForBuffer(id), 1);

        f.select(id);
        f.canvas()->selectAll();
        f.canvas()->deleteSelection();
        QCOMPARE(f.buffers.countLive(), 0);
    }

    // --- card chrome and sizing ----------------------------------------------
    void aPastedParagraphGetsACardTallEnoughToReadIt()
    {
        GuiFixture f;
        QApplication::clipboard()->setText(QStringLiteral(
            "This is some of the multilined text that got accumulated into lines and can "
            "be showcased, and this has to be a very cool napkin where you can store "
            "things temporarily without having to name them."));
        f.trigger("pasteAction");

        auto* card = f.canvas()->findChildren<TextItemCard*>().first();
        auto* edit = card->findChild<QPlainTextEdit*>();

        // The card must be tall enough that the text does not need scrolling:
        // a paragraph that wraps to five lines used to arrive as one visible
        // line, because the height was measured against a viewport that did not
        // have a width yet.
        const int lines = 5;
        QVERIFY2(card->height() >= edit->fontMetrics().lineSpacing() * lines,
                 qPrintable(QString("card is only %1px tall").arg(card->height())));
        QVERIFY(!card->isClipped());

        // And nothing is scrolled out of view horizontally or vertically: the
        // card shows the text from its very first character.
        QCOMPARE(edit->horizontalScrollBar()->value(), 0);
        QCOMPARE(edit->verticalScrollBar()->value(), 0);
    }

    void pastingTextDoesNotAlsoOpenABlankCard()
    {
        GuiFixture f;
        QApplication::clipboard()->setText(QStringLiteral("just this"));
        f.trigger("pasteAction");

        const auto cards = f.canvas()->findChildren<TextItemCard*>();
        QCOMPARE(cards.size(), 1);          // the pasted card, and nothing else
        QVERIFY(!cards.first()->isComposer());
        QCOMPARE(f.items.countForBuffer(f.buffers.listLive(10).front().id), 1);
    }

    void doubleClickingABufferDoesNotAddATextBlock()
    {
        GuiFixture f;
        const auto id = f.seed("just looking");
        f.select(id);
        const int before = f.canvas()->findChildren<TextItemCard*>().size();
        QCOMPARE(before, 1);   // the one real item, and no blank block

        const QRect rect = f.view()->visualRect(f.model()->index(f.model()->rowForId(id), 0));
        QTest::mouseDClick(f.view()->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());

        // Opening a buffer is not a request to write in it.
        QCOMPARE(f.canvas()->findChildren<TextItemCard*>().size(), before);
        QCOMPARE(f.items.countForBuffer(id), 1);
    }

    void everyCardHasACopyActionAndAnAge()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        const auto footers = f.canvas()->findChildren<CardFooter*>();
        QCOMPARE(footers.size(), f.items.countForBuffer(id));
    }

    void theCardCopyActionCopiesThatCardNotTheSelection()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        QApplication::clipboard()->clear();

        // Select something else entirely, then use a different card's button.
        auto* image = f.canvas()->findChildren<ImageItemCard*>().first();
        QTest::mouseClick(image, Qt::LeftButton);

        auto* textCard = f.canvas()->findChildren<TextItemCard*>().first();
        emit textCard->copyRequested(textCard->itemId());
        QCOMPARE(QApplication::clipboard()->text(), textCard->text());

        // And the selection is left exactly as it was.
        QCOMPARE(f.canvas()->selection().size(), 1);
        QCOMPARE(f.canvas()->selection().first(), image->itemId());
    }

    void aTinyCardStillHasAMinimumSize()
    {
        GuiFixture f;
        const auto id = f.buffers.create();
        f.service.appendTo(id, Item::makeText(QStringLiteral("ok")));
        f.model()->reload();
        f.select(id);

        auto* card = f.canvas()->findChildren<TextItemCard*>().first();
        QVERIFY2(card->height() >= tokens::kCardMinHeight,
                 qPrintable(QString("a two-letter card is %1px tall").arg(card->height())));
        QVERIFY(card->width() >= tokens::kCardMinWidth);
    }

    void aCardsHeightDependsOnlyOnItsOwnContent()
    {
        GuiFixture f;
        const auto id = f.buffers.create();
        f.service.appendTo(id, Item::makeText(QStringLiteral("short")));
        f.model()->reload();
        f.select(id);
        const int aloneHeight = f.canvas()->findChildren<TextItemCard*>().first()->height();

        // Add a very tall neighbour; the short card must not change size.
        QString huge;
        for (int i = 0; i < 60; ++i) huge += QStringLiteral("line %1\n").arg(i);
        f.service.appendTo(id, Item::makeText(huge));
        f.select(id);
        f.window.selectBuffer(f.model()->rowForId(id));

        for (auto* card : f.canvas()->findChildren<TextItemCard*>())
            if (card->text() == QStringLiteral("short"))
                QCOMPARE(card->height(), aloneHeight);
    }

    // --- the board model -----------------------------------------------------
    void clearingATextCardDeletesTheItem()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);
        const int before = f.items.countForBuffer(id);

        auto* card = f.canvas()->findChildren<TextItemCard*>().first();
        card->beginEditing();
        auto* edit = card->findChild<QPlainTextEdit*>();
        edit->selectAll();
        QTest::keyClick(edit, Qt::Key_Delete);

        // An item holding nothing is not a thing; a blank card is litter.
        QTRY_VERIFY_WITH_TIMEOUT(f.items.countForBuffer(id) == before - 1, 3000);
    }

    void clearingTheOnlyTextCardOfASingleItemBufferRemovesTheBuffer()
    {
        GuiFixture f;
        QApplication::clipboard()->setText(QStringLiteral("only thing"));
        f.trigger("pasteAction");
        const auto id = f.buffers.listLive(10).front().id;

        auto* card = f.canvas()->findChildren<TextItemCard*>().first();
        card->beginEditing();
        auto* edit = card->findChild<QPlainTextEdit*>();
        edit->selectAll();
        QTest::keyClick(edit, Qt::Key_Delete);

        QTRY_VERIFY_WITH_TIMEOUT(f.buffers.countLive() == 0, 3000);
        QVERIFY(f.buffers.find(id)->inTrash());
    }

    void ctrlNGivesAnEmptyBoardRatherThanABlankPage()
    {
        GuiFixture f;
        f.trigger("newBufferAction");

        // Napkin is temporary storage, not an editor: a new buffer waits to be
        // pasted into instead of offering somewhere to write.
        QCOMPARE(f.canvas()->findChildren<TextItemCard*>().size(), 0);
        QCOMPARE(f.buffers.countLive(), 0);
    }

    void theNewestItemIsFirst()
    {
        GuiFixture f;
        const auto id = f.seed("oldest");
        f.select(id);
        QTest::keyClicks(f.newTextCard(), "newest");
        QTRY_VERIFY_WITH_TIMEOUT(f.items.countForBuffer(id) == 2, 2000);

        const auto ordered = f.items.listForBuffer(id);
        QCOMPARE(ordered.front().text, QStringLiteral("newest"));
        // And the board agrees: first on screen is the newest, not the first
        // widget that happened to be constructed.
        QCOMPARE(f.canvas()->itemOrder().first(), ordered.front().id);
    }

    void aCardIsNoWiderThanTheBoardAllows()
    {
        GuiFixture f;
        const auto id = f.seed("short");
        f.select(id);
        f.window.resize(1400, 800);
        QTest::qWait(50);

        for (auto* card : f.canvas()->findChildren<ItemCard*>())
            QVERIFY2(card->width() <= tokens::kCardMaxWidth,
                     qPrintable(QString("card is %1px wide").arg(card->width())));
    }

    void aVeryLongTextCardIsCappedRatherThanOwningTheBoard()
    {
        GuiFixture f;
        const auto id = f.buffers.create();
        QString huge;
        for (int i = 0; i < 400; ++i) huge += QStringLiteral("line %1\n").arg(i);
        f.service.appendTo(id, Item::makeText(huge));
        f.model()->reload();
        f.select(id);

        auto* card = f.canvas()->findChildren<TextItemCard*>().first();
        QVERIFY(card->height() <= tokens::kCardMaxHeight);
        QVERIFY(card->isClipped());   // and it says so, rather than hiding it
    }

    // --- editing -------------------------------------------------------------
    void ctrlTThenTypingAddsANewTextItem()
    {
        GuiFixture f;
        const auto id = seedMixed(f);
        f.select(id);

        QTest::keyClicks(f.newTextCard(), "one more thought");
        QTRY_VERIFY_WITH_TIMEOUT(f.items.countForBuffer(id) == 5, 2000);
        QCOMPARE(f.items.listForBuffer(id).front().text, QStringLiteral("one more thought"));
    }

    void switchingBufferSavesTheOneYouAreLeaving()
    {
        GuiFixture f;
        const auto first = f.seed("first buffer");
        const auto second = f.seed("second buffer");

        f.select(first);
        QTest::keyClicks(f.newTextCard(), "an unsaved addition");
        f.select(second);   // inside the debounce window

        QTRY_VERIFY_WITH_TIMEOUT(f.items.countForBuffer(first) == 2, 2000);
        QCOMPARE(f.items.listForBuffer(first).front().text,
                 QStringLiteral("an unsaved addition"));
    }

    void theListDoesNotResortWhileYouTypeInTheCanvas()
    {
        GuiFixture f;
        const auto older = f.seed("older");
        const auto newer = f.seed("newer");
        QCOMPARE(f.model()->idAt(0), newer);

        f.select(older);
        QTest::keyClicks(f.newTextCard(), "edited");
        QTRY_VERIFY_WITH_TIMEOUT(f.items.countForBuffer(older) == 2, 2000);

        QCOMPARE(f.model()->idAt(0), newer);   // held while typing
        QVERIFY(f.model()->orderFrozen());
    }
};

QTEST_MAIN(TestCanvas)
#include "test_canvas.moc"
