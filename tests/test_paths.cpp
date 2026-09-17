#include "../src/app/Paths.h"
#include "../src/data/Database.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtTest>

// Regression guard. The first implementation chmod'd the database only if it
// already existed, so on a first run — when SQLite had not created it yet —
// napkin.db was left at the process umask (0644), along with the -wal and -shm
// sidecars that hold uncommitted user content.
class TestPaths : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationName(QStringLiteral("napkin"));
        QStandardPaths::setTestModeEnabled(true);
        QDir(napkin::paths::dataDir()).removeRecursively();
    }

    void cleanupTestCase()
    {
        QDir(napkin::paths::dataDir()).removeRecursively();
        QStandardPaths::setTestModeEnabled(false);
    }

    void dataDirectoryIsOwnerOnly()
    {
        napkin::paths::ensureDirs();
        const auto p = QFile::permissions(napkin::paths::dataDir());
        QVERIFY(p.testFlag(QFile::ReadOwner));
        QVERIFY(!p.testFlag(QFile::ReadGroup));
        QVERIFY(!p.testFlag(QFile::ReadOther));
    }

    void databaseAndWalAreOwnerOnlyOnAFirstRun()
    {
        napkin::paths::ensureDirs();
        QVERIFY(!QFile::exists(napkin::paths::databaseFile()));  // genuinely a first run

        napkin::Database db;
        db.open(napkin::paths::databaseFile());
        napkin::paths::secureDatabaseFiles();

        for (const QString& suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")}) {
            const QString path = napkin::paths::databaseFile() + suffix;
            if (!QFile::exists(path)) continue;
            const auto p = QFile::permissions(path);
            QVERIFY2(!p.testFlag(QFile::ReadGroup), qPrintable("group-readable: " + path));
            QVERIFY2(!p.testFlag(QFile::ReadOther), qPrintable("world-readable: " + path));
        }
    }

    void dataDirectoryIsMarkedUnindexable()
    {
        napkin::paths::ensureDirs();
        QVERIFY(QFile::exists(napkin::paths::dataDir() + "/.noindex"));
        QVERIFY(QFile::exists(napkin::paths::dataDir() + "/.nomedia"));
    }
};

QTEST_APPLESS_MAIN(TestPaths)
#include "test_paths.moc"
