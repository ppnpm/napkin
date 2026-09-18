#include "GuiFixture.h"
#include "../src/media/BlobGc.h"

#include <QBuffer>
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
};

QTEST_MAIN(TestMultiItem)
#include "test_multiitem.moc"
