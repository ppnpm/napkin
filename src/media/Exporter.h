#pragma once
#include "../domain/Types.h"
#include <QString>
#include <QStringList>

namespace napkin {

class BlobRepositoryAccess;
class BlobStore;
class BufferRepository;
class ItemRepository;

// SPEC.md §13. Local-first without an exit is its own kind of lock-in, and a
// database with no export has no recovery path when it corrupts.
//
// The output is deliberately dull: a folder of ordinary files anyone can open
// with anything, plus a manifest holding what the filesystem cannot carry —
// ordering, timestamps, the pin and keep flags, and the hash of every image.
// Nothing here needs Napkin to read it back.
class Exporter {
public:
    struct Result {
        int     buffers = 0;
        int     items   = 0;
        int     images  = 0;
        qint64  bytes   = 0;
        QString rootDir;        // where it actually landed
        // Per-item failures. An export that skipped something must say so:
        // reporting success over a missing image is how a backup becomes a
        // false promise.
        QStringList problems;
        bool    ok = false;
        QString error;          // set only when nothing could be written
    };

    Exporter(BufferRepository& buffers, ItemRepository& items, BlobStore& blobs);

    // One buffer into its own folder beneath `destination`.
    Result exportBuffer(BufferId id, const QString& destination);

    // Every live buffer, one folder each, beneath a single dated folder.
    Result exportAll(const QString& destination);

    // Exposed for tests and for the folder names: user content becomes a
    // filename here, so this is the only place that decides what is allowed to.
    static QString slug(const QString& text, int maxChars = 40);

private:
    bool writeBuffer(BufferId id, const QString& parentDir, Result& result);

    BufferRepository& buffers_;
    ItemRepository&   items_;
    BlobStore&        blobs_;
};

}  // namespace napkin
