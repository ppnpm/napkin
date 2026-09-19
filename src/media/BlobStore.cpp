#include "BlobStore.h"
#include "ImageFormats.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QBuffer>
#include <QImageReader>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <io.h>
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace napkin {
namespace {

// Invariant 6 needs the bytes on disk before the rename, and the rename on disk
// before the row is committed. POSIX spells that fsync(file), rename,
// fsync(directory). Windows has no directory handle to sync; the equivalent is
// FlushFileBuffers on the file (which _commit is) and a rename issued with
// MOVEFILE_WRITE_THROUGH, which does not return until the move is on disk.
bool syncFile(QFile& f)
{
#ifdef Q_OS_WIN
    return ::_commit(f.handle()) == 0;
#else
    return ::fsync(f.handle()) == 0;
#endif
}

bool durableRename(const QString& from, const QString& to)
{
#ifdef Q_OS_WIN
    return ::MoveFileExW(reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(from).utf16()),
                         reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(to).utf16()),
                         MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (!QFile::rename(from, to)) return false;
    // The rename itself is only durable once the directory is synced.
    const QString dir = QFileInfo(to).absolutePath();
    if (int dfd = ::open(dir.toLocal8Bit().constData(), O_RDONLY | O_DIRECTORY); dfd >= 0) {
        ::fsync(dfd);
        ::close(dfd);
    }
    return true;
#endif
}

}  // namespace

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

    // An SVG is a program that can name files. Compressed SVG cannot be
    // inspected without decompressing it, so it is refused outright.
    if (mime == QLatin1String("image/svg+xml-compressed")) {
        out.error = QObject::tr("Napkin does not accept compressed SVG files.");
        return out;
    }
    if (mime == QLatin1String("image/svg+xml") && formats::svgHasExternalReferences(payload)) {
        out.error = QObject::tr(
            "That SVG refers to other files on this computer, so Napkin will not store it. "
            "An image that reaches outside itself could expose private files.");
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
            if (size.isValid() && qint64(size.width()) * size.height() > kMaxPixels) {
                out.error = QObject::tr(
                    "That image is %1 by %2 pixels, which is larger than Napkin will open.")
                        .arg(size.width()).arg(size.height());
                return out;
            }
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
            out.error = QObject::tr("The image could not be saved. The napkin was not changed.");
            return out;
        }
        if (f.write(payload) != payload.size() || !f.flush() || !syncFile(f)) {
            f.close();
            QFile::remove(tmpPath);
            out.error = QObject::tr("Napkin could not finish saving the image — the disk may be full.");
            return out;
        }
        f.close();
        QFile::setPermissions(tmpPath, QFile::ReadOwner | QFile::WriteOwner);
    }

    // Neither QFile::rename nor MoveFileEx without REPLACE_EXISTING will move
    // onto an existing file. The name is the content's hash, so if another
    // store of the same bytes got there between the exists() check above and
    // here, what is on disk is already exactly this image.
    if (!durableRename(tmpPath, finalPath)) {
        QFile::remove(tmpPath);
        if (QFile::exists(finalPath)) { out.ok = true; return out; }
        out.error = QObject::tr("Napkin could not store the image.");
        return out;
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
