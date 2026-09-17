#include "Autosave.h"

namespace napkin {

Autosave::Autosave(QObject* parent) : QObject(parent)
{
    debounce_.setSingleShot(true);
    debounce_.setInterval(kDebounceMs);
    maxDelay_.setSingleShot(true);
    maxDelay_.setInterval(kMaxDelayMs);

    connect(&debounce_, &QTimer::timeout, this, &Autosave::doFlush);
    connect(&maxDelay_, &QTimer::timeout, this, &Autosave::doFlush);
}

void Autosave::noteChange()
{
    dirty_ = true;
    debounce_.start();                              // restarts on every keystroke
    if (!maxDelay_.isActive()) maxDelay_.start();   // but this one does not
}

void Autosave::flushNow()
{
    if (dirty_) doFlush();
}

void Autosave::doFlush()
{
    debounce_.stop();
    maxDelay_.stop();
    if (!dirty_) return;
    dirty_ = false;
    if (flush_) flush_();
}

}  // namespace napkin
