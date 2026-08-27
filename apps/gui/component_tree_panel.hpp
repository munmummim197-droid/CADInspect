#pragma once

#include <QWidget>

#include <functional>
#include <span>
#include <string>
#include <unordered_map>

#include <stepcompare/viewer/selection_presenter.hpp>

class QTreeWidget;
class QTreeWidgetItem;

namespace stepcompare::gui {

enum class ComponentTreeContextAction {
    Locate,
    ShowOnlyA,
    ShowOnlyB,
    ShowOnlyPair,
    RestoreAssembly,
};

class ComponentTreePanel final : public QWidget {
public:
    using SelectionHandler = std::function<void(std::string)>;
    using ActivationHandler = std::function<void(std::string)>;
    using ContextActionHandler =
        std::function<void(std::string, ComponentTreeContextAction)>;

    explicit ComponentTreePanel(QWidget* parent = nullptr);

    void setRows(std::span<const stepcompare::viewer::ResultRowSnapshot> rows);
    void selectStableId(const stepcompare::viewer::StableSelectionId& stableId);
    void clearSelection();
    void setSelectionHandler(SelectionHandler handler);
    void setActivationHandler(ActivationHandler handler);
    void setContextActionHandler(ContextActionHandler handler);

private:
    QTreeWidget* tree_{};
    std::unordered_map<std::string, QTreeWidgetItem*> itemsByStableId_;
    SelectionHandler selectionHandler_;
    ActivationHandler activationHandler_;
    ContextActionHandler contextActionHandler_;
};

}  // namespace stepcompare::gui
