#include "TestDb.h"
#include "../src/domain/Search.h"
#include <QtTest>

using namespace napkin;

class TestSearch : public QObject {
    Q_OBJECT
private:
    BufferId seed(TestDb& t, const char* text, const char* filename = nullptr)
    {
        const auto id = t.buffers.create();
        if (text) t.service.appendTo(id, Item::makeText(QString::fromUtf8(text)));
        if (filename)
            t.service.appendTo(id, Item::makeImage(QStringLiteral("h%1").arg(id), 10, 10, 1,
                                                   QString::fromUtf8(filename)));
        return id;
    }

private slots:
    // --- the query language is not free text ---------------------------------
    void whatTheUserTypedBecomesAValidMatchExpression()
    {
        QCOMPARE(toMatchExpression(QStringLiteral("nginx")), QStringLiteral("\"nginx\"*"));
        QCOMPARE(toMatchExpression(QStringLiteral("restart nginx")),
                 QStringLiteral("\"restart\" \"nginx\"*"));
        QCOMPARE(toMatchExpression(QStringLiteral("  spaced   out  ")),
                 QStringLiteral("\"spaced\" \"out\"*"));
        QVERIFY(toMatchExpression(QStringLiteral("   ")).isEmpty());
    }

    void fts5OperatorsTypedAsWordsAreTreatedAsWords()
    {
        // Unquoted, FTS5 reads these as operators and either changes the
        // meaning or raises a syntax error.
        TestDb t;
        seed(t, "this AND that OR the other NOT nearby");
        seed(t, "something else entirely");

        for (const char* term : {"AND", "OR", "NOT", "NEAR"}) {
            const auto hits = searchBuffers(t.db, QString::fromUtf8(term), 10);
            QCOMPARE(int(hits.size()), 1);
        }
    }

    void aQuoteDoesNotBreakTheSearch()
    {
        TestDb t;
        seed(t, "it's a \"quoted\" phrase");
        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("quoted"), 10).size()), 1);
        // The point is that this does not throw.
        searchBuffers(t.db, QStringLiteral("\""), 10);
        searchBuffers(t.db, QStringLiteral("it's"), 10);
    }

    // --- finding things ------------------------------------------------------
    void findsTextAnywhereInABuffer()
    {
        TestDb t;
        const auto wanted = seed(t, "systemctl restart nginx");
        seed(t, "sudo pacman -Syu");

        const auto hits = searchBuffers(t.db, QStringLiteral("nginx"), 10);
        QCOMPARE(int(hits.size()), 1);
        QCOMPARE(hits.front().bufferId, wanted);
    }

    void findsAFilenameJustLikeAWord()
    {
        TestDb t;
        const auto wanted = seed(t, "some notes", "wayland-clipboard.png");
        seed(t, "unrelated");

        // A hit on item 3's filename must surface the whole buffer: the list
        // shows buffers, not items (SPEC.md §5).
        const auto hits = searchBuffers(t.db, QStringLiteral("wayland"), 10);
        QCOMPARE(int(hits.size()), 1);
        QCOMPARE(hits.front().bufferId, wanted);
    }

    void narrowsWhileYouAreStillTypingTheWord()
    {
        TestDb t;
        seed(t, "screenshot of the dashboard");
        for (const char* partial : {"scr", "screen", "screenshot"})
            QCOMPARE(int(searchBuffers(t.db, QString::fromUtf8(partial), 10).size()), 1);
    }

    void severalWordsMustAllMatch()
    {
        TestDb t;
        seed(t, "restart nginx now");
        seed(t, "restart apache now");

        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("restart"), 10).size()), 2);
        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("restart nginx"), 10).size()), 1);
    }

    void aSnippetShowsWhyItMatched()
    {
        TestDb t;
        seed(t, "the quick brown fox jumps over the lazy dog");
        const auto hits = searchBuffers(t.db, QStringLiteral("brown"), 10);
        QCOMPARE(int(hits.size()), 1);
        QVERIFY(hits.front().snippet.contains(QStringLiteral("brown")));
    }

    void countsHowManyItemsMatched()
    {
        TestDb t;
        const auto id = t.buffers.create();
        t.service.appendTo(id, Item::makeText(QStringLiteral("nginx one")));
        t.service.appendTo(id, Item::makeText(QStringLiteral("nginx two")));
        t.service.appendTo(id, Item::makeText(QStringLiteral("something else")));

        const auto hits = searchBuffers(t.db, QStringLiteral("nginx"), 10);
        QCOMPARE(int(hits.size()), 1);
        QCOMPARE(hits.front().matchingItems, 2);
    }

    void locatesTheMatchingItemsWithinABuffer()
    {
        TestDb t;
        const auto id = t.buffers.create();
        const auto a = t.service.appendTo(id, Item::makeText(QStringLiteral("nginx here")));
        t.service.appendTo(id, Item::makeText(QStringLiteral("nothing here")));

        const auto items = matchingItems(t.db, id, QStringLiteral("nginx"));
        QCOMPARE(int(items.size()), 1);
        QCOMPARE(items.front(), a);
    }

    // --- the index has to stay true ------------------------------------------
    void editingAnItemUpdatesTheIndex()
    {
        TestDb t;
        const auto id = t.buffers.create();
        const auto item = t.service.appendTo(id, Item::makeText(QStringLiteral("before")));

        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("before"), 10).size()), 1);
        t.service.updateTextItem(id, item, QStringLiteral("after"));
        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("before"), 10).size()), 0);
        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("after"), 10).size()), 1);
    }

    void removingAnItemRemovesItFromTheIndex()
    {
        TestDb t;
        const auto id = t.buffers.create();
        const auto item = t.service.appendTo(id, Item::makeText(QStringLiteral("ephemeral")));
        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("ephemeral"), 10).size()), 1);

        t.service.removeItem(id, item);
        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("ephemeral"), 10).size()), 0);
    }

    void trashedBuffersDoNotAppearInResults()
    {
        TestDb t;
        const auto id = seed(t, "findable text");
        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("findable"), 10).size()), 1);

        t.service.trash(id);
        // Search is for finding what you have, not what you threw away.
        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("findable"), 10).size()), 0);
    }

    void searchSeesPinnedAndKeptBuffersToo()
    {
        TestDb t;
        const auto pinned = seed(t, "pinned content");
        const auto kept = seed(t, "kept content");
        t.service.setPinned(pinned, true);
        t.service.setKept(kept, true);

        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("content"), 10).size()), 2);
    }

    void caseAndAccentsDoNotMatter()
    {
        TestDb t;
        seed(t, "Café RÉSUMÉ Notes");
        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("cafe"), 10).size()), 1);
        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("resume"), 10).size()), 1);
        QCOMPARE(int(searchBuffers(t.db, QStringLiteral("NOTES"), 10).size()), 1);
    }

    void searchIsFastEnoughToRunOnEveryKeystroke()
    {
        TestDb t;
        Transaction tx(t.db);
        for (int i = 0; i < 2000; ++i) {
            const auto id = t.buffers.create();
            t.items.append(id, Item::makeText(
                QStringLiteral("buffer %1 systemctl restart nginx journalctl notes").arg(i)));
        }
        tx.commit();

        QElapsedTimer timer;
        timer.start();
        const auto hits = searchBuffers(t.db, QStringLiteral("journalctl"), 50);
        const qint64 ms = timer.elapsed();
        qInfo() << "search across 2000 buffers:" << ms << "ms," << hits.size() << "hits";
        QVERIFY2(ms < 100, qPrintable(QString("search took %1 ms").arg(ms)));
    }
};

QTEST_APPLESS_MAIN(TestSearch)
#include "test_search.moc"
