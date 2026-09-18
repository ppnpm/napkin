#include "Application.h"

#include <QMessageBox>
#include <QTimer>
#include <exception>

namespace napkin {

Application::Application(int& argc, char** argv) : QApplication(argc, argv) {}

bool Application::notify(QObject* receiver, QEvent* event)
{
    try {
        return QApplication::notify(receiver, event);
    } catch (const std::exception& e) {
        report(QString::fromUtf8(e.what()));
    } catch (...) {
        report(tr("An unknown error occurred."));
    }
    return false;
}

void Application::report(const QString& detail)
{
    // Reporting opens a dialog, which delivers events, which can throw again.
    // Swallow anything that arrives while a report is already on screen.
    if (reporting_) return;
    reporting_ = true;

    // Deferred so the dialog does not open inside the event that failed: a
    // modal loop there would re-enter whatever was half-finished.
    QTimer::singleShot(0, this, [this, detail] {
        QMessageBox::warning(
            nullptr, tr("Napkin hit a problem"),
            tr("Something went wrong and Napkin has stopped what it was doing.\n\n"
               "Your buffers have not been changed. If this keeps happening, the "
               "storage folder may be full or unwritable.\n\n%1").arg(detail));
        reporting_ = false;
    });
}

}  // namespace napkin
