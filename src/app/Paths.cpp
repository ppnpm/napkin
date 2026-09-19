#include "Paths.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <stdexcept>

namespace napkin::paths {
namespace {

constexpr auto kOwnerOnlyDir  = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
constexpr auto kOwnerOnlyFile = QFile::ReadOwner | QFile::WriteOwner;

void mkdirOrThrow(const QString& path)
{
    if (!QDir().mkpath(path))
        throw std::runtime_error(QString("cannot create directory: %1").arg(path).toStdString());
    QFile::setPermissions(path, kOwnerOnlyDir);
}

}  // namespace

// AppLocalDataLocation, not AppDataLocation. They are the same directory on
// Linux and macOS; on Windows AppDataLocation is the *roaming* profile, which
// can be synced to a server at logon and logoff — no place for a live SQLite
// database and its WAL. This was decided before any Windows build shipped, so
// nobody's data is at the other path.
QString dataDir()      { return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation); }
QString configDir()    { return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation); }
QString databaseFile() { return dataDir() + QStringLiteral("/napkin.db"); }
QString blobsDir()     { return dataDir() + QStringLiteral("/blobs"); }
QString thumbsDir()    { return dataDir() + QStringLiteral("/thumbs"); }

void ensureDirs()
{
    mkdirOrThrow(dataDir());
    mkdirOrThrow(configDir());
    mkdirOrThrow(blobsDir());
    mkdirOrThrow(thumbsDir());

    const QFileInfo info(dataDir());
    if (!info.isWritable())
        throw std::runtime_error(
            QString("Napkin's data folder is not writable:\n%1").arg(dataDir()).toStdString());

    markNotIndexable();
    secureDatabaseFiles();  // no-op on a first run; see the header
}

void markNotIndexable()
{
    // Baloo (KDE) honours a .noindex directory marker; .nomedia is the
    // widely-respected equivalent for media scanners.
    for (const char* name : {"/.noindex", "/.nomedia"}) {
        const QString marker = dataDir() + QLatin1String(name);
        if (QFile::exists(marker)) continue;
        QFile f(marker);
        if (f.open(QIODevice::WriteOnly)) f.close();
    }
}

void secureDatabaseFiles()
{
    for (const auto& suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")}) {
        const QString path = databaseFile() + suffix;
        if (QFile::exists(path)) QFile::setPermissions(path, kOwnerOnlyFile);
    }
}

}  // namespace napkin::paths
