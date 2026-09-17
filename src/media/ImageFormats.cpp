#include "ImageFormats.h"

#include <QBuffer>
#include <QImageReader>
#include <QMimeDatabase>
#include <QSet>

namespace napkin::formats {
namespace {

QSet<QString> decodableMimes()
{
    static const QSet<QString> set = [] {
        QSet<QString> out;
        for (const auto& mime : QImageReader::supportedMimeTypes())
            out.insert(QString::fromUtf8(mime).toLower());
        return out;
    }();
    return set;
}

}  // namespace

const QStringList& preferenceOrder()
{
    static const QStringList order{
        // Vector first: scalable and usually a fraction of the size.
        QStringLiteral("image/svg+xml"),
        QStringLiteral("image/svg+xml-compressed"),
        // Animation-capable. These must outrank PNG, or a copied GIF arrives
        // as a single flattened frame.
        QStringLiteral("image/gif"),
        QStringLiteral("image/apng"),
        QStringLiteral("image/webp"),
        QStringLiteral("image/avif"),
        // Static rasters, best-quality first.
        QStringLiteral("image/png"),
        QStringLiteral("image/jxl"),
        QStringLiteral("image/heif"),
        QStringLiteral("image/heic"),
        QStringLiteral("image/avci"),
        QStringLiteral("image/jpeg"),
        QStringLiteral("image/tiff"),
        QStringLiteral("image/bmp"),
        QStringLiteral("image/x-icon"),
        QStringLiteral("image/vnd.microsoft.icon"),
    };
    return order;
}

bool canDecode(const QString& mime)
{
    if (mime.isEmpty()) return false;
    const QString lower = mime.toLower();
    if (decodableMimes().contains(lower)) return true;

    // Some plugins register only one spelling of a pair.
    if (lower == QLatin1String("image/heic")) return decodableMimes().contains(QStringLiteral("image/heif"));
    if (lower == QLatin1String("image/apng")) return decodableMimes().contains(QStringLiteral("image/png"));
    return false;
}

QString sniff(const QByteArray& bytes)
{
    if (bytes.isEmpty()) return {};

    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly)) return {};

    // QImageReader identifies by content, which is what we want: the source's
    // claim about its own bytes is not evidence.
    QImageReader reader(&buffer);
    const QByteArray format = reader.format();
    if (format.isEmpty()) return {};

    const QMimeDatabase db;
    const auto byContent = db.mimeTypeForData(bytes);
    if (byContent.isValid() && byContent.name().startsWith(QLatin1String("image/")))
        return byContent.name();

    // SVG is XML, so content sniffing can be ambiguous; trust the reader.
    if (format == "svg")  return QStringLiteral("image/svg+xml");
    if (format == "svgz") return QStringLiteral("image/svg+xml-compressed");
    return QStringLiteral("image/") + QString::fromLatin1(format).toLower();
}

QString extensionFor(const QString& mime)
{
    static const QHash<QString, QString> overrides{
        {QStringLiteral("image/jpeg"), QStringLiteral("jpg")},
        {QStringLiteral("image/svg+xml"), QStringLiteral("svg")},
        {QStringLiteral("image/svg+xml-compressed"), QStringLiteral("svgz")},
        {QStringLiteral("image/heic"), QStringLiteral("heic")},
        {QStringLiteral("image/apng"), QStringLiteral("apng")},
    };
    if (const auto it = overrides.constFind(mime.toLower()); it != overrides.constEnd())
        return *it;

    const QMimeDatabase db;
    const auto type = db.mimeTypeForName(mime);
    const QString suffix = type.isValid() ? type.preferredSuffix() : QString();
    return suffix.isEmpty() ? QStringLiteral("bin") : suffix;
}

QString mimeForExtension(const QString& extension)
{
    const QString e = extension.toLower();
    if (e == QLatin1String("jpg") || e == QLatin1String("jpeg")) return QStringLiteral("image/jpeg");
    if (e == QLatin1String("svg"))  return QStringLiteral("image/svg+xml");
    if (e == QLatin1String("svgz")) return QStringLiteral("image/svg+xml-compressed");
    if (e == QLatin1String("apng")) return QStringLiteral("image/apng");

    const QMimeDatabase db;
    const auto type = db.mimeTypeForFile(QStringLiteral("x.") + e, QMimeDatabase::MatchExtension);
    return type.isValid() ? type.name() : QStringLiteral("image/png");
}

bool isAnimated(const QByteArray& bytes, const QString& mime)
{
    if (mime.startsWith(QLatin1String("image/svg"))) return false;  // may animate, but not as frames

    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly)) return false;

    QImageReader reader(&buffer);
    return reader.supportsAnimation() && reader.imageCount() > 1;
}

QString pickerFilter()
{
    QStringList patterns;
    for (const auto& format : QImageReader::supportedImageFormats())
        patterns << QStringLiteral("*.") + QString::fromUtf8(format).toLower();
    patterns.removeDuplicates();
    patterns.sort();

    return QObject::tr("Images (%1);;All files (*)").arg(patterns.join(QLatin1Char(' ')));
}

}  // namespace napkin::formats
