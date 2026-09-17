#include "ClipboardContent.h"

#include <QBuffer>
#include <QImage>
#include <QMimeData>
#include "ImageFormats.h"

namespace napkin {

ClipboardContent readClipboard(const QMimeData* mime)
{
    ClipboardContent out;
    if (!mime) return out;

    // 1. The richest representation the source offers that this build can
    //    decode, kept byte for byte. Vector beats raster, animation-capable
    //    beats static — a GIF offered alongside a PNG must not be flattened.
    for (const QString& candidate : formats::preferenceOrder()) {
        if (!mime->hasFormat(candidate)) continue;
        if (!formats::canDecode(candidate)) continue;   // no plugin; try the next

        const QByteArray payload = mime->data(candidate);
        if (payload.isEmpty()) continue;

        out.kind = ClipboardContent::Kind::Image;
        out.imageBytes = payload;
        out.imageMime = candidate;
        out.via = candidate;
        return out;
    }

    // 2. Any other image representation, normalised on the way in. This is the
    //    lossy path: it flattens animation, so it is deliberately last.
    if (mime->hasImage()) {
        const QImage image = qvariant_cast<QImage>(mime->imageData());
        if (!image.isNull()) {
            QByteArray png;
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            if (image.save(&buffer, "PNG") && !png.isEmpty()) {
                out.kind = ClipboardContent::Kind::Image;
                out.imageBytes = png;
                out.imageMime = QStringLiteral("image/png");
                out.via = QStringLiteral("image/* transcoded");
                return out;
            }
        }
    }

    // 3. Text. Deliberately last: an image on the clipboard outranks the URL
    //    that came with it.
    if (mime->hasText() && !mime->text().isEmpty()) {
        out.kind = ClipboardContent::Kind::Text;
        out.text = mime->text();
        out.via = QStringLiteral("text/plain");
        return out;
    }

    return out;
}

}  // namespace napkin
