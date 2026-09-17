#pragma once
#include <QLocalServer>
#include <QObject>
#include <QString>

namespace napkin {

// Two processes on one SQLite file is easy to forget and painful to debug
// (SPEC.md §10). A second launch hands off to the first and exits.
class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(QObject* parent = nullptr);

    // True if we are the primary instance. False means another Napkin is
    // already running and has been asked to raise its window.
    bool acquire();

signals:
    void raiseRequested();

private:
    QString      key_;
    QLocalServer server_;
};

}  // namespace napkin
