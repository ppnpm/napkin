#pragma once
#include <QColor>
#include <QDialog>
#include <QString>

class QComboBox;
class QFontComboBox;
class QLabel;
class QSpinBox;

namespace napkin {

// The few things that are genuinely settings.
//
// Still deliberately small. Napkin's premise is that you do not configure it,
// you throw things at it — so nothing here is a knob for its own sake. What it
// does hold is appearance, because "what this looks like on my screen" is not
// fiddling: it is legibility, and someone who needs a larger face or a
// different typeface to read comfortably is not configuring the app, they are
// making it usable at all (SPEC.md §14).
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    // --- persisted settings, read anywhere -----------------------------------
    enum class Theme { System, Light, Dark };

    static Theme theme();
    static int olderThanDays();
    static int trashRetentionDays();

    // Empty means "whatever the desktop uses". Napkin ships no fonts and does
    // not second-guess the platform's default.
    static QString fontFamily();

    // Percent of the desktop's own size. 100 is the system font untouched.
    static int textScalePercent();

    // An invalid colour means "follow the system accent", which is the default:
    // the accent is the one piece of the platform theme worth inheriting.
    static QColor accent();

    // Applies the stored palette AND font to the running application. One
    // function, because a theme that changed the colours but not the type would
    // leave the two settings visibly out of step.
    static void applyAppearance();

signals:
    void settingsChanged();

private:
    void save();
    void updatePreview();

    QComboBox*     theme_ = nullptr;
    QFontComboBox* font_ = nullptr;
    QComboBox*     scale_ = nullptr;
    QComboBox*     accent_ = nullptr;
    QLabel*        preview_ = nullptr;
    QSpinBox*      older_ = nullptr;
    QSpinBox*      retention_ = nullptr;
};

}  // namespace napkin
