#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>

namespace napkin {

// What this build can actually decode depends on which Qt image plugins are
// installed (qt6-imageformats, kimageformats, qt6-svg), so it is discovered at
// runtime rather than hard-coded. A missing plugin degrades to the next-best
// representation instead of failing.
namespace formats {

// Media types Napkin will store byte for byte, richest first. Ordering is the
// contract:
//   * vector before raster — an SVG is scalable and tiny
//   * animation-capable before static — a GIF offered alongside a PNG must not
//     be flattened into a still frame
//   * everything else before a transcode, which is always a last resort
const QStringList& preferenceOrder();

// True when this build has a decoder for that media type.
bool canDecode(const QString& mime);

// The media type of these bytes, by content rather than by what the source
// claimed. Empty when nothing can decode them.
QString sniff(const QByteArray& bytes);

// File extension to store a blob under, e.g. "image/jpeg" -> "jpg".
QString extensionFor(const QString& mime);
QString mimeForExtension(const QString& extension);

// Whether these bytes hold more than one frame.
bool isAnimated(const QByteArray& bytes, const QString& mime);

// Name filter for the picker, built from what this build can actually open.
QString pickerFilter();

}  // namespace formats
}  // namespace napkin
