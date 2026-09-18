#pragma once
#include <QSyntaxHighlighter>
#include <QStringList>

namespace napkin {

// Marks the searched-for words inside a card's text.
//
// The list already says which buffers matched and shows a snippet; once you are
// looking at the card itself the term has to be findable there too, or you are
// re-reading a paragraph hunting for what the search already knew.
class MatchHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit MatchHighlighter(QTextDocument* document);

    void setTerms(const QStringList& terms);

protected:
    void highlightBlock(const QString& text) override;

private:
    QStringList terms_;
};

}  // namespace napkin
