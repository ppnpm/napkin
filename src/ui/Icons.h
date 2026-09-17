#pragma once
#include <QColor>

class QPainter;
class QRect;

namespace napkin::icons {

// Drawn rather than shipped as assets: two glyphs at one size do not justify an
// icon theme, and vector paths stay crisp at any scale factor without a @2x set.
void drawPin(QPainter* p, const QRect& box, const QColor& colour);
void drawKeep(QPainter* p, const QRect& box, const QColor& colour);

inline constexpr int kGlyphSize = 13;

}  // namespace napkin::icons
