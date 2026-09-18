#include "../src/ui/SettingsDialog.h"
#include "../src/ui/Tokens.h"
#include "GuiFixture.h"

#include <QLabel>
#include <QSettings>
#include <QtTest>
#include <cmath>

using namespace napkin;

namespace {

qreal relativeLuminance(const QColor& c)
{
    auto channel = [](int v) {
        const qreal s = v / 255.0;
        return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c.red()) + 0.7152 * channel(c.green()) + 0.0722 * channel(c.blue());
}

// Alpha is how every text token in this codebase expresses emphasis, so a
// contrast check that ignores it is checking a colour nothing ever paints.
QColor composite(const QColor& fg, const QColor& bg)
{
    const qreal a = fg.alphaF();
    return QColor::fromRgbF(fg.redF() * a + bg.redF() * (1 - a),
                            fg.greenF() * a + bg.greenF() * (1 - a),
                            fg.blueF() * a + bg.blueF() * (1 - a));
}

qreal contrast(const QColor& fg, const QColor& bg)
{
    const qreal a = relativeLuminance(composite(fg, bg));
    const qreal b = relativeLuminance(bg);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

// A platform theme that disagrees with whatever the user then picks. This is
// the condition the old applyTheme() got wrong: it patched four roles onto the
// platform's palette and left the other sixteen pointing the wrong way.
QPalette darkPlatformPalette()
{
    QPalette p = QApplication::palette();
    p.setColor(QPalette::Window,        QColor( 35,  38,  41));
    p.setColor(QPalette::WindowText,    QColor(252, 252, 252));
    p.setColor(QPalette::Base,          QColor( 27,  30,  32));
    p.setColor(QPalette::Text,          QColor(252, 252, 252));
    p.setColor(QPalette::Button,        QColor( 49,  54,  59));
    p.setColor(QPalette::ButtonText,    QColor(252, 252, 252));
    p.setColor(QPalette::ToolTipBase,   QColor( 49,  54,  59));
    p.setColor(QPalette::ToolTipText,   QColor(252, 252, 252));
    p.setColor(QPalette::AlternateBase, QColor( 35,  38,  41));
    return p;
}

}  // namespace

// A theme is every role or it is none of them.
//
// Running a dark desktop and choosing Light gave white text on a light window
// across the menu bar, the header buttons and the start page's shortcut rows.
// Two independent faults produced it, and each of these tests fails on one of
// them alone.
class TestTheme : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("napkin-test-theme"));
        QCoreApplication::setApplicationName(QStringLiteral("napkin-test-theme"));
        QSettings().clear();

        // Captured before anything else calls applyTheme(), so the process
        // believes it is running under a dark platform theme.
        QApplication::setPalette(darkPlatformPalette());
        QSettings().setValue(QStringLiteral("appearance/theme"),
                             int(SettingsDialog::Theme::System));
        SettingsDialog::applyTheme();
    }

    void cleanupTestCase() { QSettings().clear(); }

    void everyPairedRoleIsReadableInBothThemes()
    {
        const QList<QPair<QPalette::ColorRole, QPalette::ColorRole>> pairs = {
            {QPalette::WindowText, QPalette::Window},
            {QPalette::Text, QPalette::Base},
            {QPalette::Text, QPalette::AlternateBase},
            {QPalette::ButtonText, QPalette::Button},
            {QPalette::ToolTipText, QPalette::ToolTipBase},
            {QPalette::PlaceholderText, QPalette::Base},
        };

        for (auto theme : {SettingsDialog::Theme::Light, SettingsDialog::Theme::Dark}) {
            QSettings().setValue(QStringLiteral("appearance/theme"), int(theme));
            SettingsDialog::applyTheme();
            const QPalette p = QApplication::palette();

            for (const auto& [fg, bg] : pairs) {
                const qreal ratio = contrast(p.color(fg), p.color(bg));
                QVERIFY2(ratio >= 4.5,
                         qPrintable(QStringLiteral("theme %1: role %2 on %3 is %4:1")
                                        .arg(int(theme)).arg(int(fg)).arg(int(bg))
                                        .arg(ratio, 0, 'f', 2)));
            }

            // Selected text has to survive on the accent it is drawn over, and
            // the accent is the one colour still inherited from the platform.
            QVERIFY(contrast(p.color(QPalette::HighlightedText),
                             p.color(QPalette::Highlight)) >= 3.0);
        }
    }

    void theStartPageIsReadableWhenTheThemeOpposesThePlatform()
    {
        QSettings().setValue(QStringLiteral("appearance/theme"),
                             int(SettingsDialog::Theme::Light));
        SettingsDialog::applyTheme();

        GuiFixture f;   // no buffers, so the start page is what is showing
        const QColor window = QApplication::palette().color(QPalette::Window);

        const auto labels = f.window.findChildren<QLabel*>();
        QVERIFY(labels.size() >= 10);   // the mark, the name, the tagline, the rows

        for (QLabel* label : labels) {
            if (label->text().isEmpty()) continue;   // the logo carries a pixmap
            // The colour the label actually paints with. Reading WindowText
            // here instead is precisely the bug: the shortcut rows live inside
            // buttons and draw with ButtonText, so tinting WindowText on them
            // changed nothing at all.
            const QColor drawn = label->palette().color(label->foregroundRole());
            const qreal ratio = contrast(drawn, window);
            QVERIFY2(ratio >= 3.0,
                     qPrintable(QStringLiteral("“%1” is %2:1 against the window")
                                    .arg(label->text().left(30)).arg(ratio, 0, 'f', 2)));
        }
    }

    void theShortcutRowsKeepTheirEmphasisInsideAButton()
    {
        // The rows are clickable, so their labels live inside a QPushButton and
        // inherit ButtonText as their foreground role. Tinting WindowText on
        // them — which is what applyPalette() used to do — is a no-op: both
        // labels then paint in the same colour and the key stops standing out
        // from its description. Readability alone will not catch this, because
        // ButtonText is now a perfectly readable colour.
        QSettings().setValue(QStringLiteral("appearance/theme"),
                             int(SettingsDialog::Theme::Light));
        SettingsDialog::applyTheme();

        GuiFixture f;
        auto* key = f.window.findChild<QLabel*>(QStringLiteral("shortcutKey"));
        auto* what = f.window.findChild<QLabel*>(QStringLiteral("shortcutWhat"));
        QVERIFY(key && what);

        const QPalette app = QApplication::palette();
        QCOMPARE(key->palette().color(key->foregroundRole()),
                 tokens::text(app, tokens::kTextPrimary));
        QCOMPARE(what->palette().color(what->foregroundRole()),
                 tokens::text(app, tokens::kTextSecondary));
    }
};

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
