#include "TimeFormat.h"
#include <QDateTime>
#include <QLocale>

namespace napkin {

QString relativeTime(Timestamp when, Timestamp now)
{
    const qint64 delta = now - when;
    if (delta < 0) return QObject::tr("just now");  // clock skew; do not say "in 3 minutes"

    constexpr qint64 minute = 60'000, hour = 60 * minute;
    if (delta < minute) return QObject::tr("just now");
    if (delta < hour) {
        const int m = int(delta / minute);
        return m == 1 ? QObject::tr("a minute ago") : QObject::tr("%1 minutes ago").arg(m);
    }

    const QDateTime whenLocal = QDateTime::fromMSecsSinceEpoch(when);
    const QDateTime nowLocal  = QDateTime::fromMSecsSinceEpoch(now);
    const qint64 days = whenLocal.date().daysTo(nowLocal.date());

    if (days == 0) return QLocale::system().toString(whenLocal.time(), QLocale::ShortFormat);
    if (days == 1) return QObject::tr("Yesterday");
    if (days < 7)  return QObject::tr("%1 days ago").arg(days);

    if (whenLocal.date().year() == nowLocal.date().year())
        return QLocale::system().toString(whenLocal.date(), QStringLiteral("d MMM"));
    return QLocale::system().toString(whenLocal.date(), QStringLiteral("d MMM yyyy"));
}

}  // namespace napkin
