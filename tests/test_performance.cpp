#include "../src/data/BufferRepository.h"
#include "../src/data/Database.h"
#include "../src/data/ItemRepository.h"
#include "../src/domain/Preview.h"
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QtTest>

using namespace napkin;

// SPEC.md §12 promises 5000 buffers with smooth scrolling and a list that does
// not read every item to draw itself. These run against a real file-backed
// database, not :memory:, so the numbers mean something.
class TestPerformance : public QObject {
    Q_OBJECT
private:
    QTemporaryDir dir_;
    Database db_;
    static constexpr int kBuffers = 5000;

private slots:
    void initTestCase()
    {
        db_.open(dir_.path() + "/perf.db");
        BufferRepository buffers(db_);
        ItemRepository items(db_);

        QElapsedTimer t; t.start();
        Transaction tx(db_);
        for (int i = 0; i < kBuffers; ++i) {
            const auto id = buffers.create(i % 50 == 0 /*pinned*/, false);
            items.append(id, Item::makeText(
                QStringLiteral("buffer %1\nsystemctl restart nginx --now\nsecond line").arg(i)));
            if (i % 3 == 0)
                items.append(id, Item::makeImage(QStringLiteral("hash%1").arg(i), 1920, 1080, 250000));
        }
        tx.commit();
        qInfo() << "seeded" << kBuffers << "buffers in" << t.elapsed() << "ms";
    }

    void listLiveIsFastEnoughForStartup()
    {
        BufferRepository buffers(db_);
        QElapsedTimer t; t.start();
        const auto rows = buffers.listLive(kBuffers);
        const qint64 ms = t.elapsed();

        QCOMPARE(int(rows.size()), kBuffers);
        qInfo() << "listLive(5000) metadata:" << ms << "ms";
        // Startup budget is 300 ms total; the list query gets a fraction of it.
        QVERIFY2(ms < 100, qPrintable(QString("listLive took %1 ms").arg(ms)));
    }

    void pinnedSortStillHoldsAtScale()
    {
        BufferRepository buffers(db_);
        const auto rows = buffers.listLive(kBuffers);
        bool seenUnpinned = false;
        for (const auto& b : rows) {
            if (!b.pinned) seenUnpinned = true;
            else QVERIFY2(!seenUnpinned, "a pinned buffer appeared below an unpinned one");
        }
    }

    void drawingAScreenfulNeverReadsEveryItem()
    {
        BufferRepository buffers(db_);
        ItemRepository items(db_);
        const auto rows = buffers.listLive(kBuffers);

        // A screenful is ~12 cards. This is what a scroll tick costs.
        constexpr int kScreenful = 12;
        QElapsedTimer t; t.start();
        for (int i = 0; i < kScreenful; ++i) {
            const auto id = rows[size_t(i)].id;
            const auto p = derivePreview(items.previewHead(id), items.countForBuffer(id));
            QVERIFY(!p.primary.isEmpty());
        }
        const qint64 ms = t.elapsed();
        qInfo() << "previews for a screenful of" << kScreenful << "cards:" << ms << "ms";
        QVERIFY2(ms < 16, qPrintable(QString("a screenful took %1 ms, which would drop frames").arg(ms)));
    }

    void windowedQueryDoesNotDegradeDeepIntoTheList()
    {
        BufferRepository buffers(db_);
        QElapsedTimer t; t.start();
        const auto deep = buffers.listLive(20, kBuffers - 20);
        const qint64 ms = t.elapsed();
        QCOMPARE(int(deep.size()), 20);
        qInfo() << "windowed query at offset" << kBuffers - 20 << ":" << ms << "ms";
        QVERIFY(ms < 50);
    }
};

QTEST_APPLESS_MAIN(TestPerformance)
#include "test_performance.moc"
