#pragma once
#include <QPixmap>
#include <QString>

namespace napkin {

class BlobStore;

// Card thumbnails. Generated once, cached on disk, then held in Qt's shared
// pixmap cache — §12 forbids loading full-resolution images just to draw a list.
class Thumbnailer {
public:
    static constexpr int kCardSize = 96;  // logical pixels, square bounding box

    Thumbnailer(QString cacheDir, BlobStore& blobs);

    // Null pixmap when the blob is missing; callers draw a placeholder rather
    // than nothing, so a vanished file is visible instead of silent.
    QPixmap forBlob(const QString& hash, const QString& mime, int maxSize = kCardSize);

    void forget(const QString& hash);

private:
    QString cachePath(const QString& hash, int maxSize) const;

    QString    cacheDir_;
    BlobStore& blobs_;
};

}  // namespace napkin
