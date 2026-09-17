#include "Statement.h"
#include "Database.h"

namespace napkin {

Statement::Statement(Database& db, const char* sql) : db_(db)
{
    if (sqlite3_prepare_v2(db.handle(), sql, -1, &stmt_, nullptr) != SQLITE_OK)
        db_.fail(QString("prepare failed [%1]").arg(QString::fromUtf8(sql).left(120)));
}

Statement::~Statement() { sqlite3_finalize(stmt_); }

void Statement::check(int rc, const char* what) const
{
    if (rc != SQLITE_OK) db_.fail(QString::fromUtf8(what));
}

Statement& Statement::bind(int i, qint64 v) { check(sqlite3_bind_int64(stmt_, i, v), "bind int64"); return *this; }
Statement& Statement::bind(int i, int v)    { check(sqlite3_bind_int(stmt_, i, v), "bind int"); return *this; }
Statement& Statement::bind(int i, bool v)   { check(sqlite3_bind_int(stmt_, i, v ? 1 : 0), "bind bool"); return *this; }

Statement& Statement::bind(int i, const QString& v)
{
    const QByteArray utf8 = v.toUtf8();
    check(sqlite3_bind_text(stmt_, i, utf8.constData(), utf8.size(), SQLITE_TRANSIENT), "bind text");
    return *this;
}

Statement& Statement::bind(int i, std::optional<qint64> v)
{
    return v ? bind(i, *v) : bindNull(i);
}

Statement& Statement::bindNull(int i) { check(sqlite3_bind_null(stmt_, i), "bind null"); return *this; }

bool Statement::step()
{
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    db_.fail("step failed");
}

void Statement::exec()
{
    if (step()) throw DbError("statement returned rows where none were expected");
}

qint64  Statement::columnInt64(int c) const { return sqlite3_column_int64(stmt_, c); }
int     Statement::columnInt(int c) const   { return sqlite3_column_int(stmt_, c); }
bool    Statement::columnBool(int c) const  { return sqlite3_column_int(stmt_, c) != 0; }
bool    Statement::columnIsNull(int c) const { return sqlite3_column_type(stmt_, c) == SQLITE_NULL; }

QString Statement::columnText(int c) const
{
    const auto* p = sqlite3_column_text(stmt_, c);
    if (!p) return {};
    return QString::fromUtf8(reinterpret_cast<const char*>(p), sqlite3_column_bytes(stmt_, c));
}

std::optional<qint64> Statement::columnOptInt64(int c) const
{
    if (columnIsNull(c)) return std::nullopt;
    return sqlite3_column_int64(stmt_, c);
}

}  // namespace napkin
