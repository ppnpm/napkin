#include "TrayIcon.h"

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

namespace napkin {

TrayIcon::TrayIcon(QObject* parent) : QObject(parent)
{
    if (!availableOnThisDesktop()) return;

    // The installed theme icon if there is one, otherwise the copies compiled
    // into the binary — the same fallback the window icon uses, so a build run
    // out of the source tree still has a tray icon rather than a blank square.
    QIcon art = QIcon::fromTheme(QStringLiteral("io.github.sudomonas.Napkin"));
    if (art.isNull())
        for (const char* size : {"22", "32", "48"})
            art.addFile(QStringLiteral(":/resources/icons/%1x%1/io.github.sudomonas.Napkin.png")
                            .arg(size));

    menu_ = new QMenu;
    auto* show = menu_->addAction(tr("Show Napkin"));
    connect(show, &QAction::triggered, this, &TrayIcon::showRequested);
    menu_->addSeparator();
    auto* newBuffer = menu_->addAction(tr("New buffer"));
    connect(newBuffer, &QAction::triggered, this, &TrayIcon::newBufferRequested);
    auto* paste = menu_->addAction(tr("Paste into a new buffer"));
    connect(paste, &QAction::triggered, this, &TrayIcon::pasteRequested);
    menu_->addSeparator();
    auto* quit = menu_->addAction(tr("Quit Napkin"));
    connect(quit, &QAction::triggered, this, &TrayIcon::quitRequested);

    icon_ = new QSystemTrayIcon(art, this);
    icon_->setToolTip(tr("Napkin — a persistent scratch surface"));
    icon_->setContextMenu(menu_);

    // A left click is "show me", which is what the icon is for. The menu is for
    // everything else.
    connect(icon_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick)
                    emit showRequested();
            });
}

TrayIcon::~TrayIcon()
{
    // The menu has no parent — it cannot be parented to the icon, which is a
    // QObject and not a widget — so it is owned here explicitly.
    delete menu_;
}

bool TrayIcon::availableOnThisDesktop()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayIcon::setVisible(bool visible)
{
    if (icon_) icon_->setVisible(visible);
}

bool TrayIcon::isShowing() const
{
    return icon_ && icon_->isVisible();
}

}  // namespace napkin
