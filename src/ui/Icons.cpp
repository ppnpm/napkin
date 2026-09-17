#include "Icons.h"
#include <QPainter>
#include <QPainterPath>
#include <QRect>

namespace napkin::icons {

// A pushpin seen head-on: round head, tapered shaft going down-left.
void drawPin(QPainter* p, const QRect& box, const QColor& colour)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r(box);
    const qreal cx = r.center().x(), cy = r.center().y(), s = r.width();

    QPainterPath head;
    head.addEllipse(QPointF(cx + s * 0.08, cy - s * 0.12), s * 0.26, s * 0.26);
    p->fillPath(head, colour);

    QPen pen(colour, std::max(1.0, s * 0.11), Qt::SolidLine, Qt::RoundCap);
    p->setPen(pen);
    p->drawLine(QPointF(cx - s * 0.10, cy + s * 0.06), QPointF(cx - s * 0.34, cy + s * 0.40));
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

}  // namespace napkin::icons
