#pragma once
#include <QString>
#include <stdexcept>

namespace napkin {

// Thrown for any SQLite failure. Callers at service/UI boundaries translate
// these into the plain-language messages required by SPEC.md §14 — the raw
// text never reaches the user.
class DbError : public std::runtime_error {
public:
    explicit DbError(const QString& what)
        : std::runtime_error(what.toStdString()), message_(what) {}
    const QString& message() const { return message_; }

private:
    QString message_;
};

}  // namespace napkin
