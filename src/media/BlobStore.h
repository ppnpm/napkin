#pragma once
#include <QByteArray>
#include <QSize>
#include <QString>
#include <vector>

namespace napkin {

// Content-addressed image storage. Identical bytes hash to the same name, so
// pasting the same screenshot twice costs one copy — and there is no refcount
// to drift, because liveness is a query against items.blob_hash (SPEC.md §5).
class BlobStore {
public:
    // Anything larger is almost certainly not a scratch-surface image. Refused
    // with a clear message rather than silently swallowing a gigabyte.
    static constexpr qint64 kMaxBytes = 64LL * 1024 * 1024;

    // Bytes are not the only way to be enormous: a 48 KB PNG can declare
    // 20000x20000 and cost 1.6 GB to decode.
    //
    // 48 megapixels, not 80. QImageReader's default allocation limit is 256 MB,
    // i.e. 64 megapixels at 4 bytes a pixel — so an 80 MP ceiling let Napkin
    // accept images it could then never display, and the card reported them as
    // missing from disk while the file sat right there.
    static constexpr qint64 kMaxPixels = 48LL * 1000 * 1000;

    struct Stored {
        QString hash;
        QString mime;
        QSize   size;
        qint64  byteSize = 0;
        bool    animated = false;
        bool    ok = false;
        QString error;   // plain language, safe to show (SPEC.md §14)
    };

    explicit BlobStore(QString rootDir);

    // Stores the bytes verbatim for every format this build can decode, so a
    // JPEG stays a JPEG, a GIF keeps its frames and an SVG stays vector. Only
    // bytes nothing can read are rejected. Invariant 6: the file is fsynced and
    // renamed into place before this returns, so the caller may then commit.
    Stored store(const QByteArray& bytes, const QString& mimeHint = {});

    QString pathFor(const QString& hash, const QString& mime) const;
    bool exists(const QString& hash, const QString& mime) const;

    // Invariant 7: only call once the referencing row deletion has committed.
    bool remove(const QString& hash, const QString& mime);

    // Every blob currently on disk, as "<hash>.<ext>" relative names.
    std::vector<QString> allStoredFiles() const;

    QString rootDir() const { return root_; }

private:
    QString root_;
};

}  // namespace napkin
