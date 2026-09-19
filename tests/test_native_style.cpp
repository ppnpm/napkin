#include "../src/ui/SettingsDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QStyleHints>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

using namespace napkin;

// Dark means the controls too, not only the window behind them.
//
// Reported from the first Windows build: in dark mode the window went dark and
// the buttons and dropdowns stayed light. Windows' native style draws controls
// through the system theme engine, which ignores the application's palette, so
// every palette check in test_theme passed while the controls were plainly
// wrong. Those tests run on the offscreen platform, whose default style honours
// the palette; this one runs on the real platform on Windows (see CMakeLists),
// with the style Napkin would actually get there, and looks at the pixels.
//
// NAPKIN_RENDER_DIR, if set, is where each render is saved, so a failure on a
// machine nobody can sit at still leaves something to look at.
namespace {

qreal luminance(const QColor& c)
{
    auto channel = [](int v) {
        const qreal s = v / 255.0;
        return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c.red()) + 0.7152 * channel(c.green()) + 0.0722 * channel(c.blue());
}

// The median over the whole control: the face dominates, and the label, the
// border and the arrow are too few pixels to move it.
qreal medianLuminance(const QImage& img)
{
    std::vector<qreal> values;
    values.reserve(size_t(img.width()) * img.height());
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            values.push_back(luminance(img.pixelColor(x, y)));
    if (values.empty()) return -1;
    std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
    return values[values.size() / 2];
}

struct Control {
    const char* name;
    std::unique_ptr<QWidget> widget;
};

std::vector<Control> controls()
{
    std::vector<Control> out;
    out.push_back({"button", std::make_unique<QPushButton>(QStringLiteral("Clean up"))});
    auto combo = std::make_unique<QComboBox>();
    combo->addItems({QStringLiteral("System"), QStringLiteral("Light"), QStringLiteral("Dark")});
    out.push_back({"dropdown", std::move(combo)});
    out.push_back({"textfield", std::make_unique<QLineEdit>()});
    out.push_back({"spinbox", std::make_unique<QSpinBox>()});
    auto bar = std::make_unique<QScrollBar>(Qt::Horizontal);
    bar->setRange(0, 100);
    bar->setPageStep(20);
    out.push_back({"scrollbar", std::move(bar)});
    return out;
}

QImage render(QWidget* w)
{
    w->ensurePolished();
    w->resize(w->sizeHint().expandedTo(QSize(160, 20)));
    return w->grab().toImage();
}

void setTheme(SettingsDialog::Theme t)
{
    QSettings().setValue(QStringLiteral("appearance/theme"), int(t));
    SettingsDialog::applyAppearance();
}

// Every control's face must be on the requested side of the scale.
void verifyControls(const QString& label, bool dark)
{
    const QString dir = qEnvironmentVariable("NAPKIN_RENDER_DIR");
    if (!dir.isEmpty()) QDir().mkpath(dir);
    const QString style = QApplication::style()->name();

    QStringList wrong;
    for (auto& c : controls()) {
        const QImage img = render(c.widget.get());
        const qreal lum = medianLuminance(img);
        if (!dir.isEmpty())
            img.save(QStringLiteral("%1/%2-%3-%4.png").arg(dir, label, style, QLatin1String(c.name)));
        // (60,60,60) is 0.045 and (200,200,200) is 0.58: the thresholds sit
        // well clear of both, so this is about which side, not about shades.
        if (dark ? lum > 0.25 : lum < 0.35)
            wrong << QStringLiteral("%1=%2").arg(QLatin1String(c.name)).arg(lum, 0, 'f', 3);
    }
    QVERIFY2(wrong.isEmpty(),
             qPrintable(QStringLiteral("%1 theme, style %2: %3 controls drawn %4 (%5)")
                            .arg(label, style)
                            .arg(wrong.size())
                            .arg(dark ? QStringLiteral("light") : QStringLiteral("dark"),
                                 wrong.join(u' '))));
}

#ifdef Q_OS_WIN
// What Windows' own Settings app does when the user picks light or dark: write
// the per-user value, then tell every top-level window the colour set changed.
void setWindowsDarkMode(bool dark)
{
    QSettings personalize(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat);
    personalize.setValue(QStringLiteral("AppsUseLightTheme"), dark ? 0 : 1);
    personalize.sync();
    DWORD_PTR ignored = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        reinterpret_cast<LPARAM>(L"ImmersiveColorSet"),
                        SMTO_ABORTIFHUNG, 2000, &ignored);
}
#endif

}  // namespace

class TestNativeStyle : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("napkin-test-native-style"));
        QCoreApplication::setApplicationName(QStringLiteral("napkin-test-native-style"));
        QSettings().clear();
        qInfo("platform %s, style %s, system scheme %s",
              qPrintable(QGuiApplication::platformName()),
              qPrintable(QApplication::style()->name()),
              QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? "dark"
                                                                                     : "light/unknown");
    }

    void cleanupTestCase() { QSettings().clear(); }

    void darkThemeDarkensTheControls()
    {
        setTheme(SettingsDialog::Theme::Dark);
        verifyControls(QStringLiteral("dark"), true);
    }

    void lightThemeKeepsTheControlsLight()
    {
        setTheme(SettingsDialog::Theme::Light);
        verifyControls(QStringLiteral("light"), false);
    }

    // What the report was actually about: Windows set to dark, Napkin left on
    // System. Only testable where the platform says it is dark.
    void systemThemeFollowsADarkDesktopIntoTheControls()
    {
        if (QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Dark)
            QSKIP("the platform is not in dark mode here");
        setTheme(SettingsDialog::Theme::System);
        verifyControls(QStringLiteral("system-dark"), true);
    }

    // Reported after 0.1.3: flipping Windows between light and dark while
    // Napkin was open changed nothing until a restart. Flip it both ways under
    // a running Napkin and look at the controls each time.
    void aDesktopThemeChangeIsFollowedWithoutARestart()
    {
#ifndef Q_OS_WIN
        QSKIP("drives the Windows setting itself; the desktop here cannot be flipped from a test");
#else
        SettingsDialog::followSystemChanges();
        setTheme(SettingsDialog::Theme::System);
        // WM_SETTINGCHANGE is broadcast to top-level windows, so the process
        // needs one, exactly as the real app has.
        QWidget window;
        window.resize(200, 100);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        const bool startedDark =
            QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        for (const bool dark : {!startedDark, startedDark}) {
            setWindowsDarkMode(dark);
            const auto want = dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light;
            QTRY_VERIFY2_WITH_TIMEOUT(QGuiApplication::styleHints()->colorScheme() == want,
                                      "Qt never saw the desktop change", 10000);
            // The refresh is queued behind the change; let it land.
            QTest::qWait(300);
            verifyControls(dark ? QStringLiteral("flipped-dark") : QStringLiteral("flipped-light"),
                           dark);
            if (QTest::currentTestFailed()) break;
        }
        setWindowsDarkMode(startedDark);   // leave the machine as it was found
#endif
    }

    // Napkin's own choice outranks the desktop's: a theme change arriving while
    // Dark is chosen must refresh from the platform and still end up Dark.
    void aDesktopThemeChangeDoesNotOverrideAnExplicitChoice()
    {
        SettingsDialog::followSystemChanges();
        setTheme(SettingsDialog::Theme::Dark);
        QWidget w;
        QEvent change(QEvent::ThemeChange);
        QCoreApplication::sendEvent(&w, &change);
        QTest::qWait(100);
        verifyControls(QStringLiteral("dark-after-themechange"), true);
    }

    // Switching back must restore what the platform chose, not leave behind
    // whatever the dark theme needed.
    void returningToLightRestoresThePlatformStyle()
    {
        setTheme(SettingsDialog::Theme::Light);
        const QString before = QApplication::style()->name();
        setTheme(SettingsDialog::Theme::Dark);
        setTheme(SettingsDialog::Theme::Light);
        QCOMPARE(QApplication::style()->name(), before);
    }
};

QTEST_MAIN(TestNativeStyle)
#include "test_native_style.moc"
