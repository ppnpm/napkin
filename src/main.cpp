#include "app/Paths.h"
#include "app/SingleInstance.h"
#include "data/BufferRepository.h"
#include "data/Database.h"
#include "data/ItemRepository.h"
#include "domain/BufferService.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QMessageBox>

using namespace napkin;

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("napkin"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QGuiApplication::setDesktopFileName(QStringLiteral("napkin"));

    SingleInstance instance;
    if (!instance.acquire())
        return 0;  // an existing Napkin was asked to raise itself

    Database db;
    BufferRepository buffers(db);
    ItemRepository items(db);

    try {
        paths::ensureDirs();
        db.open(paths::databaseFile());
        paths::secureDatabaseFiles();  // the files exist only now, on a first run
    } catch (const std::exception& e) {
        // SPEC.md §14: plain language, no stack traces, content accounted for.
        QMessageBox::critical(
            nullptr, QStringLiteral("Napkin cannot start"),
            QStringLiteral("Napkin could not open its storage, so it has not started.\n\n"
                           "No data has been changed.\n\n%1")
                .arg(QString::fromUtf8(e.what())));
        return 1;
    }

    BufferService service(db, buffers, items);
    service.purgeExpiredTrash();  // the only automatic hard delete (§6)

    MainWindow window(db, buffers);
    QObject::connect(&instance, &SingleInstance::raiseRequested,
                     &window, &MainWindow::raiseFromOtherInstance);
    window.show();
    return app.exec();
}
