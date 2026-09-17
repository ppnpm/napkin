#include "BlobGc.h"
#include "BlobStore.h"
#include "../data/ItemRepository.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace napkin {

GcResult reconcileBlobs(ItemRepository& items, BlobStore& blobs)
{
    GcResult result;

    QSet<QString> referenced;
    for (const auto& hash : items.allBlobHashes()) referenced.insert(hash);

    QDirIterator it(blobs.rootDir(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info(it.fileInfo());

        // A .tmp survives only when a write was interrupted: it was never
        // renamed into place, so nothing can reference it.
        if (info.suffix() == QLatin1String("tmp")) {
            if (QFile::remove(info.absoluteFilePath())) ++result.temporariesRemoved;
            continue;
        }

        if (!referenced.contains(info.completeBaseName())) {
            if (QFile::remove(info.absoluteFilePath())) ++result.orphansRemoved;
        }
    }

    // The other direction: rows pointing at files that are gone.
    for (const auto& item : items.allImageItems())
        if (!blobs.exists(item.blobHash, item.mime))
            result.missingBlobs.push_back(item.blobHash);

    return result;
}

}  // namespace napkin
