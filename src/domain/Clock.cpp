#include "Clock.h"
#include <QDateTime>

namespace napkin {
namespace {
std::function<Timestamp()> g_clock;
}

Timestamp nowMs()
{
    return g_clock ? g_clock() : QDateTime::currentMSecsSinceEpoch();
}

void setClockForTesting(std::function<Timestamp()> clock) { g_clock = std::move(clock); }
void resetClock() { g_clock = nullptr; }

}  // namespace napkin
