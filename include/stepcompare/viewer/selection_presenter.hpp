#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <stepcompare/viewer/viewer_state.hpp>

namespace stepcompare::viewer {

enum class ComponentChangeKind {
    Unchanged,
    GeometryChanged,
    Moved,
    Rotated,
    Added,
    Missing,
};

enum class ComponentHighlight {
    None,
    Changed,
    Selected,
    SelectedChanged,
};

struct ResultRowSnapshot final {
    StableSelectionId stableId;
    std::string label;
    ComponentChangeKind change{ComponentChangeKind::Unchanged};
    std::optional<StableSelectionId> parentStableId;
};

struct ViewerSelectionRequest final {
    StableSelectionId stableId;
    bool fitSelection{true};
    bool highlightSelection{true};
};

class ViewerTreeSelectionPresenter final {
public:
    using RowSelectionHandler = std::function<void(const StableSelectionId&)>;
    using ViewerSelectionHandler =
        std::function<void(const ViewerSelectionRequest&)>;

    ViewerTreeSelectionPresenter(RowSelectionHandler rowSelectionHandler,
                                 ViewerSelectionHandler viewerSelectionHandler);

    void publishRows(std::vector<ResultRowSnapshot> rows);
    [[nodiscard]] const std::vector<ResultRowSnapshot>& rows() const noexcept;

    void onViewerSelection(std::string_view stableId);
    void onRowSelection(std::string_view stableId);
    void clearSelection() noexcept;

    [[nodiscard]] const std::optional<StableSelectionId>& selectedId() const noexcept;
    [[nodiscard]] ComponentHighlight highlightFor(std::string_view stableId) const noexcept;

private:
    [[nodiscard]] const ResultRowSnapshot* find(std::string_view stableId) const noexcept;
    [[nodiscard]] bool isSelected(std::string_view stableId) const noexcept;

    std::vector<ResultRowSnapshot> rows_;
    std::unordered_map<std::string, std::size_t> rowIndex_;
    std::optional<StableSelectionId> selectedId_;
    RowSelectionHandler rowSelectionHandler_;
    ViewerSelectionHandler viewerSelectionHandler_;
};

[[nodiscard]] bool isChanged(ComponentChangeKind change) noexcept;
[[nodiscard]] std::string_view toString(ComponentChangeKind change) noexcept;

}  // namespace stepcompare::viewer
