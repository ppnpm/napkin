#include "Paths.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <stdexcept>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <aclapi.h>
#  include <sddl.h>
#  include <string>
#  include <vector>
#endif

namespace napkin::paths {
namespace {

constexpr auto kOwnerOnlyDir  = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
constexpr auto kOwnerOnlyFile = QFile::ReadOwner | QFile::WriteOwner;

#ifdef Q_OS_WIN
// Windows' 0700. QFile::setPermissions only toggles the read-only attribute
// there, and the first Windows run showed the data directory readable by group
// and world as far as its ACL was concerned. So the directory gets an explicit
// DACL — this user, SYSTEM and Administrators — inherited by everything created
// inside it, and protected, so a loosened parent folder cannot loosen it.
// Setting it also propagates to whatever is already inside.
void restrictToOwner(const QString& path)
{
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) return;
    DWORD len = 0;
    ::GetTokenInformation(token, TokenUser, nullptr, 0, &len);
    std::vector<BYTE> user(len);
    const bool gotUser = ::GetTokenInformation(token, TokenUser, user.data(), len, &len);
    ::CloseHandle(token);
    if (!gotUser) return;

    LPWSTR sid = nullptr;
    if (!::ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid, &sid))
        return;
    const std::wstring sddl =
        L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;" + std::wstring(sid) + L")";
    ::LocalFree(sid);

    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
                                                                &sd, nullptr))
        return;
    BOOL present = FALSE, defaulted = FALSE;
    PACL dacl = nullptr;
    if (::GetSecurityDescriptorDacl(sd, &present, &dacl, &defaulted) && present) {
        std::wstring native = QDir::toNativeSeparators(path).toStdWString();
        ::SetNamedSecurityInfoW(native.data(), SE_FILE_OBJECT,
                                DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                nullptr, nullptr, dacl, nullptr);
    }
    ::LocalFree(sd);
}
#endif

void mkdirOrThrow(const QString& path)
{
    if (!QDir().mkpath(path))
        throw std::runtime_error(QString("cannot create directory: %1").arg(path).toStdString());
#ifdef Q_OS_WIN
    restrictToOwner(path);
#else
    QFile::setPermissions(path, kOwnerOnlyDir);
#endif
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
    // On Windows the files inherit the data directory's ACL; setPermissions
    // would only clear the read-only attribute, which is harmless.
    for (const auto& suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")}) {
        const QString path = databaseFile() + suffix;
        if (QFile::exists(path)) QFile::setPermissions(path, kOwnerOnlyFile);
    }
}

}  // namespace napkin::paths
