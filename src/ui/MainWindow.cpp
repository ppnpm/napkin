#include "MainWindow.h"
#include "../data/BufferRepository.h"
#include "../data/Database.h"
#include "../app/Paths.h"
#include "../data/Migrations.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace napkin {

MainWindow::MainWindow(Database& db, BufferRepository& buffers, QWidget* parent)
    : QMainWindow(parent), db_(db), buffers_(buffers)
{
    auto* central = new QWidget;
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(28, 28, 28, 28);

    auto* title = new QLabel(QStringLiteral("Napkin"));
    QFont f = title->font();
    f.setPointSize(f.pointSize() + 8);
    title->setFont(f);
    layout->addWidget(title);

    status_ = new QLabel;
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    layout->addStretch();

    setCentralWidget(central);
    setWindowTitle(QStringLiteral("Napkin"));
    resize(640, 420);
    refresh();
}

void MainWindow::refresh()
{
    status_->setText(QStringLiteral(
        "<p>Phase 1 — data layer online.</p>"
        "<p><b>schema:</b> v%1<br>"
        "<b>data:</b> %2<br>"
        "<b>buffers:</b> %3 live · %4 kept · %5 in trash</p>"
        "<p style='color:gray'>Phase 2 replaces this window with the buffer stack.</p>")
        .arg(db_.userVersion())
        .arg(paths::dataDir())
        .arg(buffers_.countLive())
        .arg(buffers_.countKept())
        .arg(buffers_.countTrash()));
}

void MainWindow::raiseFromOtherInstance()
{
    showNormal();
    raise();
    activateWindow();
    refresh();
}

}  // namespace napkin
