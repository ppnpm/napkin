#include "TestDb.h"
#include <QtTest>

class TestItems : public QObject {
    Q_OBJECT
private slots:
    void draftWithNoContentWritesNoRow()
    {
        TestDb t;
        napkin::Draft draft;
        QCOMPARE(t.service.commitDraft(draft), napkin::kNoBuffer);

        draft.setText(QStringLiteral("   \n  "));  // whitespace is not content
        QCOMPARE(t.service.commitDraft(draft), napkin::kNoBuffer);
        QCOMPARE(t.buffers.countLive(), 0);  // invariant 5
    }

    void draftWithContentWritesExactlyOneBuffer()
    {
        TestDb t;
        napkin::Draft draft;
        draft.setText(QStringLiteral("systemctl restart nginx"));
        const auto id = t.service.commitDraft(draft);

        QVERIFY(id != napkin::kNoBuffer);
        QCOMPARE(t.buffers.countLive(), 1);
        QCOMPARE(t.items.countForBuffer(id), 1);
    }

    void draftOfOnlyAnImageIsContent()
    {
        TestDb t;
        napkin::Draft draft;
        draft.add(napkin::Item::makeImage(QStringLiteral("abc123"), 800, 600, 4096));
        const auto id = t.service.commitDraft(draft);

        QVERIFY(id != napkin::kNoBuffer);
        QCOMPARE(t.items.listForBuffer(id)[0].type, napkin::ItemType::Image);
    }

    void positionsAreAssignedInInsertionOrderEvenThoughTheBoardShowsNewestFirst()
    {
        TestDb t;
        const auto id = t.buffers.create();
        t.service.appendTo(id, napkin::Item::makeText(QStringLiteral("Investigate this bug")));
        t.service.appendTo(id, napkin::Item::makeImage(QStringLiteral("hash1"), 100, 50, 900));
        t.service.appendTo(id, napkin::Item::makeText(QStringLiteral("https://example.com")));

        // listForBuffer returns the board order: newest first. position still
        // records the order things arrived, which is what undo restores to.
        const auto list = t.items.listForBuffer(id);
        QCOMPARE(list.size(), size_t(3));
        QCOMPARE(list[0].position, 2);
        QCOMPARE(list[1].position, 1);
        QCOMPARE(list[2].position, 0);
        QCOMPARE(list[1].type, napkin::ItemType::Image);
        QCOMPARE(list[0].text, QStringLiteral("https://example.com"));
    }

    void imageMetadataRoundTrips()
    {
        TestDb t;
        const auto id = t.buffers.create();
        t.service.appendTo(id, napkin::Item::makeImage(
            QStringLiteral("deadbeef"), 1920, 1080, 250000, QStringLiteral("diagram.png")));

        const auto it = t.items.listForBuffer(id)[0];
        QCOMPARE(it.blobHash, QStringLiteral("deadbeef"));
        QCOMPARE(it.width, 1920);
        QCOMPARE(it.height, 1080);
        QCOMPARE(it.byteSize, 250000LL);
        QCOMPARE(it.sourceName, QStringLiteral("diagram.png"));
        QVERIFY(it.text.isEmpty());
    }

    void editingAnItemBumpsTheBuffer()
    {
        TestDb t;
        const auto id = t.buffers.create();
        const auto itemId = t.service.appendTo(id, napkin::Item::makeText(QStringLiteral("a")));
        const auto before = t.buffers.find(id)->modifiedAt;

        t.advanceDays(1);
        t.service.updateTextItem(id, itemId, QStringLiteral("a modified"));

        QVERIFY(t.buffers.find(id)->modifiedAt > before);
        QCOMPARE(t.items.find(itemId)->text, QStringLiteral("a modified"));
    }

    void blobIsReferencedUntilTheLastItemGoes()
    {
        TestDb t;
        const auto a = t.buffers.create();
        const auto b = t.buffers.create();
        const QString hash = QStringLiteral("sharedhash");

        // The same screenshot pasted twice dedupes to one blob (SPEC.md §5).
        const auto i1 = t.service.appendTo(a, napkin::Item::makeImage(hash, 10, 10, 100));
        const auto i2 = t.service.appendTo(b, napkin::Item::makeImage(hash, 10, 10, 100));
        QVERIFY(t.items.blobIsReferenced(hash));

        t.service.removeItem(a, i1);
        QVERIFY(t.items.blobIsReferenced(hash));  // still held by the other buffer

        t.service.removeItem(b, i2);
        QVERIFY(!t.items.blobIsReferenced(hash));  // now safe to unlink
    }

    void deletingABufferReleasesItsBlobs()
    {
        TestDb t;
        const auto id = t.buffers.create();
        t.service.appendTo(id, napkin::Item::makeImage(QStringLiteral("h"), 1, 1, 1));

        t.service.trash(id);
        QVERIFY(t.items.blobIsReferenced(QStringLiteral("h")));  // trash is not deletion

        t.advanceDays(napkin::kTrashRetentionDays + 1);
        t.service.purgeExpiredTrash();
        QVERIFY(!t.items.blobIsReferenced(QStringLiteral("h")));
    }
};

QTEST_APPLESS_MAIN(TestItems)
#include "test_items.moc"
