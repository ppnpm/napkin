#include "SettingsDialog.h"
#include "../domain/BufferService.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSettings>
#include <QSpinBox>
#include <QStyleFactory>
#include <QVBoxLayout>

namespace napkin {
namespace {

constexpr auto kTheme = "appearance/theme";
constexpr auto kOlder = "lifecycle/olderThanDays";
constexpr auto kRetention = "lifecycle/trashRetentionDays";

// Kept for the life of the process so the "System" choice can be restored
// without asking the platform again.
QPalette& systemPalette()
{
    static QPalette saved = QApplication::palette();
    return saved;
}

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

    // The accent is the user's, not ours — it is the one part of the platform
    // theme worth keeping, and a saturated accent reads on either background.
    // Its partner is chosen here rather than inherited, because a light-theme
    // HighlightedText carried into a dark theme is how selected text disappears.
    const QColor accent = system.color(QPalette::Highlight);
    both(QPalette::Highlight, accent);
    both(QPalette::HighlightedText,
         accent.lightness() > 140 ? QColor(35, 38, 41) : QColor(252, 252, 252));

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

int SettingsDialog::olderThanDays()
{
    return QSettings().value(kOlder, kOlderThresholdDays).toInt();
}

int SettingsDialog::trashRetentionDays()
{
    return QSettings().value(kRetention, kTrashRetentionDays).toInt();
}

void SettingsDialog::applyTheme()
{
    systemPalette();   // capture the platform's palette before overriding it

    switch (theme()) {
    case Theme::System:
        QApplication::setPalette(systemPalette());
        return;
    case Theme::Light:
        QApplication::setPalette(buildPalette(false, systemPalette()));
        return;
    case Theme::Dark:
        QApplication::setPalette(buildPalette(true, systemPalette()));
        return;
    }
}

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Settings"));

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    form->setSpacing(10);

    theme_ = new QComboBox;
    theme_->addItems({tr("Follow the system"), tr("Light"), tr("Dark")});
    theme_->setCurrentIndex(int(theme()));
    form->addRow(tr("Appearance"), theme_);

    older_ = new QSpinBox;
    older_->setRange(1, 3650);
    older_->setSuffix(tr(" days"));
    older_->setValue(olderThanDays());
    form->addRow(tr("Move to “Older” after"), older_);

    retention_ = new QSpinBox;
    retention_->setRange(1, 3650);
    retention_->setSuffix(tr(" days"));
    retention_->setValue(trashRetentionDays());
    form->addRow(tr("Keep trash for"), retention_);

    layout->addLayout(form);

    auto* note = new QLabel(
        tr("Napkin never deletes a buffer on its own. “Older” only changes where "
           "a buffer sits in the list; the trash is the only thing that empties, "
           "and only what you have already deleted."));
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { save(); accept(); });
}

void SettingsDialog::save()
{
    QSettings settings;
    settings.setValue(kTheme, theme_->currentIndex());
    settings.setValue(kOlder, older_->value());
    settings.setValue(kRetention, retention_->value());
    applyTheme();
    emit settingsChanged();
}

}  // namespace napkin
