#include "GuiFixture.h"
#include "../src/media/BlobGc.h"
#include "../src/media/ImageFormats.h"
#include "../src/ui/ItemCanvas.h"
#include "../src/ui/ItemCard.h"

#include <QBuffer>
#include <QDir>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QtTest>

using namespace napkin;

namespace {
QByteArray png(int w, int h)
{
    QImage image(w, h, QImage::Format_RGB32);
    image.fill(Qt::magenta);
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return out;
}
}  // namespace

// Regressions for findings from the independent security/performance and
// UI/UX reviews. Each of these reproduced a real defect before its fix.
class TestReviewFixes : public QObject {
    Q_OBJECT
private slots:
    // --- SVG could read local files -----------------------------------------
    void anSvgThatNamesALocalFileIsRefused()
    {
        // Qt refuses file:// but happily loads a BARE path, which an earlier
        // "measured, not assumed" claim missed because the probe only ever
        // tested the spelling Qt already rejected.
        QVERIFY(formats::svgHasExternalReferences(
            "<svg xmlns='http://www.w3.org/2000/svg'><image href='/etc/hostname'/></svg>"));
        QVERIFY(formats::svgHasExternalReferences(
            "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'>"
            "<image xlink:href='/tmp/secret.png'/></svg>"));
        QVERIFY(formats::svgHasExternalReferences(
            "<svg xmlns='http://www.w3.org/2000/svg'><rect fill='url(/tmp/x.png)'/></svg>"));
    }

    void aSelfContainedSvgIsStillAccepted()
    {
        QVERIFY(!formats::svgHasExternalReferences(
            "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'>"
            "<defs><linearGradient id='g'/></defs><rect fill='url(#g)' width='10' height='10'/>"
            "</svg>"));

        QTemporaryDir dir;
        BlobStore store(dir.path());
        const auto ok = store.store(
            "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'>"
            "<rect width='10' height='10' fill='red'/></svg>");
        QVERIFY2(ok.ok, qPrintable(ok.error));

        const auto refused = store.store(
            "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'>"
            "<image href='/tmp/secret.png' width='10' height='10'/></svg>");
        QVERIFY(!refused.ok);
        QVERIFY(refused.error.contains(QStringLiteral("other files")));
    }

    // --- a small file can still be an enormous image -------------------------
    void aPixelBombIsRefusedEvenThoughItIsTiny()
    {
        QTemporaryDir dir;
        BlobStore store(dir.path());
        QImage huge(20000, 20000, QImage::Format_Mono);   // declares 400 megapixels
        huge.fill(0);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!huge.save(&buffer, "PNG")) QSKIP("could not build the test image");

        QVERIFY(bytes.size() < BlobStore::kMaxBytes);   // passes the byte check
        const auto stored = store.store(bytes);
        QVERIFY(!stored.ok);                            // and is still refused
    }

    // --- INSERT OR REPLACE walked straight past the guard --------------------
    void replaceCannotSmuggleAKeptBufferOut()
    {
        GuiFixture f;
        const auto id = f.buffers.create();
        f.buffers.setKept(id, true);

        QVERIFY_THROWS_EXCEPTION(DbError,
            f.db.exec(QString("INSERT OR REPLACE INTO buffers(id, created_at, modified_at, kept)"
                              " VALUES(%1, 1, 1, 0)").arg(id).toUtf8().constData()));
        QVERIFY(f.buffers.find(id)->kept);
    }

    // --- undo used to hand back a stripped buffer ----------------------------
    void undoingAConfirmedDeleteRestoresTheKeep()
    {
        GuiFixture f;
        const auto id = f.seed("important reference");
        f.window.toggleKeep(f.model()->rowForId(id));
        const auto modifiedBefore = f.buffers.find(id)->modifiedAt;

        // Confirming release the keep, by design. Undo has to put it back.
        f.service.trashConfirmed(id);
        f.model()->reload();
        QVERIFY(!f.buffers.find(id)->kept);

        f.window.undoLastTrashForTest(id, /*wasKept=*/true, modifiedBefore);

        QVERIFY(f.buffers.find(id)->kept);
        QVERIFY(!f.buffers.find(id)->inTrash());
        QCOMPARE(f.buffers.find(id)->modifiedAt, modifiedBefore);   // history intact
    }

    // --- thumbnails outlived the images they came from -----------------------
    void emptyingTheTrashAlsoRemovesTheThumbnail()
    {
        GuiFixture f;
        const auto stored = f.blobs.store(png(300, 200));
        QVERIFY(stored.ok);
        const auto id = f.buffers.create();
        f.service.appendTo(id, Item::makeImage(stored.hash, 300, 200, stored.byteSize,
                                               {}, stored.mime));
        QVERIFY(!f.thumbs.forBlob(stored.hash, stored.mime).isNull());   // writes the cache file

        const QString thumbDir = f.thumbsDir();
        QVERIFY(!QDir(thumbDir).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty()
                || !QDir(thumbDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());

        f.service.trash(id);
        f.service.emptyTrash();
        const auto result = reconcileBlobs(f.items, f.blobs, thumbDir);

        QVERIFY(!f.blobs.exists(stored.hash, stored.mime));
        QCOMPARE(result.thumbnailsRemoved, 1);   // the rendering went too
    }

    // --- the empty-trash dialog promised more than it delivered --------------
    void theEmptyTrashCountMatchesWhatWillActuallyBeDestroyed()
    {
        GuiFixture f;
        const auto ordinary = f.buffers.create();
        const auto stuck = f.buffers.create();
        f.buffers.setKept(stuck, true);
        f.db.exec("UPDATE buffers SET deleted_at = 1");   // both in the bin

        QCOMPARE(f.buffers.countTrash(), 2);        // what is in there
        QCOMPARE(f.buffers.countPurgeable(), 1);    // what emptying it would take
        QCOMPARE(f.service.emptyTrash(), 1);
        QVERIFY(f.buffers.find(stuck).has_value());
        QVERIFY(!f.buffers.find(ordinary).has_value());
    }

    // --- Ctrl+N inside the trash created a live buffer shown in the bin ------
    void newBufferLeavesTheTrashViewFirst()
    {
        GuiFixture f;
        f.seed("something");
        f.window.showTrash(true);
        QCOMPARE(f.model()->mode(), BufferListModel::Mode::Trash);

        f.trigger("newBufferAction");

        QCOMPARE(f.model()->mode(), BufferListModel::Mode::Live);
    }

    // --- a huge single line made every repaint O(text length) ---------------
    void aPreviewLineIsBoundedNoMatterHowLargeTheText()
    {
        const QString huge(400000, QLatin1Char('x'));
        const auto p = derivePreview({Item::makeText(huge)}, 1, 0);
        QVERIFY(p.primary.size() <= kPreviewLineLimit);
    }

    void metadataRolesDoNotTouchTheDatabase()
    {
        // sizeHint() reads IsExpandedRole for every row, and QListView asks
        // every row. Computing a preview there turned startup into thousands of
        // queries and materialised every buffer's text.
        GuiFixture f;
        for (int i = 0; i < 50; ++i) f.seed("buffer");

        f.model()->reload();          // drops the preview cache
        const int before = f.statementCount();
        for (int row = 0; row < f.model()->rowCount(); ++row) {
            f.model()->index(row, 0).data(BufferListModel::IsDraftRole);
            f.model()->index(row, 0).data(BufferListModel::PinnedRole);
            f.model()->index(row, 0).data(BufferListModel::SectionFirstRole);
        }
        QCOMPARE(f.statementCount(), before);   // not one query
    }

// --- third audit ------------------------------------------------------------
    void editingAnOlderCardThenAppendingMustNotRebindTheOthers()
    {
        GuiFixture f;
        const auto id = f.buffers.create();
        for (const char* t : {"AAA", "BBB", "CCC"})
            f.service.appendTo(id, Item::makeText(QString::fromUtf8(t)));
        f.model()->reload();
        f.select(id);

        // Edit the OLDEST card. That bumps its modified_at, so the database
        // order changes while the widget order deliberately does not.
        TextItemCard* oldest = nullptr;
        for (auto* c : f.canvas()->findChildren<TextItemCard*>())
            if (c->text() == QStringLiteral("AAA")) oldest = c;
        QVERIFY(oldest);
        oldest->beginEditing();
        QTest::keyClicks(oldest->findChild<QPlainTextEdit*>(), "-edited");
        QTest::qWait(600);

        // Now append a new card, which is what triggers the rebind.
        QTest::keyClicks(f.newTextCard(), "NEW");
        QTest::qWait(600);

        // Every widget must still be bound to the row whose text it shows.
        for (auto* c : f.canvas()->findChildren<TextItemCard*>()) {
            if (c->isComposer()) continue;
            const auto row = f.items.find(c->itemId());
            QVERIFY(row.has_value());
            QCOMPARE(c->text(), row->text);
        }
    }

    void theOverflowMenuActuallyListsTheActions()
    {
        GuiFixture f;
        auto* button = f.window.findChild<QToolButton*>(QStringLiteral("overflowButton"));
        QVERIFY(button);
        QVERIFY(button->menu());
        QStringList labels;
        for (auto* a : button->menu()->actions())
            if (!a->isSeparator()) labels << a->text();
        // Ctrl+T is the only way to make a text card; it must be findable.
        QVERIFY2(labels.filter(QStringLiteral("text")).size() > 0,
                 qPrintable("menu had: " + labels.join(", ")));
        QVERIFY(labels.size() >= 4);
    }

    void aFailingReadDuringARowClickDoesNotTerminate()
    {
        GuiFixture f;
        f.seed("one"); f.seed("two");
        f.db.exec("DROP TABLE items");

        bool threw = false;
        try { f.window.selectBuffer(0); } catch (...) { threw = true; }
        QVERIFY2(threw, "selectBuffer still throws — the boundary must be at notify()");
    }

    void theSweepDoesNotDestroyABlobUndoStillNeeds()
    {
        GuiFixture f;
        QImage img(40, 30, QImage::Format_RGB32); img.fill(Qt::red);
        QByteArray png; QBuffer buf(&png); buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");

        const auto stored = f.blobs.store(png);
        const auto keep = f.buffers.create();
        f.service.appendTo(keep, Item::makeText(QStringLiteral("still here")));
        const auto doomed = f.buffers.create();
        f.service.appendTo(doomed, Item::makeImage(stored.hash, 40, 30, stored.byteSize,
                                                   {}, stored.mime));
        f.model()->reload();

        f.select(doomed);
        f.canvas()->selectAll();
        f.canvas()->deleteSelection();
        QVERIFY(f.toast()->hasOffer());

        // Any sweep inside the eight-second window used to destroy the blob the
        // offer depends on. Run one directly.
        reconcileBlobs(f.items, f.blobs, f.thumbsDir(), f.window.undoProtectedBlobsForTest());

        if (f.toast()->hasOffer()) {
            f.toast()->findChild<QPushButton*>()->click();
            for (const auto& item : f.items.allImageItems())
                QVERIFY2(f.blobs.exists(item.blobHash, item.mime),
                         "undo restored a row whose blob had been swept away");
        }
    }

    void thePreviewCacheIsBounded()
    {
        GuiFixture f;
        for (int i = 0; i < 60; ++i) f.seed("buffer");
        f.model()->reload();
        for (int row = 0; row < f.model()->rowCount(); ++row)
            f.model()->index(row, 0).data(BufferListModel::PrimaryRole);
        QVERIFY(f.model()->previewCacheSize() <= 400);
    }
};

QTEST_MAIN(TestReviewFixes)
#include "test_review_fixes.moc"
