#pragma once
#include <QColor>
#include <QFont>
#include <QPalette>

// The visual system, in one place. Every alpha here is a measured contrast
// threshold composited over QPalette::Base against Breeze Light, the harsher of
// the two themes — not a taste choice. Anything readable stays at or above 161.
namespace napkin::tokens {

// --- text alphas -------------------------------------------------------------
inline constexpr int kTextPrimary   = 255;  // >= 15:1
inline constexpr int kTextSecondary = 170;  // 5.06:1  — secondary lines, meta
inline constexpr int kTextTertiary  = 161;  // 4.51:1  — timestamps, captions,
                                            //           section labels, and
                                            //           placeholders, which are
                                            //           text and get no exemption

// --- non-text ----------------------------------------------------------------
inline constexpr int kHairline      = 36;   // dividers, image edges
inline constexpr int kItemHover     = 60;
inline constexpr int kRailHover     = 90;
inline constexpr int kCardResting   = 128;  // 3.07:1 — floor for a meaningful affordance
inline constexpr int kCardActive    = 178;
inline constexpr int kBorderSelected = 160;
inline constexpr int kBorderEditing  = 200;
inline constexpr int kFillSelectedLight = 26;
inline constexpr int kFillSelectedDark  = 44;

// --- spacing, on a 4px scale -------------------------------------------------
inline constexpr int kGapTight  = 8;
inline constexpr int kGapItem   = 24;
inline constexpr int kPadX      = 32;
inline constexpr int kPadTop    = 28;
inline constexpr int kPadBottom = 120;   // clickable dead space below the composer

// --- geometry ----------------------------------------------------------------
// Cards, not a column. A board of pasted things wants a uniform width and each
// card's own height; a single 780px column gave a three-word note a 780px row.
inline constexpr int kCardMinWidth = 280;
inline constexpr int kCardMaxWidth = 400;

// Enough for roughly 18 lines of body text or a landscape screenshot. Past this
// a card would own the board, so it clips with a fade and opens on double-click.
inline constexpr int kCardMaxHeight = 420;

inline constexpr int kRailOffset     = 26;
inline constexpr int kRailWidth      = 3;
inline constexpr int kSelectionBleed = 12;   // so the text never moves between states

// --- radii, nothing above 8 --------------------------------------------------
inline constexpr int kRadiusImage     = 4;
inline constexpr int kRadiusSelection = 5;
inline constexpr int kRadiusCard      = 6;
inline constexpr int kRadiusToast     = 8;

inline QColor text(const QPalette& pal, int alpha)
{
    QColor c = pal.color(QPalette::Text);
    c.setAlpha(alpha);
    return c;
}

inline QColor highlight(const QPalette& pal, int alpha)
{
    QColor c = pal.color(QPalette::Highlight);
    c.setAlpha(alpha);
    return c;
}

inline bool isLightTheme(const QPalette& pal)
{
    return pal.color(QPalette::Window).lightness() > 128;
}

inline QFont scaled(const QFont& base, qreal deltaPt, int weight = -1)
{
    QFont f = base;
    f.setPointSizeF(std::max(7.0, base.pointSizeF() + deltaPt));
    if (weight >= 0) f.setWeight(QFont::Weight(weight));
    return f;
}

}  // namespace napkin::tokens
