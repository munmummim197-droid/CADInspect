#include <stepcompare/viewer/selection_presenter.hpp>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::vector<stepcompare::viewer::ResultRowSnapshot> rows() {
    using namespace stepcompare::viewer;
    return {
        {StableSelectionId{"root/unchanged"}, "Unchanged", ComponentChangeKind::Unchanged},
        {StableSelectionId{"root/changed"}, "Changed", ComponentChangeKind::GeometryChanged},
        {StableSelectionId{"root/moved"}, "Moved", ComponentChangeKind::Moved},
    };
}

void viewerClickSelectsResultRow() {
    using namespace stepcompare::viewer;
    std::optional<std::string> selectedRow;
    int viewerCommands = 0;
    ViewerTreeSelectionPresenter presenter(
        [&](const StableSelectionId& id) { selectedRow = id.value(); },
        [&](const ViewerSelectionRequest&) { ++viewerCommands; });
    presenter.publishRows(rows());

    presenter.onViewerSelection("root/changed");

    expect(selectedRow == "root/changed", "viewer click must select matching result row");
    expect(viewerCommands == 0, "viewer click must not echo a viewer command");
    expect(presenter.selectedId() && presenter.selectedId()->value() == "root/changed",
           "presenter must retain selected stable ID");
}

void rowClickSelectsFitsAndHighlightsViewer() {
    using namespace stepcompare::viewer;
    std::optional<ViewerSelectionRequest> request;
    int rowCommands = 0;
    ViewerTreeSelectionPresenter presenter(
        [&](const StableSelectionId&) { ++rowCommands; },
        [&](const ViewerSelectionRequest& command) { request = command; });
    presenter.publishRows(rows());

    presenter.onRowSelection("root/moved");

    expect(request && request->stableId.value() == "root/moved",
           "row click must select matching viewer object");
    expect(request && request->fitSelection, "row click must fit selected viewer object");
    expect(request && request->highlightSelection,
           "row click must request viewer selection highlight");
    expect(rowCommands == 0, "row click must not echo a row command");
}

void changedHighlightIsPersistentAndSelectionAware() {
    using namespace stepcompare::viewer;
    ViewerTreeSelectionPresenter presenter({}, {});
    presenter.publishRows(rows());

    expect(presenter.highlightFor("root/unchanged") == ComponentHighlight::None,
           "unchanged component must not receive persistent highlight");
    expect(presenter.highlightFor("root/changed") == ComponentHighlight::Changed,
           "geometry-changed component must receive persistent highlight");
    expect(presenter.highlightFor("root/moved") == ComponentHighlight::Changed,
           "moved component must receive persistent highlight");

    presenter.onRowSelection("root/changed");
    expect(presenter.highlightFor("root/changed") == ComponentHighlight::SelectedChanged,
           "selected changed component must preserve changed and selected semantics");
    presenter.clearSelection();
    expect(presenter.highlightFor("root/changed") == ComponentHighlight::Changed,
           "changed highlight must survive selection clearing");
}

void unknownAndDuplicateIdsFailClosed() {
    using namespace stepcompare::viewer;
    int callbacks = 0;
    ViewerTreeSelectionPresenter presenter(
        [&](const StableSelectionId&) { ++callbacks; },
        [&](const ViewerSelectionRequest&) { ++callbacks; });
    presenter.publishRows(rows());
    presenter.onViewerSelection("unknown");
    presenter.onRowSelection("unknown");
    expect(callbacks == 0, "unknown IDs must never synchronize to another surface");
    expect(!presenter.selectedId(), "unknown IDs must not become selected");

    bool duplicateRejected = false;
    try {
        presenter.publishRows({
            {StableSelectionId{"duplicate"}, "One", ComponentChangeKind::Unchanged},
            {StableSelectionId{"duplicate"}, "Two", ComponentChangeKind::GeometryChanged},
        });
    } catch (const std::invalid_argument&) {
        duplicateRejected = true;
    }
    expect(duplicateRejected, "duplicate stable IDs must be rejected");
}

void assemblyHierarchyMustReferenceStableAcyclicParents() {
    using namespace stepcompare::viewer;
    ViewerTreeSelectionPresenter presenter({}, {});
    presenter.publishRows({
        {StableSelectionId{"root"}, "Assembly", ComponentChangeKind::Unchanged},
        {StableSelectionId{"root/part"},
         "Part",
         ComponentChangeKind::GeometryChanged,
         StableSelectionId{"root"}},
    });
    expect(presenter.rows()[1].parentStableId &&
               presenter.rows()[1].parentStableId->value() == "root",
           "assembly row must retain explicit stable parent ID");

    bool missingParentRejected = false;
    try {
        presenter.publishRows({
            {StableSelectionId{"orphan"},
             "Orphan",
             ComponentChangeKind::Unchanged,
             StableSelectionId{"missing"}},
        });
    } catch (const std::invalid_argument&) {
        missingParentRejected = true;
    }
    expect(missingParentRejected, "unknown parent IDs must fail closed");

    bool cycleRejected = false;
    try {
        presenter.publishRows({
            {StableSelectionId{"one"},
             "One",
             ComponentChangeKind::Unchanged,
             StableSelectionId{"two"}},
            {StableSelectionId{"two"},
             "Two",
             ComponentChangeKind::Unchanged,
             StableSelectionId{"one"}},
        });
    } catch (const std::invalid_argument&) {
        cycleRejected = true;
    }
    expect(cycleRejected, "cyclic assembly parent IDs must fail closed");
}

}  // namespace

int main() {
    viewerClickSelectsResultRow();
    rowClickSelectsFitsAndHighlightsViewer();
    changedHighlightIsPersistentAndSelectionAware();
    unknownAndDuplicateIdsFailClosed();
    assemblyHierarchyMustReferenceStableAcyclicParents();

    if (failures != 0) {
        std::cerr << failures << " presenter assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All headless viewer/tree presenter tests passed\n";
    return EXIT_SUCCESS;
}
