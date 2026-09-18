#include "Icons.h"
#include <QPainter>
#include <QPainterPath>
#include <QRect>

namespace napkin::icons {

// A pushpin: flat head, a crossbar, a tapered shaft and a point.
//
// The first attempt was a filled circle with a straight stem, which at 13px is
// the universal magnifying-glass idiom — and would have collided head-on with
// the search field arriving in the header. The crossbar and the point are what
// make it read as a pin rather than a lens.
void drawPin(QPainter* p, const QRect& box, const QColor& colour)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r(box);
    const qreal cx = r.center().x(), top = r.top() + r.height() * 0.10;
    const qreal w = r.width(), h = r.height();

    QPainterPath pin;
    pin.moveTo(cx - w * 0.26, top);                  // flat head
    pin.lineTo(cx + w * 0.26, top);
    pin.lineTo(cx + w * 0.17, top + h * 0.16);
    pin.lineTo(cx + w * 0.38, top + h * 0.42);       // crossbar, right wing
    pin.lineTo(cx + w * 0.07, top + h * 0.50);
    pin.lineTo(cx,            top + h * 0.90);       // point
    pin.lineTo(cx - w * 0.07, top + h * 0.50);
    pin.lineTo(cx - w * 0.38, top + h * 0.42);       // crossbar, left wing
    pin.lineTo(cx - w * 0.17, top + h * 0.16);
    pin.closeSubpath();
    p->fillPath(pin, colour);
    p->restore();
}

// A bookmark: what you mark so it is still there later. Deliberately not a
// padlock — Keep is retention, not security, and the icon must not promise
// otherwise (SPEC.md §3).
void drawKeep(QPainter* p, const QRect& box, const QColor& colour)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r(box);
    const qreal w = r.width() * 0.62, h = r.height() * 0.78;
    const qreal x = r.center().x() - w / 2, y = r.center().y() - h / 2;

    QPainterPath mark;
    mark.moveTo(x, y);
    mark.lineTo(x + w, y);
    mark.lineTo(x + w, y + h);
    mark.lineTo(x + w / 2, y + h * 0.66);
    mark.lineTo(x, y + h);
    mark.closeSubpath();
    p->fillPath(mark, colour);
    p->restore();
}

// Two offset sheets, which is what every toolbar in the world means by copy.
void drawCopy(QPainter* p, const QRect& box, const QColor& colour)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r(box);
    const qreal w = r.width(), h = r.height();
    QPen pen(colour, std::max(1.0, w * 0.085));
    pen.setJoinStyle(Qt::RoundJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(QRectF(r.left() + w * 0.06, r.top() + w * 0.06,
                              w * 0.62, h * 0.62), w * 0.10, w * 0.10);
    p->drawRoundedRect(QRectF(r.left() + w * 0.32, r.top() + w * 0.32,
                              w * 0.62, h * 0.62), w * 0.10, w * 0.10);
    p->restore();
}

void drawLink(QPainter* p, const QRect& box, const QColor& colour)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r(box);
    const qreal w = r.width(), h = r.height();
    QPen pen(colour, std::max(1.2, w * 0.10));
    pen.setCapStyle(Qt::RoundCap);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);

    // Two capsules on a diagonal with the bar between them: a chain link, at a
    // size where a more literal drawing turns to mush.
    const qreal cw = w * 0.46, ch = h * 0.30;
    QRectF upper(r.left() + w * 0.06, r.top() + h * 0.10, cw, ch);
    QRectF lower(r.left() + w * 0.48, r.top() + h * 0.60, cw, ch);
    p->drawRoundedRect(upper, ch / 2, ch / 2);
    p->drawRoundedRect(lower, ch / 2, ch / 2);
    p->drawLine(QPointF(r.left() + w * 0.36, r.top() + h * 0.56),
                QPointF(r.left() + w * 0.64, r.top() + h * 0.44));
    p->restore();
}

}  // namespace napkin::icons
