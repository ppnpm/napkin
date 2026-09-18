#include "Links.h"

#include <QRegularExpression>
#include <QUrl>

namespace napkin::links {
namespace {

// Deliberately conservative: an explicit scheme, or a bare www. host. Guessing
// that "example.com" is a link turns ordinary prose — version numbers, file
// names, sentences with no space after a full stop — into false chips, and a
// chip that is wrong is worse than a chip that is missing.
const QRegularExpression& pattern()
{
    static const QRegularExpression re(
        QStringLiteral(R"((?:https?://|www\.)[^\s<>"'`\x{0000}-\x{0020}]+)"),
        QRegularExpression::UseUnicodePropertiesOption);
    return re;
}

// Trailing punctuation belongs to the sentence, not the URL. Brackets are
// balanced rather than stripped, because a Wikipedia path legitimately contains
// them: "…/Foo_(bar)" must keep its closing parenthesis.
QString trimTrailing(QString url)
{
    static const QString kStrip = QStringLiteral(".,;:!?'\"");
    while (!url.isEmpty()) {
        const QChar last = url.back();
        if (kStrip.contains(last)) { url.chop(1); continue; }
        if (last == u')' || last == u']') {
            const QChar open = last == u')' ? u'(' : u'[';
            if (url.count(open) < url.count(last)) { url.chop(1); continue; }
        }
        break;
    }
    return url;
}

QString normalize(const QString& raw)
{
    return raw.startsWith(QLatin1String("www."), Qt::CaseInsensitive)
               ? QStringLiteral("https://") + raw
               : raw;
}

}  // namespace

bool isOpenable(const QString& candidate)
{
    const QUrl url(candidate, QUrl::StrictMode);
    if (!url.isValid() || url.isRelative()) return false;

    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) return false;

    // A scheme with no host is either malformed or a local path wearing a
    // scheme; neither is something to hand to the desktop.
    if (url.host().isEmpty()) return false;

    // Control characters survive QUrl parsing and are how a link is made to
    // display as one thing and resolve as another.
    for (const QChar c : candidate)
        if (c.category() == QChar::Other_Control) return false;

    return true;
}

std::optional<QString> soleUrl(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.contains(QChar::Space)
        || trimmed.contains(u'\n') || trimmed.contains(u'\t'))
        return std::nullopt;

    const QString candidate = normalize(trimmed);
    if (!isOpenable(candidate)) return std::nullopt;

    // The match must cover the whole thing: "https://a.example/x)y" is one
    // token but not one URL.
    const auto m = pattern().match(trimmed);
    if (!m.hasMatch() || m.capturedStart() != 0 || m.capturedLength() != trimmed.size())
        return std::nullopt;

    return candidate;
}

std::vector<Span> findUrls(const QString& text, int limit)
{
    std::vector<Span> out;
    auto it = pattern().globalMatch(text);
    while (it.hasNext() && int(out.size()) < limit) {
        const auto m = it.next();
        const QString trimmed = trimTrailing(m.captured());
        if (trimmed.isEmpty()) continue;
        const QString url = normalize(trimmed);
        if (!isOpenable(url)) continue;
        out.push_back({int(m.capturedStart()), int(trimmed.size()), url});
    }
    return out;
}

QString hostOf(const QString& url)
{
    return QUrl(url, QUrl::StrictMode).host();
}

QString displayForm(const QString& url, int maxChars)
{
    const QUrl parsed(url, QUrl::StrictMode);
    // The host exactly as QUrl resolves it, "www." included. Tidying it away
    // would mean the chip and this helper disagreed about what the host is,
    // and a link chip is the wrong place to be approximately truthful.
    const QString host = parsed.host();

    QString rest = parsed.path();
    if (parsed.hasQuery()) rest += u'?' + parsed.query();
    if (rest == QLatin1String("/")) rest.clear();

    QString shown = host + rest;
    if (shown.size() <= maxChars) return shown;

    // Elide the middle of the path, never the host: the host is the part that
    // says where this goes, so it is the part that must stay legible.
    const int keepTail = 12;
    const int keepHead = std::max(0, maxChars - keepTail - 1);
    if (host.size() >= keepHead) return host;
    return shown.left(keepHead) + QChar(0x2026) + shown.right(keepTail);
}

}  // namespace napkin::links
