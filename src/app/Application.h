#pragma once
#include <QApplication>

namespace napkin {

// The exception boundary for the whole program.
//
// An exception thrown inside any slot, event handler or paint routine unwinds
// into Qt's event loop, which calls std::terminate. Wrapping individual write
// calls was not enough and the claim that it was is one this audit falsified:
// a DbError from a plain SELECT during a row click, or from a repaint, or from
// MainWindow's own construction, aborted the process with no dialog.
//
// Overriding notify() catches every one of them, because every event in a Qt
// application passes through here.
class Application : public QApplication {
    Q_OBJECT
public:
    Application(int& argc, char** argv);

    bool notify(QObject* receiver, QEvent* event) override;

private:
    void report(const QString& detail);

    bool reporting_ = false;   // a failure while reporting must not recurse
};

}  // namespace napkin
