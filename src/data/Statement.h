#pragma once
#include "DbError.h"
#include "../domain/Types.h"
#include <sqlite3.h>
#include <QByteArray>
#include <QString>
#include <optional>

namespace napkin {

class Database;

// RAII prepared statement. Binding is 1-indexed to match SQLite; column reads
// are 0-indexed, also to match SQLite. Deliberately not smoothed over — the
// mismatch is SQLite's and hiding it causes worse bugs than it prevents.
class Statement {
public:
    Statement(Database& db, const char* sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement& bind(int i, qint64 v);
    Statement& bind(int i, int v);
    Statement& bind(int i, bool v);
    Statement& bind(int i, const QString& v);
    Statement& bind(int i, std::optional<qint64> v);
    Statement& bindNull(int i);

    bool step();   // true while a row is available
    void exec();   // step to completion, expecting no rows

    qint64  columnInt64(int c) const;
    int     columnInt(int c) const;
    double  columnDouble(int c) const;
    bool    columnBool(int c) const;
    QString columnText(int c) const;
    std::optional<qint64> columnOptInt64(int c) const;
    bool    columnIsNull(int c) const;

private:
    void check(int rc, const char* what) const;

    Database&     db_;
    sqlite3_stmt* stmt_ = nullptr;
};

}  // namespace napkin
