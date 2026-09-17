#pragma once
#include "../src/data/BufferRepository.h"
#include "../src/data/Database.h"
#include "../src/data/ItemRepository.h"
#include "../src/domain/BufferService.h"
#include "../src/domain/Clock.h"
#include "../src/data/Migrations.h"
#include <QTest>

// QTest formats values through a char* overload; give it one for ItemType so a
// failure prints "Image" rather than an opaque pointer dump.
namespace QTest {
template <> inline char* toString(const napkin::ItemType& t)
{
    return qstrdup(napkin::itemTypeName(t).toUtf8().constData());
}
}  // namespace QTest

// Every test runs against an in-memory database with a controllable clock, so
// the whole suite is headless and never touches the user's real data.
struct TestDb {
    napkin::Database         db;
    napkin::BufferRepository buffers{db};
    napkin::ItemRepository   items{db};
    napkin::BufferService    service{db, buffers, items};
    qint64                   clock = 1'700'000'000'000LL;  // fixed, arbitrary

    TestDb()
    {
        napkin::setClockForTesting([this] { return clock; });
        db.open(QStringLiteral(":memory:"));
    }
    ~TestDb() { napkin::resetClock(); }

    void advanceDays(int days) { clock += days * napkin::kMsPerDay; }
};
