#include <stepcompare/viewer/selection_presenter.hpp>

#include <stdexcept>
#include <utility>

namespace stepcompare::viewer {

ViewerTreeSelectionPresenter::ViewerTreeSelectionPresenter(
    RowSelectionHandler rowSelectionHandler,
    ViewerSelectionHandler viewerSelectionHandler)
    : rowSelectionHandler_(std::move(rowSelectionHandler)),
      viewerSelectionHandler_(std::move(viewerSelectionHandler)) {}

void ViewerTreeSelectionPresenter::publishRows(std::vector<ResultRowSnapshot> rows) {
    std::unordered_map<std::string, std::size_t> nextIndex;
    nextIndex.reserve(rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const auto& row = rows[index];
        if (!nextIndex.emplace(row.stableId.value(), index).second) {
            throw std::invalid_argument("Duplicate stable component ID: " +
                                        row.stableId.value());
        }
    }
    for (std::size_t index = 0; index < rows.size(); ++index) {
        auto cursor = index;
        std::size_t parentHops = 0;
        while (rows[cursor].parentStableId) {
            const auto parent = nextIndex.find(rows[cursor].parentStableId->value());
            if (parent == nextIndex.end()) {
                throw std::invalid_argument("Unknown parent stable component ID: " +
                                            rows[cursor].parentStableId->value());
            }
            cursor = parent->second;
            ++parentHops;
            if (parentHops >= rows.size()) {
                throw std::invalid_argument("Cycle in stable component hierarchy");
            }
        }
    }

    rows_ = std::move(rows);
    rowIndex_ = std::move(nextIndex);
    if (selectedId_ && find(selectedId_->value()) == nullptr) {
        selectedId_.reset();
    }
}

const std::vector<ResultRowSnapshot>& ViewerTreeSelectionPresenter::rows() const noexcept {
    return rows_;
}

void ViewerTreeSelectionPresenter::onViewerSelection(const std::string_view stableId) {
    const auto* row = find(stableId);
    if (row == nullptr) {
        return;
    }
    selectedId_ = row->stableId;
    if (rowSelectionHandler_) {
        rowSelectionHandler_(row->stableId);
    }
}

void ViewerTreeSelectionPresenter::onRowSelection(const std::string_view stableId,
                                                  const bool fitSelection) {
    const auto* row = find(stableId);
    if (row == nullptr) {
        return;
    }
    selectedId_ = row->stableId;
    if (viewerSelectionHandler_) {
        viewerSelectionHandler_({.stableId = row->stableId,
                                 .fitSelection = fitSelection,
                                 .highlightSelection = true});
    }
}

void ViewerTreeSelectionPresenter::clearSelection() noexcept {
    selectedId_.reset();
}

const std::optional<StableSelectionId>&
ViewerTreeSelectionPresenter::selectedId() const noexcept {
    return selectedId_;
}

ComponentHighlight ViewerTreeSelectionPresenter::highlightFor(
    const std::string_view stableId) const noexcept {
    const auto* row = find(stableId);
    if (row == nullptr) {
        return ComponentHighlight::None;
    }
    const bool changed = isChanged(row->change);
    const bool selected = isSelected(stableId);
    if (changed && selected) {
        return ComponentHighlight::SelectedChanged;
    }
    if (changed) {
        return ComponentHighlight::Changed;
    }
    if (selected) {
        return ComponentHighlight::Selected;
    }
    return ComponentHighlight::None;
}

const ResultRowSnapshot* ViewerTreeSelectionPresenter::find(
    const std::string_view stableId) const noexcept {
    const auto found = rowIndex_.find(std::string{stableId});
    if (found == rowIndex_.end()) {
        return nullptr;
    }
    return &rows_[found->second];
}

bool ViewerTreeSelectionPresenter::isSelected(
    const std::string_view stableId) const noexcept {
    return selectedId_ && selectedId_->value() == stableId;
}

bool isChanged(const ComponentChangeKind change) noexcept {
    return change != ComponentChangeKind::Unchanged;
}

std::string_view toString(const ComponentChangeKind change) noexcept {
    switch (change) {
        case ComponentChangeKind::Unchanged:
            return "UNCHANGED";
        case ComponentChangeKind::GeometryChanged:
            return "GEOMETRY_CHANGED";
        case ComponentChangeKind::Moved:
            return "MOVED";
        case ComponentChangeKind::Rotated:
            return "ROTATED";
        case ComponentChangeKind::Added:
            return "NEW";
        case ComponentChangeKind::Missing:
            return "MISSING";
        case ComponentChangeKind::Ambiguous:
            return "AMBIGUOUS";
    }
    return "UNKNOWN";
}

}  // namespace stepcompare::viewer
