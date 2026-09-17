#include "Preview.h"
#include <QStringList>

namespace napkin {
namespace {

QStringList nonBlankLines(const QString& text, int limit)
{
    QStringList out;
    for (const auto& raw : text.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        out << line;
        if (out.size() >= limit) break;
    }
    return out;
}

QString imageLabel(const Item& item)
{
    // A pasted screenshot has no filename; one added through the picker does.
    return item.sourceName.isEmpty() ? QStringLiteral("Screenshot") : item.sourceName;
}

}  // namespace

QString firstLine(const QString& text)
{
    const auto lines = nonBlankLines(text, 1);
    return lines.isEmpty() ? QString() : lines.first();
}

QString formatBytes(qint64 bytes)
{
    constexpr qint64 kb = 1024, mb = kb * 1024, gb = mb * 1024;
    if (bytes >= gb) return QStringLiteral("%1 GB").arg(double(bytes) / gb, 0, 'f', 1);
    if (bytes >= mb) return QStringLiteral("%1 MB").arg(double(bytes) / mb, 0, 'f', 1);
    if (bytes >= kb) return QStringLiteral("%1 KB").arg(bytes / kb);
    return QStringLiteral("%1 B").arg(bytes);
}

BufferPreview derivePreview(const std::vector<Item>& head, int totalCount)
{
    BufferPreview p;
    p.itemCount = totalCount;
    for (const auto& i : head) {
        if (i.type != ItemType::Image) continue;
        p.hasImage  = true;
        p.thumbHash = i.blobHash;
        p.thumbMime = i.mime;
        p.thumbAnimated = i.animated;
        break;
    }

    if (head.empty()) return p;

    const Item& first = head.front();
    if (first.type == ItemType::Text) {
        const auto lines = nonBlankLines(first.text, 2);
        if (!lines.isEmpty()) p.primary = lines.first();
        if (lines.size() > 1)  p.secondary = lines.at(1);
    } else {
        p.primary = imageLabel(first);
        if (first.width > 0 && first.height > 0)
            p.secondary = QStringLiteral("%1 × %2").arg(first.width).arg(first.height);
    }

    // A multi-item buffer says so, in place of whatever detail the first item
    // offered — the count is the more useful fact.
    if (totalCount > 1)
        p.secondary = QStringLiteral("%1 items").arg(totalCount);

    // A buffer holding only an untitled image still needs a primary line.
    if (p.primary.isEmpty() && p.hasImage) p.primary = QStringLiteral("Screenshot");

    return p;
}

}  // namespace napkin
