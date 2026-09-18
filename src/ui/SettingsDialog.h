#pragma once
#include <QDialog>

class QComboBox;
class QSpinBox;

namespace napkin {

// The few things that are genuinely settings.
//
// Deliberately small. Napkin's whole premise is that you do not configure it,
// you throw things at it — so this holds the handful of choices that change how
// the app behaves over time, and nothing that merely changes how it looks for
// its own sake.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    // --- persisted settings, read anywhere -----------------------------------
    enum class Theme { System, Light, Dark };

    static Theme theme();
    static int olderThanDays();
    static int trashRetentionDays();

    // Applies the stored theme to the running application.
    static void applyTheme();

signals:
    void settingsChanged();

private:
    void save();

    QComboBox* theme_ = nullptr;
    QSpinBox*  older_ = nullptr;
    QSpinBox*  retention_ = nullptr;
};

}  // namespace napkin
