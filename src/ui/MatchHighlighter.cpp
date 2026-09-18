#include "MatchHighlighter.h"
#include "Tokens.h"
#include "../domain/Links.h"

#include <QGuiApplication>
#include <QTextDocument>

namespace napkin {

MatchHighlighter::MatchHighlighter(QTextDocument* document) : QSyntaxHighlighter(document) {}

void MatchHighlighter::setTerms(const QStringList& terms)
{
    if (terms_ == terms) return;
    terms_ = terms;
    rehighlight();
}

void MatchHighlighter::setMarkLinks(bool mark)
{
    if (markLinks_ == mark) return;
    markLinks_ = mark;
    rehighlight();
}

void MatchHighlighter::highlightBlock(const QString& text)
{
    // Links first, so a search term inside a URL still gets its wash on top.
    if (markLinks_) {
        QTextCharFormat linkFormat;
        linkFormat.setForeground(tokens::readableAccent(QGuiApplication::palette(), 1.0));
        linkFormat.setFontUnderline(true);
        for (const auto& span : links::findUrls(text, 64))
            setFormat(span.start, span.length, linkFormat);
    }

    if (terms_.isEmpty()) return;

    QTextCharFormat format;
    // A wash behind the word rather than a colour on it, so the text keeps the
    // contrast it was designed with and the mark survives any theme.
    QColor mark = tokens::readableAccent(QGuiApplication::palette(), 1.0);
    mark.setAlpha(tokens::isLightTheme(QGuiApplication::palette()) ? 60 : 90);
    format.setBackground(mark);
    format.setFontWeight(QFont::DemiBold);

    for (const QString& term : terms_) {
        if (term.isEmpty()) continue;
        int from = 0;
        while ((from = text.indexOf(term, from, Qt::CaseInsensitive)) >= 0) {
            setFormat(from, int(term.size()), format);
            from += int(term.size());
        }
    }
}

}  // namespace napkin
