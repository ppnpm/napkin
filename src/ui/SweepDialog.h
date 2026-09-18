#pragma once
#include "../domain/Buffer.h"
#include <QDialog>
#include <QList>
#include <vector>

class QLabel;
class QListWidget;

namespace napkin {

class BufferRepository;
class ItemRepository;

// The review step before a sweep.
//
// SPEC.md §6 is emphatic that Napkin never deletes a live buffer on its own.
// A sweep is therefore never automatic and never silent: it shows exactly what
// it proposes to take, lets you keep any of it, and moves the rest to the trash
// where it is still recoverable. Nothing here is irreversible.
class SweepDialog : public QDialog {
    Q_OBJECT
public:
    SweepDialog(BufferRepository& buffers, ItemRepository& items, QWidget* parent = nullptr);

    // Buffers the user confirmed. Empty if they cancelled.
    QList<BufferId> accepted() const { return accepted_; }

private:
    void populate();

    BufferRepository& buffers_;
    ItemRepository&   items_;
    QListWidget*      list_ = nullptr;
    QLabel*           summary_ = nullptr;
    QList<BufferId>   accepted_;
};

}  // namespace napkin
