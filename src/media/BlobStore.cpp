#include "BlobStore.h"
#include "ImageFormats.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QBuffer>
#include <QImageReader>

#include <fcntl.h>
#include <unistd.h>

namespace napkin {
BlobStore::BlobStore(QString rootDir) : root_(std::move(rootDir)) {}

QString BlobStore::pathFor(const QString& hash, const QString& mime) const
{
    return QStringLiteral("%1/%2/%3.%4")
        .arg(root_, hash.left(2), hash, formats::extensionFor(mime));
}

bool BlobStore::exists(const QString& hash, const QString& mime) const
{
    return QFile::exists(pathFor(hash, mime));
}

BlobStore::Stored BlobStore::store(const QByteArray& bytes, const QString& mimeHint)
{
    Stored out;

    if (bytes.isEmpty()) { out.error = QObject::tr("The image was empty."); return out; }
    if (bytes.size() > kMaxBytes) {
        out.error = QObject::tr("That image is larger than %1 MB, which is more than Napkin keeps.")
                        .arg(kMaxBytes / (1024 * 1024));
        return out;
    }

    QByteArray payload = bytes;
    QString mime = formats::sniff(bytes);
    if (mime.isEmpty() && !mimeHint.isEmpty() && formats::canDecode(mimeHint)) mime = mimeHint;

    if (mime.isEmpty() || !formats::canDecode(mime)) {
        out.error = QObject::tr("Napkin cannot read that image format on this system.");
        return out;
    }

    // Read the geometry without decoding the whole thing: a 100-megapixel HEIC
    // should not be fully rasterised just to record how big it is.
    QSize size;
    bool animated = false;
    {
        QBuffer probe;
        probe.setData(payload);
        if (probe.open(QIODevice::ReadOnly)) {
            QImageReader reader(&probe);
            reader.setAutoTransform(true);
            size = reader.size();
            animated = reader.supportsAnimation() && reader.imageCount() > 1;
            if (!size.isValid()) {
                const QImage decoded = reader.read();   // some formats need it
                if (decoded.isNull()) {
                    out.error = QObject::tr("That does not look like an image Napkin can read.");
                    return out;
                }
                size = decoded.size();
            }
        }
    }

    // Every decodable format is kept byte for byte. Nothing is re-encoded, so
    // animation, vector geometry and original quality all survive.
    out.hash = QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
    out.mime     = mime;
    out.size     = size;
    out.byteSize = payload.size();
    out.animated = animated;

    const QString finalPath = pathFor(out.hash, mime);
    if (QFile::exists(finalPath)) { out.ok = true; return out; }  // dedupe

    const QString dir = QFileInfo(finalPath).absolutePath();
    if (!QDir().mkpath(dir)) {
        out.error = QObject::tr("Napkin could not create its image folder.");
        return out;
    }

    // Invariant 6. A crash at any point leaves at worst an orphan blob, which
    // the reconciliation sweep reclaims — never a row pointing at nothing.
    const QString tmpPath = finalPath + QStringLiteral(".tmp");
    {
        QFile f(tmpPath);
        if (!f.open(QIODevice::WriteOnly)) {
            out.error = QObject::tr("Napkin could not save the image. The buffer was not changed.");
            return out;
        }
        if (f.write(payload) != payload.size() || !f.flush() || ::fsync(f.handle()) != 0) {
            f.close();
            QFile::remove(tmpPath);
            out.error = QObject::tr("Napkin could not finish saving the image — the disk may be full.");
            return out;
        }
        f.close();
        QFile::setPermissions(tmpPath, QFile::ReadOwner | QFile::WriteOwner);
    }

    if (!QFile::rename(tmpPath, finalPath)) {
        QFile::remove(tmpPath);
        out.error = QObject::tr("Napkin could not store the image.");
        return out;
    }

    // The rename itself is only durable once the directory is synced.
    if (int dfd = ::open(dir.toLocal8Bit().constData(), O_RDONLY | O_DIRECTORY); dfd >= 0) {
        ::fsync(dfd);
        ::close(dfd);
    }

    out.ok = true;
    return out;
}

bool BlobStore::remove(const QString& hash, const QString& mime)
{
    const QString path = pathFor(hash, mime);
    return QFile::exists(path) ? QFile::remove(path) : true;
}

std::vector<QString> BlobStore::allStoredFiles() const
{
    std::vector<QString> out;
    QDirIterator it(root_, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString name = it.fileName();
        if (name.endsWith(QLatin1String(".tmp"))) continue;  // a crashed write
        out.push_back(name);
    }
    return out;
}

}  // namespace napkin
