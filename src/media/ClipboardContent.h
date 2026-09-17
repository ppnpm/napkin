#pragma once
#include <QByteArray>
#include <QString>

class QMimeData;

namespace napkin {

// SPEC.md §4 makes the clipboard format preference order a contract, not a
// heuristic: image/png, then any other image/*, then text/plain, then nothing.
// The ambiguous case is real — copying an image in a browser offers both a
// bitmap and a URL, and the image must win.
struct ClipboardContent {
    enum class Kind { None, Image, Text };

    Kind       kind = Kind::None;
    QByteArray imageBytes;
    QString    imageMime;
    QString    text;
    QString    via;   // which branch fired, for the log — never user content
};

ClipboardContent readClipboard(const QMimeData* mime);

}  // namespace napkin
