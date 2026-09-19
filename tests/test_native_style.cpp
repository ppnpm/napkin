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
