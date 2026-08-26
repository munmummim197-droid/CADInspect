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

class ComponentTreePanel final : public QWidget {
public:
    using SelectionHandler = std::function<void(std::string)>;

    explicit ComponentTreePanel(QWidget* parent = nullptr);

    void setRows(std::span<const stepcompare::viewer::ResultRowSnapshot> rows);
    void selectStableId(const stepcompare::viewer::StableSelectionId& stableId);
    void clearSelection();
    void setSelectionHandler(SelectionHandler handler);

private:
    QTreeWidget* tree_{};
    std::unordered_map<std::string, QTreeWidgetItem*> itemsByStableId_;
    SelectionHandler selectionHandler_;
};

}  // namespace stepcompare::gui
