#include "BufferListModel.h"
#include "../data/BufferRepository.h"
#include "../data/ItemRepository.h"
#include "../domain/Clock.h"

namespace napkin {
namespace {
// Generous enough that scrolling never blocks on a query, small enough that we
// are never reading the whole table. Revisited if §12 measurements say so.
constexpr int kMaxRows = 5000;
}  // namespace

BufferListModel::BufferListModel(BufferRepository& buffers, ItemRepository& items, QObject* parent)
    : QAbstractListModel(parent), buffers_(buffers), items_(items)
{
    reload();
}

int BufferListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(rows_.size());
}

void BufferListModel::reload()
{
    if (expandedRow_ >= 0) { pendingReload_ = true; return; }  // see setExpandedRow

    beginResetModel();
    rows_ = buffers_.listLive(kMaxRows);
    previewCache_.clear();
    endResetModel();
    emit countChanged(int(rows_.size()));
}

BufferId BufferListModel::idAt(int row) const
{
    if (row < 0 || row >= int(rows_.size())) return kNoBuffer;
    return rows_[size_t(row)].id;
}

int BufferListModel::rowForId(BufferId id) const
{
    for (size_t i = 0; i < rows_.size(); ++i)
        if (rows_[i].id == id) return int(i);
    return -1;
}

int BufferListModel::draftRow() const
{
    for (size_t i = 0; i < rows_.size(); ++i)
        if (rows_[i].id == kNoBuffer) return int(i);
    return -1;
}

BufferPreview BufferListModel::previewFor(BufferId id) const
{
    if (const auto it = previewCache_.constFind(id); it != previewCache_.constEnd())
        return *it;
    const auto preview = derivePreview(items_.previewHead(id), items_.countForBuffer(id));
    previewCache_.insert(id, preview);
    return preview;
}

QVariant BufferListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= int(rows_.size())) return {};
    const int row = index.row();
    const Buffer& b = rows_[size_t(row)];
    const bool isDraft = b.id == kNoBuffer;
    const BufferPreview p = isDraft ? draftPreview_ : previewFor(b.id);

    switch (role) {
    case IdRole:         return QVariant::fromValue(b.id);
    case PrimaryRole:    return p.primary;
    case SecondaryRole:  return p.secondary;
    case ItemCountRole:  return p.itemCount;
    case HasImageRole:   return p.hasImage;
    case ModifiedAtRole: return QVariant::fromValue(b.modifiedAt);
    case PinnedRole:     return b.pinned;
    case KeptRole:       return b.kept;
    case IsDraftRole:    return isDraft;
    case IsExpandedRole: return row == expandedRow_;
    case SectionFirstRole:
        if (row == 0) return true;
        return rows_[size_t(row) - 1].pinned != b.pinned;
    case SectionNameRole:
        return b.pinned ? QStringLiteral("PINNED") : QStringLiteral("RECENT");
    case Qt::AccessibleTextRole:
        // Never encode state in styling alone (SPEC.md §14).
        return QStringLiteral("%1. %2%3")
            .arg(p.primary.isEmpty() ? QStringLiteral("Empty buffer") : p.primary,
                 p.secondary, b.kept ? QStringLiteral(". Kept") : QString());
    default: return {};
    }
}

Qt::ItemFlags BufferListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void BufferListModel::setExpandedRow(int row)
{
    if (row == expandedRow_) return;
    const int previous = expandedRow_;
    expandedRow_ = row;

    for (int r : {previous, row})
        if (r >= 0 && r < int(rows_.size()))
            emit dataChanged(index(r), index(r), {IsExpandedRole});

    // The list re-sorts only once nothing is being edited.
    if (expandedRow_ < 0 && pendingReload_) {
        pendingReload_ = false;
        reload();
    }
}

int BufferListModel::insertDraftRow()
{
    if (const int existing = draftRow(); existing >= 0) return existing;

    // A new draft is the most recent thing there is, so it goes at the top of
    // RECENT — below pinned buffers, above everything else.
    int row = 0;
    while (row < int(rows_.size()) && rows_[size_t(row)].pinned) ++row;

    Buffer draft;
    draft.id = kNoBuffer;
    draft.createdAt = draft.modifiedAt = nowMs();

    beginInsertRows({}, row, row);
    rows_.insert(rows_.begin() + row, draft);
    draftPreview_ = {};
    endInsertRows();
    return row;
}

void BufferListModel::setDraftPreview(const BufferPreview& preview)
{
    const int row = draftRow();
    if (row < 0) return;
    draftPreview_ = preview;
    emit dataChanged(index(row), index(row));
}

void BufferListModel::promoteDraft(BufferId newId)
{
    const int row = draftRow();
    if (row < 0) return;
    rows_[size_t(row)].id = newId;
    previewCache_.insert(newId, draftPreview_);
    emit dataChanged(index(row), index(row));
    emit countChanged(int(rows_.size()));
}

void BufferListModel::removeDraftRow()
{
    const int row = draftRow();
    if (row < 0) return;
    beginRemoveRows({}, row, row);
    rows_.erase(rows_.begin() + row);
    endRemoveRows();
    if (expandedRow_ == row) expandedRow_ = -1;
    emit countChanged(int(rows_.size()));
}

void BufferListModel::invalidatePreview(BufferId id)
{
    previewCache_.remove(id);
    const int row = rowForId(id);
    if (row >= 0) emit dataChanged(index(row), index(row));
}

void BufferListModel::refreshTimestamps()
{
    emitAllChanged();
}

void BufferListModel::emitAllChanged()
{
    if (rows_.empty()) return;
    emit dataChanged(index(0), index(int(rows_.size()) - 1), {ModifiedAtRole});
}

}  // namespace napkin
