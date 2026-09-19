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
// On Windows the permission bits say nothing — QFile::setPermissions only
// toggles the read-only attribute — so there the same promise is checked
// against the ACL itself: nobody may be let in but this user, SYSTEM and
// Administrators.
#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <aclapi.h>
#  include <sddl.h>
#  include <vector>

namespace {

QString currentUserSid()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
    DWORD len = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &len);
    std::vector<BYTE> buf(len);
    const bool ok = GetTokenInformation(token, TokenUser, buf.data(), len, &len);
    CloseHandle(token);
    LPWSTR s = nullptr;
    if (!ok || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buf.data())->User.Sid, &s))
        return {};
    const QString out = QString::fromWCharArray(s);
    LocalFree(s);
    return out;
}

// Every principal the path's DACL lets in, by SID. A null DACL lets in
// everyone, which is reported as such rather than as an empty list.
QStringList allowedSids(const QString& path)
{
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    if (GetNamedSecurityInfoW(native.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                              nullptr, nullptr, &dacl, nullptr, &sd) != ERROR_SUCCESS)
        return {QStringLiteral("<unreadable ACL>")};
    QStringList out;
    if (!dacl) out << QStringLiteral("<null DACL: everyone>");
    for (DWORD i = 0; dacl && i < dacl->AceCount; ++i) {
        void* ace = nullptr;
        if (!GetAce(dacl, i, &ace)) continue;
        if (static_cast<ACE_HEADER*>(ace)->AceType != ACCESS_ALLOWED_ACE_TYPE) continue;
        LPWSTR s = nullptr;
        if (ConvertSidToStringSidW(&static_cast<ACCESS_ALLOWED_ACE*>(ace)->SidStart, &s)) {
            out << QString::fromWCharArray(s);
            LocalFree(s);
        }
    }
    LocalFree(sd);
    return out;
}

// SYSTEM and BUILTIN\Administrators, besides the user.
void verifyOwnerOnly(const QString& path)
{
    const QStringList allowed{currentUserSid(), QStringLiteral("S-1-5-18"),
                              QStringLiteral("S-1-5-32-544")};
    const QStringList actual = allowedSids(path);
    QVERIFY2(!actual.isEmpty(), qPrintable("no one is allowed in, not even the user: " + path));
    for (const QString& sid : actual)
        QVERIFY2(allowed.contains(sid),
                 qPrintable(QStringLiteral("%1 lets in %2 (allowed: %3)")
                                .arg(path, actual.join(u' '), allowed.join(u' '))));
}

// The parent's ACL before loosenParent, as SDDL, so cleanup can put it back.
QString gParentSddl;

// The case the protected DACL exists for. A default profile folder is already
// owner-only, so on a stock machine an unprotected data directory passes too —
// which is exactly what a break-test of the first version of this check showed.
// So the parent is given an inheritable Everyone-read entry first: without the
// protection it flows straight into Napkin's directory.
bool loosenParent(const QString& parent)
{
    const std::wstring native = QDir::toNativeSeparators(parent).toStdWString();
    PACL old = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (GetNamedSecurityInfoW(native.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                              nullptr, nullptr, &old, nullptr, &sd) != ERROR_SUCCESS)
        return false;
    LPWSTR sddl = nullptr;
    if (ConvertSecurityDescriptorToStringSecurityDescriptorW(
            sd, SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &sddl, nullptr)) {
        gParentSddl = QString::fromWCharArray(sddl);
        LocalFree(sddl);
    }

    EXPLICIT_ACCESS_W everyone{};
    everyone.grfAccessPermissions = GENERIC_READ;
    everyone.grfAccessMode = GRANT_ACCESS;
    everyone.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    everyone.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    everyone.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    wchar_t world[] = L"EVERYONE";
    everyone.Trustee.ptstrName = world;
    PACL loosened = nullptr;
    const bool ok = SetEntriesInAclW(1, &everyone, old, &loosened) == ERROR_SUCCESS
        && SetNamedSecurityInfoW(const_cast<wchar_t*>(native.c_str()), SE_FILE_OBJECT,
                                 DACL_SECURITY_INFORMATION, nullptr, nullptr, loosened,
                                 nullptr) == ERROR_SUCCESS;
    LocalFree(loosened);
    LocalFree(sd);
    return ok;
}

void restoreParent(const QString& parent)
{
    if (gParentSddl.isEmpty()) return;
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            reinterpret_cast<const wchar_t*>(gParentSddl.utf16()), SDDL_REVISION_1, &sd, nullptr))
        return;
    BOOL present = FALSE, defaulted = FALSE;
    PACL dacl = nullptr;
    if (GetSecurityDescriptorDacl(sd, &present, &dacl, &defaulted) && present) {
        std::wstring native = QDir::toNativeSeparators(parent).toStdWString();
        SetNamedSecurityInfoW(native.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                              nullptr, nullptr, dacl, nullptr);
    }
    LocalFree(sd);
}

}  // namespace
#endif

class TestPaths : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationName(QStringLiteral("napkin"));
        QStandardPaths::setTestModeEnabled(true);
        QDir(napkin::paths::dataDir()).removeRecursively();
#ifdef Q_OS_WIN
        const QString parent = QFileInfo(napkin::paths::dataDir()).absolutePath();
        QVERIFY(QDir().mkpath(parent));
        QVERIFY2(loosenParent(parent), "could not loosen the parent's ACL, so nothing would be tested");
        QVERIFY(allowedSids(parent).contains(QStringLiteral("S-1-1-0")));
#endif
    }

    void cleanupTestCase()
    {
        QDir(napkin::paths::dataDir()).removeRecursively();
#ifdef Q_OS_WIN
        restoreParent(QFileInfo(napkin::paths::dataDir()).absolutePath());
#endif
        QStandardPaths::setTestModeEnabled(false);
    }

    void dataDirectoryIsOwnerOnly()
    {
        napkin::paths::ensureDirs();
#ifdef Q_OS_WIN
        verifyOwnerOnly(napkin::paths::dataDir());
#else
        const auto p = QFile::permissions(napkin::paths::dataDir());
        QVERIFY(p.testFlag(QFile::ReadOwner));
        QVERIFY(!p.testFlag(QFile::ReadGroup));
        QVERIFY(!p.testFlag(QFile::ReadOther));
#endif
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
#ifdef Q_OS_WIN
            verifyOwnerOnly(path);
#else
            const auto p = QFile::permissions(path);
            QVERIFY2(!p.testFlag(QFile::ReadGroup), qPrintable("group-readable: " + path));
            QVERIFY2(!p.testFlag(QFile::ReadOther), qPrintable("world-readable: " + path));
#endif
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
