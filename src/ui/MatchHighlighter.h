#pragma once
#include <QSyntaxHighlighter>
#include <QStringList>

namespace napkin {

// Marks two things inside a card's text: the words you searched for, and the
// URLs in the prose.
//
// The list already says which buffers matched and shows a snippet; once you are
// looking at the card itself the term has to be findable there too, or you are
// re-reading a paragraph hunting for what the search already knew.
//
// URLs are marked with an underline as well as a colour, because SPEC.md §14
// says meaning is never carried by colour alone — and an underline is what
// every other piece of software on the machine uses to mean "this is a link".
class MatchHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit MatchHighlighter(QTextDocument* document);

    void setTerms(const QStringList& terms);
    void setMarkLinks(bool mark);

protected:
    void highlightBlock(const QString& text) override;

private:
    QStringList terms_;
    bool markLinks_ = true;
};

}  // namespace napkin
