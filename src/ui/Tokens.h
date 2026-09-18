#pragma once
#include <QColor>
#include <QFont>
#include <QPalette>
#include <algorithm>
#include <cmath>

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
inline constexpr int kCardMaxWidth = 460;

// Enough for roughly 18 lines of body text or a landscape screenshot. Past this
// a card would own the board, so it clips with a fade and opens on double-click.
inline constexpr int kCardMaxHeight = 420;

// A card is never shorter than this, so a one-word paste is still a card you can
// aim at rather than a sliver. Every card's size depends only on its own
// content: nothing here is derived from what the neighbours are doing.
// One line of body text plus the chrome. Deliberately not padded out to
// something rounder: a min height that exceeds what a short card needs makes
// the board lie about how much is in it.
inline constexpr int kCardMinHeight = 88;

// Enough characters to overflow kCardMaxHeight at any column width we allow, so
// measuring beyond this cannot change a card's height.
inline constexpr int kMeasureLimit = 2048;

// Card chrome.
inline constexpr int kCardPad      = 16;   // content inset
inline constexpr int kCardFooterH  = 28;   // the copy action and the timestamp
inline constexpr int kCardRadius   = 10;
inline constexpr int kCardGap      = 20;

// A card's edge is a meaningful affordance, so it obeys the same 3:1 floor as
// everything else here. An earlier value of 62 was 1.63:1 in light — declared
// "quiet", measured invisible, and contradicting the constant four lines above
// it. Dark needs less to read as an edge, so it gets less.
inline constexpr int kCardBorderLight = 128;  // 3.09:1
inline constexpr int kCardBorderDark  = 108;  // 4.00:1
inline constexpr int kCardBorderHoverBoost = 46;

// QPalette::Highlight is chosen by the theme for *fills*, where the text on top
// carries the contrast. Used as a hairline against Base it is often far below
// 3:1 — Breeze Light's is 2.1:1 at full strength. Darken (or lighten, in a dark
// theme) until it genuinely reads as an edge.
inline QColor readableAccent(const QPalette& pal, qreal strength = 1.0)
{
    const QColor base = pal.color(QPalette::Base);
    const bool light = base.lightness() > 128;
    QColor accent = pal.color(QPalette::Highlight);

    auto relLum = [](const QColor& c) {
        auto ch = [](int v) {
            const qreal s = v / 255.0;
            return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * ch(c.red()) + 0.7152 * ch(c.green()) + 0.0722 * ch(c.blue());
    };
    auto ratio = [&](const QColor& a, const QColor& b) {
        const qreal la = relLum(a), lb = relLum(b);
        return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
    };

    for (int i = 0; i < 24 && ratio(accent, base) < 3.0; ++i)
        accent = light ? accent.darker(112) : accent.lighter(112);

    if (strength < 1.0) {
        // A softer variant for "selected but not editing", still above 3:1
        // because it only ever moves toward the accent, never back to Base.
        accent.setAlphaF(std::max(0.75, strength));
    }
    return accent;
}

inline int cardBorderAlpha(const QPalette& pal, bool hovered)
{
    const int base = pal.color(QPalette::Window).lightness() > 128 ? kCardBorderLight
                                                                  : kCardBorderDark;
    return hovered ? std::min(255, base + kCardBorderHoverBoost) : base;
}

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
