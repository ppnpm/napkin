#pragma once
#include <QString>
#include <QtGlobal>

namespace napkin {

using BufferId = qint64;
using ItemId   = qint64;
using Timestamp = qint64;  // UTC epoch milliseconds (invariant 9)

inline constexpr BufferId kNoBuffer = 0;
inline constexpr ItemId   kNoItem   = 0;

enum class ItemType { Text, Image };

inline QString itemTypeName(ItemType t)
{
    return t == ItemType::Text ? QStringLiteral("text") : QStringLiteral("image");
}

inline ItemType itemTypeFromString(const QString& s)
{
    return s == QLatin1String("image") ? ItemType::Image : ItemType::Text;
}

}  // namespace napkin
