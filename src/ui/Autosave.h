#pragma once
#include <QObject>
#include <QTimer>
#include <functional>

namespace napkin {

// SPEC.md §8. A plain debounce would make its own window the data-loss window,
// so there are two timers: a short one that coalesces keystrokes, and a hard
// max-delay that fires regardless during continuous typing. Combined with
// WAL + synchronous=NORMAL, an application crash can cost at most kMaxDelayMs
// of work, and never a completed edit.
class Autosave : public QObject {
    Q_OBJECT
public:
    static constexpr int kDebounceMs = 300;
    static constexpr int kMaxDelayMs = 2000;

    explicit Autosave(QObject* parent = nullptr);

    void setFlushHandler(std::function<void()> handler) { flush_ = std::move(handler); }

    // Content changed. Restarts the debounce; leaves the max-delay running.
    void noteChange();

    // Write now if anything is pending. Called on collapse, buffer switch,
    // window blur and close — the moments where waiting would be indefensible.
    void flushNow();

    bool isDirty() const { return dirty_; }

private:
    void doFlush();

    std::function<void()> flush_;
    QTimer debounce_;
    QTimer maxDelay_;
    bool   dirty_ = false;
};

}  // namespace napkin
