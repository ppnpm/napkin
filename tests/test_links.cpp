#include "../src/domain/Links.h"
#include <QtTest>

using namespace napkin;
using namespace napkin::links;

// SPEC.md §3: a URL is a text item that renders as a link chip. Every rule about
// what counts as a link lives in one place, so this is where the rules are
// pinned — including the ones that decide what Napkin will hand to the desktop.
class TestLinks : public QObject {
    Q_OBJECT
private slots:
    void aLoneUrlIsAChip()
    {
        QCOMPARE(soleUrl(QStringLiteral("https://example.org/a")).value(),
                 QStringLiteral("https://example.org/a"));
        QCOMPARE(soleUrl(QStringLiteral("  http://example.org  ")).value(),
                 QStringLiteral("http://example.org"));
        // A bare www. host is unambiguous enough to promote, and https is the
        // safe guess in 2026.
        QCOMPARE(soleUrl(QStringLiteral("www.example.org/x")).value(),
                 QStringLiteral("https://www.example.org/x"));
    }

    void proseIsNotAChip()
    {
        QVERIFY(!soleUrl(QStringLiteral("see https://example.org for more")).has_value());
        QVERIFY(!soleUrl(QStringLiteral("https://example.org\nhttps://other.org")).has_value());
        QVERIFY(!soleUrl(QStringLiteral("just some text")).has_value());
        QVERIFY(!soleUrl(QString()).has_value());
    }

    void onlyHttpAndHttpsAreOpenable()
    {
        // The whole point of the restriction: a scratch surface holds whatever
        // was on the clipboard, and Open hands it to the desktop. These are not
        // hidden behind a confirmation — they are simply not links.
        for (const char* hostile : {"file:///etc/passwd",
                                    "javascript:alert(1)",
                                    "data:text/html,<script>alert(1)</script>",
                                    "vbscript:msgbox(1)",
                                    "smb://server/share",
                                    "ftp://example.org/x"}) {
            const QString s = QString::fromLatin1(hostile);
            QVERIFY2(!isOpenable(s), hostile);
            QVERIFY2(!soleUrl(s).has_value(), hostile);
            QVERIFY2(findUrls(s).empty(), hostile);
        }
    }

    void schemeWithoutAHostIsNotALink()
    {
        QVERIFY(!isOpenable(QStringLiteral("http://")));
        QVERIFY(!isOpenable(QStringLiteral("https:///path")));
    }

    void controlCharactersAreRefused()
    {
        // How a link is made to display as one thing and resolve as another.
        QString sneaky = QStringLiteral("https://example.org/");
        sneaky.append(QChar(0x0001)).append(QStringLiteral("evil"));
        QVERIFY(!isOpenable(sneaky));
    }

    void theChipNamesTheRealHost()
    {
        // Reads as bank.example to anyone skimming; resolves to evil.example.
        // The chip shows what QUrl resolves, so it cannot repeat the lie.
        const QString deceptive = QStringLiteral("https://bank.example@evil.example/login");
        QCOMPARE(hostOf(deceptive), QStringLiteral("evil.example"));
        QVERIFY(displayForm(deceptive).startsWith(QStringLiteral("evil.example")));
    }

    void urlsInsideProseAreFound()
    {
        const QString text = QStringLiteral(
            "deploy notes: https://ci.example.org/job/42 then see www.example.org/docs.");
        const auto found = findUrls(text);
        QCOMPARE(int(found.size()), 2);
        QCOMPARE(found[0].url, QStringLiteral("https://ci.example.org/job/42"));
        QCOMPARE(text.mid(found[0].start, found[0].length),
                 QStringLiteral("https://ci.example.org/job/42"));
        // The full stop ends the sentence, not the URL.
        QCOMPARE(found[1].url, QStringLiteral("https://www.example.org/docs"));
    }

    void trailingPunctuationIsLeftToTheSentence()
    {
        QCOMPARE(findUrls(QStringLiteral("(see https://example.org/a)")).at(0).url,
                 QStringLiteral("https://example.org/a"));
        QCOMPARE(findUrls(QStringLiteral("https://example.org/a, and")).at(0).url,
                 QStringLiteral("https://example.org/a"));
        // …but a balanced bracket belongs to the path.
        QCOMPARE(findUrls(QStringLiteral("https://e.org/Foo_(bar) x")).at(0).url,
                 QStringLiteral("https://e.org/Foo_(bar)"));
    }

    void findingIsBoundedSoAPastedLogCannotStallTheEditor()
    {
        QString log;
        for (int i = 0; i < 1000; ++i)
            log += QStringLiteral("line %1 https://example.org/%1\n").arg(i);
        QElapsedTimer t; t.start();
        const auto found = findUrls(log, 200);
        QCOMPARE(int(found.size()), 200);
        QVERIFY2(t.elapsed() < 50, qPrintable(QString::number(t.elapsed())));
    }

    void displayFormKeepsTheHostLegible()
    {
        const QString longUrl = QStringLiteral("https://example.org/") + QString(200, u'a');
        const QString shown = displayForm(longUrl, 72);
        QVERIFY(shown.size() <= 72);
        QVERIFY(shown.startsWith(QStringLiteral("example.org/")));
        // The host is shown exactly as QUrl resolves it: a chip must not tidy
        // away part of the name of the place it is about to send you.
        QCOMPARE(displayForm(QStringLiteral("https://www.example.org/")),
                 QStringLiteral("www.example.org"));
    }
};

QTEST_MAIN(TestLinks)
#include "test_links.moc"
