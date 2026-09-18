#include "app/Application.h"
#include "app/Paths.h"
#include "app/SingleInstance.h"
#include "data/BufferRepository.h"
#include "data/Database.h"
#include "data/ItemRepository.h"
#include "domain/BufferService.h"
#include "media/BlobGc.h"
#include "media/BlobStore.h"
#include "media/Thumbnailer.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QMessageBox>

using namespace napkin;

int main(int argc, char** argv)
{
    Application app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("napkin"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    // Must match the .desktop file's basename, or Wayland gives the window no
    // icon and no taskbar identity. Until now this named a file that did not
    // exist anywhere.
    QGuiApplication::setDesktopFileName(QStringLiteral("napkin"));

    // Prefer the installed theme icon; fall back to the copies compiled in, so
    // a build straight out of the source tree still has an icon.
    QIcon icon = QIcon::fromTheme(QStringLiteral("napkin"));
    if (icon.isNull()) {
        for (const char* size : {"16", "32", "48", "64", "128", "256"})
            icon.addFile(QStringLiteral(":/resources/icons/%1x%1/napkin.png").arg(size));
    }
    QApplication::setWindowIcon(icon);

    Database db;
    BufferRepository buffers(db);
    ItemRepository items(db);

    try {
        paths::ensureDirs();   // the instance socket lives in here, so first
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

    SingleInstance instance;
    if (!instance.acquire())
        return 0;  // an existing Napkin was asked to raise itself

    BufferService service(db, buffers, items);
    service.purgeExpiredTrash();  // the only automatic hard delete (§6)

    BlobStore blobs(paths::blobsDir());
    Thumbnailer thumbs(paths::thumbsDir(), blobs);
    reconcileBlobs(items, blobs, paths::thumbsDir());  // collect orphans and stale thumbnails

    MainWindow window(db, buffers, items, service, blobs, thumbs);
    QObject::connect(&instance, &SingleInstance::raiseRequested,
                     &window, &MainWindow::raiseFromOtherInstance);
    window.show();
    return app.exec();
}
