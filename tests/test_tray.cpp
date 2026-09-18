#include "../src/ui/SettingsDialog.h"
#include "../src/ui/TrayIcon.h"
#include "GuiFixture.h"

#include <QCloseEvent>
#include <QSettings>
#include <QtTest>

using namespace napkin;

// Keeping Napkin in the tray is the setting that makes "somewhere to throw
// things" work — it can only catch what is thrown at it if it is running. It is
// also the setting that can strand a user, so the tests are mostly about that.
class TestTray : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("napkin-test-tray"));
        QCoreApplication::setApplicationName(QStringLiteral("napkin-test-tray"));
        QSettings().clear();
    }

    void cleanupTestCase() { QSettings().clear(); }

    void offByDefault()
    {
        QSettings().clear();
        QVERIFY(!SettingsDialog::keepInTray());
    }

    void theSettingIsRefusedOnADesktopWithNoTray()
    {
        // The stored value is only half the answer. A session with no
        // StatusNotifier host cannot honour it however it is stored, and
        // answering true there is what would let the window close to a place
        // that does not exist. The offscreen platform has no tray, so this is
        // that case rather than a simulation of it.
        QSettings().setValue(QStringLiteral("behaviour/keepInTray"), true);

        if (TrayIcon::availableOnThisDesktop())
            QSKIP("this session has a tray, so the refusal cannot be observed here");

        QCOMPARE(SettingsDialog::keepInTray(), false);
        QSettings().remove(QStringLiteral("behaviour/keepInTray"));
    }

    void closingStillClosesWhenThereIsNoTrayToHideTo()
    {
        // The trap this guards: setting on, tray absent, window closes to
        // nothing and the application is unreachable with its data still in it.
        QSettings().setValue(QStringLiteral("behaviour/keepInTray"), true);

        GuiFixture f;
        f.seed("something worth not losing");

        QCloseEvent event;
        QCoreApplication::sendEvent(&f.window, &event);

        if (TrayIcon::availableOnThisDesktop())
            QSKIP("this session has a tray; the strand case cannot arise here");

        QVERIFY2(event.isAccepted(),
                 "the window refused to close with no tray to hide into");
        QSettings().remove(QStringLiteral("behaviour/keepInTray"));
    }

};

QTEST_MAIN(TestTray)
#include "test_tray.moc"
