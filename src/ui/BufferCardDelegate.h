#pragma once
#include <QStyledItemDelegate>

namespace napkin {

// Paints the buffer stack. The visual contract is SPEC.md §7: whitespace,
// subtle separators, restrained borders, clear typography — no shadows, no
// gradients, no large radii. Every colour comes from QPalette so light and dark
// both work without a second code path.
class BufferCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit BufferCardDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    // The card box inside the item rect, below the section label when the row
    // starts a section. The view needs this to place the inline editor.
    QRect cardRect(const QRect& itemRect, const QModelIndex& index) const;
    QRect contentRect(const QRect& itemRect, const QModelIndex& index) const;

    // Set by the view from the editor's document height while a row is open.
    void setExpandedHeight(int h);
    int  expandedHeight() const { return expandedHeight_; }

    int collapsedHeight() const;
    int sectionHeight(const QModelIndex& index) const;

    static constexpr int kMarginX  = 16;
    static constexpr int kMarginY  = 4;
    static constexpr int kPadding  = 14;
    static constexpr int kRadius   = 6;
    static constexpr int kSectionH = 36;

private:
    QFont timestampFont(const QFont& base) const;

    int expandedHeight_ = 260;
};

}  // namespace napkin
