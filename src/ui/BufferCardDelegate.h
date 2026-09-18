#pragma once
#include <QPixmap>
#include <QStyledItemDelegate>

namespace napkin {

class Thumbnailer;

// Paints the buffer stack. The visual contract is SPEC.md §7: whitespace,
// subtle separators, restrained borders, clear typography — no shadows, no
// gradients, no large radii. Every colour comes from QPalette so light and dark
// both work without a second code path.
class BufferCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit BufferCardDelegate(QObject* parent = nullptr);

    // Optional: without one, image buffers simply render as text.
    void setThumbnailer(Thumbnailer* thumbnailer) { thumbnailer_ = thumbnailer; }

    // Only one card animates at a time — the one under the pointer. Animating
    // every visible GIF would break §12's idle-CPU budget for decoration.
    void setAnimationFrame(int row, const QPixmap& frame);
    void clearAnimationFrame();

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    // The card box inside the item rect, below the section label when the row
    // starts a section. The view needs this to place the inline editor.
    QRect cardRect(const QRect& itemRect, const QModelIndex& index) const;
    QRect contentRect(const QRect& itemRect, const QModelIndex& index) const;

    int collapsedHeight() const;
    int sectionHeight(const QModelIndex& index) const;

    static constexpr int kMarginX  = 16;
    static constexpr int kMarginY  = 4;
    static constexpr int kPadding  = 14;
    static constexpr int kRadius   = 6;
    static constexpr int kSectionH = 36;

    // A card holding two words was 1360px wide on a wide window, with the state
    // glyphs 1250px from the text they describe. SPEC §7 asks for content to
    // dominate; past this measure it is mostly margin pretending to be content.
    static constexpr int kMaxCardWidth = 760;

    static constexpr int kThumbSize = 52;       // a lone image
    static constexpr int kThumbSizeMulti = 38;  // several in a row

private:
    QFont timestampFont(const QFont& base) const;

    Thumbnailer* thumbnailer_ = nullptr;
    int     animatedRow_ = -1;
    QPixmap animatedFrame_;
};

}  // namespace napkin
