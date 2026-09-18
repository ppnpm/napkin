#pragma once
#include <QString>
#include <optional>
#include <vector>

namespace napkin::links {

// A URL found inside a larger piece of text, as a span into that text.
struct Span {
    int     start  = 0;
    int     length = 0;
    QString url;      // normalized: what would actually be opened
};

// SPEC.md §3: a URL is not a stored type. It is a text item that happens to
// render as a link chip, so every rule about links lives here, in one function,
// and changing it needs no migration.
//
// Only http and https are openable. A scratch surface holds whatever was on the
// clipboard and "Open" hands it to the desktop, so the schemes that can execute
// something (javascript:, data:) or reach into the filesystem (file:) are never
// offered — not hidden behind a confirmation, simply not treated as links.
bool isOpenable(const QString& candidate);

// The trimmed text is exactly one URL and nothing else.
std::optional<QString> soleUrl(const QString& text);

// Every URL inside prose. Bounded, because a pasted log can contain thousands
// and the highlighter runs on every keystroke.
std::vector<Span> findUrls(const QString& text, int limit = 200);

// What the chip shows. Derived from QUrl::host() rather than from the raw
// string: "https://evil.example@bank.example/" reads as bank.example to a
// person skimming it, and the chip must not repeat that lie.
QString hostOf(const QString& url);

// Host plus enough path to tell two links apart, elided to fit a chip.
QString displayForm(const QString& url, int maxChars = 72);

}  // namespace napkin::links
