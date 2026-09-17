#pragma once
#include <QMainWindow>

class QLabel;

namespace napkin {

class BufferRepository;
class Database;

// Phase 1 placeholder. Deliberately bare: it exists to prove the data layer
// wires up and the window opens. Phase 2 replaces it with the buffer stack.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(Database& db, BufferRepository& buffers, QWidget* parent = nullptr);

public slots:
    void raiseFromOtherInstance();

private:
    void refresh();

    Database&         db_;
    BufferRepository& buffers_;
    QLabel*           status_ = nullptr;
};

}  // namespace napkin
