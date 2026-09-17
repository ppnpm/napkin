#pragma once
#include <QString>

namespace napkin::paths {

// SPEC.md §10. Standard XDG locations on Linux; never beside the executable,
// never requiring root. Mode 0700 on the data dir, 0600 on the database —
// a scratch surface will hold tokens and passwords in practice (§11).
QString dataDir();
QString configDir();
QString databaseFile();
QString blobsDir();
QString thumbsDir();

// Creates every directory Napkin needs and applies permissions.
// Throws std::runtime_error if the data directory is not usable.
void ensureDirs();

// Marks the data directory so desktop search indexers skip it (§11): what the
// user threw into Napkin should not become system-wide searchable.
void markNotIndexable();

// Restricts the database and its WAL sidecars to the owner. Must be called
// AFTER the database is opened: SQLite creates napkin.db, -wal and -shm itself,
// subject to the process umask, so on a first run there is nothing to chmod
// until open() has happened. The sidecars matter as much as the database —
// uncommitted user content lives in the WAL.
void secureDatabaseFiles();

}  // namespace napkin::paths
