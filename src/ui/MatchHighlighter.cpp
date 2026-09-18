#include "MatchHighlighter.h"
#include "Tokens.h"

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

void MatchHighlighter::highlightBlock(const QString& text)
{
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
