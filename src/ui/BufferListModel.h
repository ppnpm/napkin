#pragma once
#include "../domain/Buffer.h"
#include "../domain/Preview.h"
#include <QAbstractListModel>
#include <QHash>
#include <vector>

namespace napkin {

class BufferRepository;
class ItemRepository;

// Holds buffer *metadata* for every live buffer — roughly 48 bytes a row, so
// 5000 buffers is a quarter of a megabyte — and fetches previews lazily, per
// visible row, into a cache. That is what keeps §12's promise without loading
// every item in the database to draw a list.
class BufferListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PrimaryRole,
        SecondaryRole,
        ItemCountRole,
        HasImageRole,
        ModifiedAtRole,
        PinnedRole,
        KeptRole,
        IsDraftRole,
        IsExpandedRole,
        SectionFirstRole,   // this row starts a section
        SectionNameRole,    // "PINNED" / "RECENT"
    };

    BufferListModel(BufferRepository& buffers, ItemRepository& items, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void reload();

    BufferId idAt(int row) const;
    int rowForId(BufferId id) const;

    // --- expansion ----------------------------------------------------------
    // Reloading is deferred while a row is expanded. Autosave bumps
    // modified_at on every flush, and re-sorting on that would make the card
    // you are typing into jump to the top of the list (SPEC.md §7).
    int  expandedRow() const { return expandedRow_; }
    void setExpandedRow(int row);

    // --- draft --------------------------------------------------------------
    // A draft row exists only in the model until it has content (invariant 5).
    int  insertDraftRow();
    int  draftRow() const;
    bool hasDraft() const { return draftRow() >= 0; }
    void setDraftPreview(const BufferPreview& preview);
    void promoteDraft(BufferId newId);
    void removeDraftRow();

    void invalidatePreview(BufferId id);
    void refreshTimestamps();

signals:
    void countChanged(int liveCount);

private:
    BufferPreview previewFor(BufferId id) const;
    void emitAllChanged();

    BufferRepository& buffers_;
    ItemRepository&   items_;

    std::vector<Buffer> rows_;
    mutable QHash<BufferId, BufferPreview> previewCache_;
    BufferPreview draftPreview_;

    int  expandedRow_   = -1;
    bool pendingReload_ = false;
};

}  // namespace napkin
