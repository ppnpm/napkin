#include "Thumbnailer.h"
#include "BlobStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPixmapCache>

namespace napkin {

Thumbnailer::Thumbnailer(QString cacheDir, BlobStore& blobs)
    : cacheDir_(std::move(cacheDir)), blobs_(blobs) {}

QString Thumbnailer::cachePath(const QString& hash, int maxSize) const
{
    return QStringLiteral("%1/%2/%3_%4.png").arg(cacheDir_, hash.left(2), hash).arg(maxSize);
}

QPixmap Thumbnailer::forBlob(const QString& hash, const QString& mime, int maxSize)
{
    if (hash.isEmpty()) return {};

    const QString key = QStringLiteral("napkin_thumb_%1_%2").arg(hash).arg(maxSize);
    QPixmap cached;
    if (QPixmapCache::find(key, &cached)) return cached;

    const QString thumbPath = cachePath(hash, maxSize);
    if (QFile::exists(thumbPath) && cached.load(thumbPath)) {
        QPixmapCache::insert(key, cached);
        return cached;
    }

    const QString blobPath = blobs_.pathFor(hash, mime);
    if (!QFile::exists(blobPath)) return {};

    // Scaled during decode, so a 4000x3000 photo never lands in memory whole.
    QImageReader reader(blobPath);
    reader.setAutoTransform(true);
    const QSize full = reader.size();
    if (full.isValid()) {
        QSize target = full;
        target.scale(maxSize, maxSize, Qt::KeepAspectRatio);
        reader.setScaledSize(target);
    }

    const QImage image = reader.read();
    if (image.isNull()) return {};

    const QPixmap pixmap = QPixmap::fromImage(image);
    QPixmapCache::insert(key, pixmap);

    QDir().mkpath(QFileInfo(thumbPath).absolutePath());
    pixmap.save(thumbPath, "PNG");
    QFile::setPermissions(thumbPath, QFile::ReadOwner | QFile::WriteOwner);
    return pixmap;
}

void Thumbnailer::forget(const QString& hash)
{
    for (int size : {kCardSize}) QFile::remove(cachePath(hash, size));
}

}  // namespace napkin
