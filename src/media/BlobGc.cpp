#include "BlobGc.h"
#include "BlobStore.h"
#include "../data/ItemRepository.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace napkin {

GcResult reconcileBlobs(ItemRepository& items, BlobStore& blobs, const QString& thumbnailDir)
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

    // Thumbnails are renderings of the same private content and must not
    // outlive it. Emptying the trash previously reclaimed the blob and left a
    // picture of it in the data directory for ever.
    if (!thumbnailDir.isEmpty()) {
        QDirIterator thumbs(thumbnailDir, QDir::Files, QDirIterator::Subdirectories);
        while (thumbs.hasNext()) {
            thumbs.next();
            const QFileInfo info(thumbs.fileInfo());
            // Named "<hash>_<size>.png".
            const QString hash = info.completeBaseName().section(QLatin1Char('_'), 0, 0);
            if (referenced.contains(hash)) continue;
            if (QFile::remove(info.absoluteFilePath())) ++result.thumbnailsRemoved;
        }
    }

    // The other direction: rows pointing at files that are gone.
    for (const auto& item : items.allImageItems())
        if (!blobs.exists(item.blobHash, item.mime))
            result.missingBlobs.push_back(item.blobHash);

    return result;
}

}  // namespace napkin
