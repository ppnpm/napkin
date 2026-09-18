#pragma once
#include <QObject>

class QSystemTrayIcon;
class QMenu;

namespace napkin {

// Keeps Napkin reachable when its window is closed.
//
// Off by default. Napkin's premise is that it is somewhere to throw things, not
// a resident service — but "somewhere to throw things" only works if it is
// already running when you have something to throw, which is what a tray icon
// and, later, a global hotkey are for.
//
// Whether a tray exists at all is a property of the desktop, not of Napkin. On
// a session with no StatusNotifier host the icon silently never appears, so
// this class reports availability rather than assuming it: closing the window
// to an icon that is not there would leave the user with no way back to the
// application at all.
class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

    static bool availableOnThisDesktop();

    void setVisible(bool visible);
    bool isShowing() const;

signals:
    void showRequested();
    void newBufferRequested();
    void pasteRequested();
    void quitRequested();

private:
    QSystemTrayIcon* icon_ = nullptr;
    QMenu*           menu_ = nullptr;
};

}  // namespace napkin
