#include "../src/app/Paths.h"
#include "../src/data/Database.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtTest>

// Regression guard. The first implementation chmod'd the database only if it
// already existed, so on a first run — when SQLite had not created it yet —
// napkin.db was left at the process umask (0644), along with the -wal and -shm
// sidecars that hold uncommitted user content.
//
// On Windows QFile::setPermissions only toggles the read-only attribute, and
// QFile::permissions reports every bit set unless it is told to read the ACL.
// Privacy there comes from the ACL the directory inherits from the user's
// profile, so that is what these checks read: without the guard they would test
// nothing, and a red result would say nothing about the files.
#ifdef Q_OS_WIN
#  define NAPKIN_READ_ACLS QNtfsPermissionCheckGuard aclGuard
#else
#  define NAPKIN_READ_ACLS do {} while (0)
#endif

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
        NAPKIN_READ_ACLS;
        const auto p = QFile::permissions(napkin::paths::dataDir());
        const QByteArray bits = QByteArray::number(int(p), 16);
        QVERIFY2(p.testFlag(QFile::ReadOwner), bits);
        QVERIFY2(!p.testFlag(QFile::ReadGroup), bits);
        QVERIFY2(!p.testFlag(QFile::ReadOther), bits);
    }

    void databaseAndWalAreOwnerOnlyOnAFirstRun()
    {
        napkin::paths::ensureDirs();
        QVERIFY(!QFile::exists(napkin::paths::databaseFile()));  // genuinely a first run

        napkin::Database db;
        db.open(napkin::paths::databaseFile());
        napkin::paths::secureDatabaseFiles();

        NAPKIN_READ_ACLS;
        for (const QString& suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")}) {
            const QString path = napkin::paths::databaseFile() + suffix;
            if (!QFile::exists(path)) continue;
            const auto p = QFile::permissions(path);
            QVERIFY2(!p.testFlag(QFile::ReadGroup), qPrintable("group-readable: " + path + " " + QString::number(int(p), 16)));
            QVERIFY2(!p.testFlag(QFile::ReadOther), qPrintable("world-readable: " + path + " " + QString::number(int(p), 16)));
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
