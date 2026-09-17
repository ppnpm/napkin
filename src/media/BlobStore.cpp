#include "BlobStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QBuffer>

#include <fcntl.h>
#include <unistd.h>

namespace napkin {
namespace {

// Formats we are willing to keep byte-for-byte. Anything else that still
// decodes gets normalised to PNG on the way in.
bool isPassthroughMime(const QString& mime)
{
    return mime == QLatin1String("image/png") || mime == QLatin1String("image/jpeg")
        || mime == QLatin1String("image/webp") || mime == QLatin1String("image/gif");
}

QString sniffMime(const QByteArray& bytes)
{
    QBuffer buf;
    buf.setData(bytes);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf);
    const QString format = QString::fromLatin1(reader.format()).toLower();
    if (format == QLatin1String("png"))  return QStringLiteral("image/png");
    if (format == QLatin1String("jpeg") || format == QLatin1String("jpg"))
        return QStringLiteral("image/jpeg");
    if (format == QLatin1String("webp")) return QStringLiteral("image/webp");
    if (format == QLatin1String("gif"))  return QStringLiteral("image/gif");
    return {};
}

}  // namespace

BlobStore::BlobStore(QString rootDir) : root_(std::move(rootDir)) {}

QString BlobStore::extensionFor(const QString& mime)
{
    if (mime == QLatin1String("image/jpeg")) return QStringLiteral("jpg");
    if (mime == QLatin1String("image/webp")) return QStringLiteral("webp");
    if (mime == QLatin1String("image/gif"))  return QStringLiteral("gif");
    return QStringLiteral("png");
}

QString BlobStore::mimeForExtension(const QString& ext)
{
    const QString e = ext.toLower();
    if (e == QLatin1String("jpg") || e == QLatin1String("jpeg")) return QStringLiteral("image/jpeg");
    if (e == QLatin1String("webp")) return QStringLiteral("image/webp");
    if (e == QLatin1String("gif"))  return QStringLiteral("image/gif");
    return QStringLiteral("image/png");
}

QString BlobStore::pathFor(const QString& hash, const QString& mime) const
{
    return QStringLiteral("%1/%2/%3.%4")
        .arg(root_, hash.left(2), hash, extensionFor(mime));
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
    QString mime = sniffMime(bytes);
    if (mime.isEmpty()) mime = mimeHint;

    QImage image;
    if (!image.loadFromData(payload)) {
        out.error = QObject::tr("That does not look like an image Napkin can read.");
        return out;
    }

    // Keep the original bytes when the format is already one we serve; only
    // re-encode the odd formats, where the cost is paid once and rarely.
    if (!isPassthroughMime(mime)) {
        payload.clear();
        QBuffer buf(&payload);
        buf.open(QIODevice::WriteOnly);
        if (!image.save(&buf, "PNG")) {
            out.error = QObject::tr("Napkin could not convert that image.");
            return out;
        }
        mime = QStringLiteral("image/png");
    }

    out.hash = QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
    out.mime     = mime;
    out.size     = image.size();
    out.byteSize = payload.size();

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
