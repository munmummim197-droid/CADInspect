#include "component_tree_panel.hpp"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <utility>

namespace stepcompare::gui {

ComponentTreePanel::ComponentTreePanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("componentResultTree"));
    tree_->setHeaderLabels({tr("Component"), tr("Result")});
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(tree_);

    connect(tree_, &QTreeWidget::itemSelectionChanged, this, [this] {
        const auto selected = tree_->selectedItems();
        if (selected.size() != 1 || !selectionHandler_) {
            return;
        }
        selectionHandler_(selected.front()->data(0, Qt::UserRole).toString().toStdString());
    });
}

void ComponentTreePanel::setRows(
    const std::span<const stepcompare::viewer::ResultRowSnapshot> rows) {
    const QSignalBlocker blocker(tree_);
    tree_->clear();
    itemsByStableId_.clear();
    itemsByStableId_.reserve(rows.size());

    for (const auto& row : rows) {
        const auto status = stepcompare::viewer::toString(row.change);
        auto* item = new QTreeWidgetItem(
            {QString::fromStdString(row.label),
             QString::fromLatin1(status.data(), static_cast<qsizetype>(status.size()))});
        item->setData(0, Qt::UserRole, QString::fromStdString(row.stableId.value()));

        if (stepcompare::viewer::isChanged(row.change)) {
            auto font = item->font(0);
            font.setBold(true);
            item->setFont(0, font);
            item->setFont(1, font);
            item->setForeground(1, QBrush(QColor(QStringLiteral("#b33a16"))));
            item->setBackground(0, QBrush(QColor(QStringLiteral("#fff2df"))));
            item->setBackground(1, QBrush(QColor(QStringLiteral("#fff2df"))));
        }
        itemsByStableId_.emplace(row.stableId.value(), item);
    }
    for (const auto& row : rows) {
        auto* item = itemsByStableId_.at(row.stableId.value());
        if (row.parentStableId) {
            const auto parent = itemsByStableId_.find(row.parentStableId->value());
            if (parent != itemsByStableId_.end()) {
                parent->second->addChild(item);
                continue;
            }
        }
        tree_->addTopLevelItem(item);
    }
    tree_->expandToDepth(1);
}

void ComponentTreePanel::selectStableId(
    const stepcompare::viewer::StableSelectionId& stableId) {
    const auto found = itemsByStableId_.find(stableId.value());
    if (found == itemsByStableId_.end()) {
        return;
    }
    const QSignalBlocker blocker(tree_);
    tree_->setCurrentItem(found->second);
    tree_->scrollToItem(found->second, QAbstractItemView::PositionAtCenter);
}

void ComponentTreePanel::clearSelection() {
    const QSignalBlocker blocker(tree_);
    tree_->clearSelection();
}

void ComponentTreePanel::setSelectionHandler(SelectionHandler handler) {
    selectionHandler_ = std::move(handler);
}

}  // namespace stepcompare::gui
