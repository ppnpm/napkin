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
    case Theme::Light: {
        QPalette p = systemPalette();
        p.setColor(QPalette::Window, QColor(239, 240, 241));
        p.setColor(QPalette::Base, QColor(252, 252, 252));
        p.setColor(QPalette::Text, QColor(35, 38, 41));
        p.setColor(QPalette::WindowText, QColor(35, 38, 41));
        QApplication::setPalette(p);
        return;
    }
    case Theme::Dark: {
        QPalette p = systemPalette();
        p.setColor(QPalette::Window, QColor(35, 38, 41));
        p.setColor(QPalette::Base, QColor(27, 30, 32));
        p.setColor(QPalette::Text, QColor(252, 252, 252));
        p.setColor(QPalette::WindowText, QColor(252, 252, 252));
        QApplication::setPalette(p);
        return;
    }
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
