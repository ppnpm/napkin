#include "ClipboardContent.h"

#include <QBuffer>
#include <QImage>
#include <QMimeData>

namespace napkin {

ClipboardContent readClipboard(const QMimeData* mime)
{
    ClipboardContent out;
    if (!mime) return out;

    // 1. A PNG we can keep byte for byte. This is what Spectacle and every
    //    browser "Copy Image" actually offer — measured in Phase 0.
    if (mime->hasFormat(QStringLiteral("image/png"))) {
        out.kind = ClipboardContent::Kind::Image;
        out.imageBytes = mime->data(QStringLiteral("image/png"));
        out.imageMime = QStringLiteral("image/png");
        out.via = QStringLiteral("image/png");
        if (!out.imageBytes.isEmpty()) return out;
        out = {};  // an empty payload is not an image; fall through
    }

    // 2. Any other image representation, normalised on the way in.
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
