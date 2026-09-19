#include "SettingsDialog.h"
#include "../domain/BufferService.h"
#include "Tokens.h"
#include "TrayIcon.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QIcon>
#include <QPixmap>
#include <QLabel>
#include <QSettings>
#include <QSpinBox>
#include <QFontComboBox>
#include <QStyleFactory>
#include <algorithm>
#include <QVBoxLayout>

namespace napkin {
namespace {

constexpr auto kTheme = "appearance/theme";
constexpr auto kFontFamily = "appearance/fontFamily";
constexpr auto kTextScale = "appearance/textScalePercent";
constexpr auto kAccent = "appearance/accent";
constexpr auto kKeepInTray = "behaviour/keepInTray";
constexpr auto kOlder = "lifecycle/olderThanDays";
constexpr auto kRetention = "lifecycle/trashRetentionDays";

// Kept for the life of the process so the "System" choice can be restored
// without asking the platform again.
QPalette& systemPalette()
{
    static QPalette saved = QApplication::palette();
    return saved;
}

// Same reasoning for the font: once Napkin has overridden it, the platform's
// own choice is no longer readable back off QApplication.
QFont& systemFont()
{
    static QFont saved = QApplication::font();
    return saved;
}

// A short, named set rather than a colour wheel. Napkin is not a theming
// engine, and every one of these is a saturated hue that stays above the 3:1
// floor against both a light and a dark surface once readableAccent() has
// hardened it.
struct NamedAccent { const char* name; QRgb rgb; };
const NamedAccent kAccents[] = {
    {QT_TRANSLATE_NOOP("SettingsDialog", "Follow the system"), 0},
    {QT_TRANSLATE_NOOP("SettingsDialog", "Blue"),   0xff3daee9},
    {QT_TRANSLATE_NOOP("SettingsDialog", "Violet"), 0xff8e6fd8},
    {QT_TRANSLATE_NOOP("SettingsDialog", "Green"),  0xff27ae60},
    {QT_TRANSLATE_NOOP("SettingsDialog", "Amber"),  0xffd88c1a},
    {QT_TRANSLATE_NOOP("SettingsDialog", "Red"),    0xffda4453},
    {QT_TRANSLATE_NOOP("SettingsDialog", "Slate"),  0xff5d7285},
};
constexpr int kAccentCount = int(sizeof(kAccents) / sizeof(kAccents[0]));

const int kScales[] = {80, 90, 100, 110, 125, 150, 175, 200};
constexpr int kScaleCount = int(sizeof(kScales) / sizeof(kScales[0]));

// A theme is every role or it is none of them.
//
// This used to patch four roles — Window, Base, Text, WindowText — onto
// whatever the platform handed us. That works only when the platform already
// agrees about light or dark. Running a dark desktop and choosing Light left
// ButtonText at white, so the menu bar, the header buttons and the shortcut
// rows on the start page were white text on a light window; choosing Dark under
// a light desktop had the mirror-image fault. Every role a widget can draw with
// is named here.
QPalette buildPalette(bool dark, const QPalette& system)
{
    QPalette p;
    auto both = [&](QPalette::ColorRole role, QColor c) { p.setColor(role, c); };

    if (!dark) {
        both(QPalette::Window,        QColor(239, 240, 241));
        both(QPalette::WindowText,    QColor( 35,  38,  41));
        both(QPalette::Base,          QColor(252, 252, 252));
        both(QPalette::AlternateBase, QColor(247, 247, 247));
        both(QPalette::Text,          QColor( 35,  38,  41));
        both(QPalette::Button,        QColor(239, 240, 241));
        both(QPalette::ButtonText,    QColor( 35,  38,  41));
        both(QPalette::BrightText,    QColor(255, 255, 255));
        both(QPalette::ToolTipBase,   QColor(247, 247, 247));
        both(QPalette::ToolTipText,   QColor( 35,  38,  41));
        both(QPalette::Light,         QColor(255, 255, 255));
        both(QPalette::Midlight,      QColor(246, 247, 248));
        both(QPalette::Mid,           QColor(196, 199, 201));
        both(QPalette::Dark,          QColor(136, 140, 143));
        both(QPalette::Shadow,        QColor( 79,  82,  85));
        both(QPalette::Link,          QColor( 41, 128, 185));
        both(QPalette::LinkVisited,   QColor(127, 140, 141));
    } else {
        both(QPalette::Window,        QColor( 35,  38,  41));
        both(QPalette::WindowText,    QColor(252, 252, 252));
        both(QPalette::Base,          QColor( 27,  30,  32));
        both(QPalette::AlternateBase, QColor( 35,  38,  41));
        both(QPalette::Text,          QColor(252, 252, 252));
        both(QPalette::Button,        QColor( 49,  54,  59));
        both(QPalette::ButtonText,    QColor(252, 252, 252));
        both(QPalette::BrightText,    QColor(255, 255, 255));
        both(QPalette::ToolTipBase,   QColor( 49,  54,  59));
        both(QPalette::ToolTipText,   QColor(252, 252, 252));
        both(QPalette::Light,         QColor( 69,  76,  82));
        both(QPalette::Midlight,      QColor( 49,  54,  59));
        both(QPalette::Mid,           QColor( 39,  43,  46));
        both(QPalette::Dark,          QColor( 24,  26,  28));
        both(QPalette::Shadow,        QColor( 16,  18,  19));
        both(QPalette::Link,          QColor( 61, 174, 233));
        both(QPalette::LinkVisited,   QColor(155,  89, 182));
    }

    // Placeholders are text and get no contrast exemption (SPEC.md §7), so this
    // is the same alpha the tokens use for the quietest readable text.
    QColor placeholder = p.color(QPalette::Text);
    placeholder.setAlpha(161);
    both(QPalette::PlaceholderText, placeholder);

    // The accent is the user's, not ours — either the one they picked here or
    // the platform's, and a saturated accent reads on either background. Its
    // partner is chosen below rather than inherited, because a light-theme
    // HighlightedText carried into a dark theme is how selected text disappears.
    const QColor chosen = SettingsDialog::accent();
    const QColor accent = chosen.isValid() ? chosen : system.color(QPalette::Highlight);
    both(QPalette::Highlight, accent);
    both(QPalette::HighlightedText, tokens::textOn(accent));

    // Disabled is a group, not a role: without it Qt keeps the enabled colour
    // and nothing looks disabled.
    const QColor greyed = dark ? QColor(137, 142, 147) : QColor(136, 140, 143);
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text,
                                     QPalette::ButtonText, QPalette::HighlightedText})
        p.setColor(QPalette::Disabled, role, greyed);
    p.setColor(QPalette::Disabled, QPalette::Highlight,
               dark ? QColor(49, 54, 59) : QColor(219, 220, 221));

    return p;
}

}  // namespace

SettingsDialog::Theme SettingsDialog::theme()
{
    return Theme(QSettings().value(kTheme, int(Theme::System)).toInt());
}

QString SettingsDialog::fontFamily()
{
    return QSettings().value(kFontFamily, QString()).toString();
}

int SettingsDialog::textScalePercent()
{
    const int stored = QSettings().value(kTextScale, 100).toInt();
    return std::clamp(stored, 50, 300);   // a corrupt setting must not be unreadable
}

QColor SettingsDialog::accent()
{
    const QString stored = QSettings().value(kAccent, QString()).toString();
    if (stored.isEmpty()) return {};
    const QColor colour(stored);
    return colour.isValid() ? colour : QColor();
}

bool SettingsDialog::keepInTray()
{
    // A desktop with no tray cannot honour this however it is stored, and
    // answering true there would let the window close to somewhere that does
    // not exist.
    return QSettings().value(kKeepInTray, false).toBool()
           && TrayIcon::availableOnThisDesktop();
}

int SettingsDialog::olderThanDays()
{
    return QSettings().value(kOlder, kOlderThresholdDays).toInt();
}

int SettingsDialog::trashRetentionDays()
{
    return QSettings().value(kRetention, kTrashRetentionDays).toInt();
}

void SettingsDialog::applyAppearance()
{
    // Capture the platform's own choices before overriding either of them.
    systemPalette();
    systemFont();

    switch (theme()) {
    case Theme::System:
        // Even here the accent may be the user's, so the system palette is
        // rebuilt rather than restored verbatim when one has been chosen.
        if (accent().isValid()) {
            QPalette p = systemPalette();
            p.setColor(QPalette::Highlight, accent());
            p.setColor(QPalette::HighlightedText, tokens::textOn(accent()));
            QApplication::setPalette(p);
        } else {
            QApplication::setPalette(systemPalette());
        }
        break;
    case Theme::Light:
        QApplication::setPalette(buildPalette(false, systemPalette()));
        break;
    case Theme::Dark:
        QApplication::setPalette(buildPalette(true, systemPalette()));
        break;
    }

    // Scaled from the platform's size, never from the current one: scaling the
    // already-scaled font would compound every time this ran.
    QFont font = systemFont();
    const QString family = fontFamily();
    if (!family.isEmpty()) font.setFamilies({family});
    font.setPointSizeF(std::max(5.0, systemFont().pointSizeF() * textScalePercent() / 100.0));
    QApplication::setFont(font);
}

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Settings"));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(14);

    // --- appearance ---------------------------------------------------------
    auto* look = new QGroupBox(tr("Appearance"));
    auto* lookForm = new QFormLayout(look);
    lookForm->setSpacing(10);

    theme_ = new QComboBox;
    theme_->addItems({tr("Follow the system"), tr("Light"), tr("Dark")});
    theme_->setCurrentIndex(int(theme()));
    lookForm->addRow(tr("Theme"), theme_);

    accent_ = new QComboBox;
    for (int i = 0; i < kAccentCount; ++i) {
        accent_->addItem(tr(kAccents[i].name));
        if (i > 0) {
            // A swatch, because a colour named in words is a colour you have to
            // imagine. §14: the name carries the meaning, the swatch only helps.
            QPixmap swatch(14, 14);
            swatch.fill(QColor::fromRgba(kAccents[i].rgb));
            accent_->setItemIcon(i, QIcon(swatch));
        }
    }
    const QColor current = accent();
    accent_->setCurrentIndex(0);
    for (int i = 1; i < kAccentCount; ++i)
        if (current.isValid() && QColor::fromRgba(kAccents[i].rgb) == current)
            accent_->setCurrentIndex(i);
    lookForm->addRow(tr("Accent"), accent_);

    font_ = new QFontComboBox;
    font_->setEditable(false);
    // Napkin ships no fonts and does not second-guess the platform, so the
    // first entry is the desktop's own choice rather than a named family.
    font_->insertItem(0, tr("System default"));
    const QString family = fontFamily();
    if (family.isEmpty()) font_->setCurrentIndex(0);
    else                  font_->setCurrentFont(QFont(family));
    lookForm->addRow(tr("Typeface"), font_);

    scale_ = new QComboBox;
    for (int i = 0; i < kScaleCount; ++i) {
        scale_->addItem(kScales[i] == 100 ? tr("100%  (system size)")
                                          : QStringLiteral("%1%").arg(kScales[i]),
                        kScales[i]);
        if (kScales[i] == textScalePercent()) scale_->setCurrentIndex(i);
    }
    lookForm->addRow(tr("Text size"), scale_);

    preview_ = new QLabel;
    preview_->setFrameShape(QFrame::StyledPanel);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumHeight(56);
    preview_->setWordWrap(true);
    lookForm->addRow(tr("Preview"), preview_);

    // Live, so the choice is made by looking rather than by guessing and
    // reopening the dialog.
    connect(font_, &QFontComboBox::currentFontChanged, this, &SettingsDialog::updatePreview);
    connect(scale_, &QComboBox::currentIndexChanged, this, &SettingsDialog::updatePreview);
    connect(accent_, &QComboBox::currentIndexChanged, this, &SettingsDialog::updatePreview);
    updatePreview();

    layout->addWidget(look);

    // --- lifecycle ----------------------------------------------------------
    auto* life = new QGroupBox(tr("Lifecycle"));
    auto* lifeForm = new QFormLayout(life);
    lifeForm->setSpacing(10);

    older_ = new QSpinBox;
    older_->setRange(1, 3650);
    older_->setSuffix(tr(" days"));
    older_->setValue(olderThanDays());
    lifeForm->addRow(tr("Move to “Older” after"), older_);

    retention_ = new QSpinBox;
    retention_->setRange(1, 3650);
    retention_->setSuffix(tr(" days"));
    retention_->setValue(trashRetentionDays());
    lifeForm->addRow(tr("Keep trash for"), retention_);

    tray_ = new QCheckBox(tr("Keep Napkin running in the system tray"));
    tray_->setChecked(QSettings().value(kKeepInTray, false).toBool());
    if (!TrayIcon::availableOnThisDesktop()) {
        tray_->setEnabled(false);
        tray_->setToolTip(tr("This desktop has no system tray."));
    }
    lifeForm->addRow(QString(), tray_);

    auto* trayNote = new QLabel(
        tr("Closing the window then hides Napkin instead of quitting it, so it is "
           "already running the next time you have something to put somewhere."));
    trayNote->setWordWrap(true);
    lifeForm->addRow(trayNote);

    auto* note = new QLabel(
        tr("Nothing is deleted on your behalf. “Older” only changes where "
           "a napkin sits in the list; the trash is the only thing that empties, "
           "and only what you have already deleted."));
    note->setWordWrap(true);
    lifeForm->addRow(note);

    layout->addWidget(life);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { save(); accept(); });
}

// Shows the chosen face at the chosen size, on the chosen accent. The dialog
// itself deliberately does not restyle as you choose: a dialog that reflowed
// under the pointer would move the control you were using.
void SettingsDialog::updatePreview()
{
    QFont sample = systemFont();
    if (font_->currentIndex() > 0) sample.setFamilies({font_->currentFont().family()});
    const int percent = scale_->currentData().toInt();
    sample.setPointSizeF(std::max(5.0, systemFont().pointSizeF() * percent / 100.0));
    preview_->setFont(sample);
    preview_->setText(tr("The quick brown fox\n0123456789"));

    const int index = accent_->currentIndex();
    QPalette p = preview_->palette();
    if (index > 0) {
        QPalette probe = QApplication::palette();
        probe.setColor(QPalette::Highlight, QColor::fromRgba(kAccents[index].rgb));
        p.setColor(preview_->foregroundRole(), tokens::readableAccent(probe, 1.0));
    } else {
        p.setColor(preview_->foregroundRole(),
                   tokens::text(QApplication::palette(), tokens::kTextPrimary));
    }
    preview_->setPalette(p);
}

void SettingsDialog::save()
{
    QSettings settings;
    settings.setValue(kTheme, theme_->currentIndex());
    settings.setValue(kFontFamily, font_->currentIndex() > 0
                                       ? font_->currentFont().family()
                                       : QString());
    settings.setValue(kTextScale, scale_->currentData().toInt());
    const int accentIndex = accent_->currentIndex();
    settings.setValue(kAccent, accentIndex > 0
                                   ? QColor::fromRgba(kAccents[accentIndex].rgb).name()
                                   : QString());
    settings.setValue(kKeepInTray, tray_->isChecked());
    settings.setValue(kOlder, older_->value());
    settings.setValue(kRetention, retention_->value());
    applyAppearance();
    emit settingsChanged();
}

}  // namespace napkin
