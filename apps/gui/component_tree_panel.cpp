#include "component_tree_panel.hpp"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QMenu>
#include <QSignalBlocker>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <utility>

namespace stepcompare::gui {
namespace {

QString resultLabel(const stepcompare::viewer::ComponentChangeKind change) {
    using stepcompare::viewer::ComponentChangeKind;
    switch (change) {
        case ComponentChangeKind::Unchanged:
            return QObject::tr("KHÔNG ĐỔI");
        case ComponentChangeKind::GeometryChanged:
            return QObject::tr("HÌNH HỌC ĐỔI");
        case ComponentChangeKind::Moved:
            return QObject::tr("DI CHUYỂN");
        case ComponentChangeKind::Rotated:
            return QObject::tr("XOAY");
        case ComponentChangeKind::Added:
            return QObject::tr("MỚI");
        case ComponentChangeKind::Missing:
            return QObject::tr("THIẾU");
        case ComponentChangeKind::Ambiguous:
            return QObject::tr("MƠ HỒ");
    }
    return QObject::tr("CHECK");
}

bool isRoot(const stepcompare::viewer::ResultRowSnapshot& row,
            const std::string_view stableId) {
    return row.stableId.value() == stableId;
}

void styleItem(QTreeWidgetItem& item,
               const stepcompare::viewer::ComponentChangeKind change) {
    if (!stepcompare::viewer::isChanged(change)) {
        item.setForeground(1, QBrush(QColor(QStringLiteral("#28734d"))));
        return;
    }
    auto font = item.font(0);
    font.setBold(true);
    item.setFont(0, font);
    item.setFont(1, font);
    item.setForeground(1, QBrush(QColor(QStringLiteral("#a9341b"))));
    item.setBackground(0, QBrush(QColor(QStringLiteral("#fff2df"))));
    item.setBackground(1, QBrush(QColor(QStringLiteral("#fff2df"))));
}

}  // namespace

ComponentTreePanel::ComponentTreePanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("componentResultTree"));
    tree_->setHeaderLabels({tr("Assembly / Part"), tr("Kết quả")});
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->setAnimated(true);
    tree_->setIndentation(18);
    tree_->setUniformRowHeights(true);
    tree_->setAlternatingRowColors(true);
    tree_->setStyleSheet(QStringLiteral(
        "QTreeWidget { background:#f8fafc; alternate-background-color:#f1f5f8; "
        "border:0; color:#1b2b38; }"
        "QTreeWidget::item { height:25px; padding:1px 3px; }"
        "QTreeWidget::item:selected { background:#cfe7f6; color:#102a3b; }"
        "QHeaderView::section { background:#dce7ef; color:#17324a; "
        "font-weight:700; border:0; border-bottom:1px solid #aebdca; padding:5px; }"));
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
    connect(tree_, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* item) {
                if (item == nullptr || !activationHandler_) {
                    return;
                }
                if (item->childCount() > 0) {
                    item->setExpanded(!item->isExpanded());
                    return;
                }
                const auto stableId = item->data(0, Qt::UserRole).toString();
                if (!stableId.isEmpty() && stableId != QStringLiteral("preview/A") &&
                    stableId != QStringLiteral("preview/B")) {
                    activationHandler_(stableId.toStdString());
                }
            });
    connect(tree_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& position) {
                auto* item = tree_->itemAt(position);
                if (item != nullptr) {
                    tree_->setCurrentItem(item);
                }
                const std::string stableId =
                    item == nullptr
                        ? std::string{}
                        : item->data(0, Qt::UserRole).toString().toStdString();
                const bool occurrence = stableId.starts_with("preview/A/") ||
                                        stableId.starts_with("preview/B/");

                QMenu menu(tree_);
                auto* locate = menu.addAction(tr("Locate / Zoom / Highlight"));
                locate->setEnabled(occurrence);
                menu.addSeparator();
                auto* showA = menu.addAction(tr("Show Only A"));
                auto* showB = menu.addAction(tr("Show Only B"));
                auto* showPair = menu.addAction(tr("Show Only Pair"));
                showA->setEnabled(occurrence);
                showB->setEnabled(occurrence);
                showPair->setEnabled(occurrence);
                auto* restore = menu.addAction(tr("Show All / Restore Assembly"));
                menu.addSeparator();
                auto* expand = menu.addAction(tr("Mở rộng nhánh"));
                auto* collapse = menu.addAction(tr("Thu gọn nhánh"));
                expand->setEnabled(item != nullptr && item->childCount() > 0);
                collapse->setEnabled(item != nullptr && item->childCount() > 0);

                QAction* chosen = menu.exec(tree_->viewport()->mapToGlobal(position));
                if (chosen == expand) {
                    item->setExpanded(true);
                } else if (chosen == collapse) {
                    item->setExpanded(false);
                } else if (contextActionHandler_) {
                    if (chosen == locate) {
                        contextActionHandler_(stableId,
                                              ComponentTreeContextAction::Locate);
                    } else if (chosen == showA) {
                        contextActionHandler_(stableId,
                                              ComponentTreeContextAction::ShowOnlyA);
                    } else if (chosen == showB) {
                        contextActionHandler_(stableId,
                                              ComponentTreeContextAction::ShowOnlyB);
                    } else if (chosen == showPair) {
                        contextActionHandler_(stableId,
                                              ComponentTreeContextAction::ShowOnlyPair);
                    } else if (chosen == restore) {
                        contextActionHandler_(
                            stableId,
                            ComponentTreeContextAction::RestoreAssembly);
                    }
                }
            });
    setRows(std::span<const stepcompare::viewer::ResultRowSnapshot>{});
}

void ComponentTreePanel::setRows(
    const std::span<const stepcompare::viewer::ResultRowSnapshot> rows) {
    const QSignalBlocker blocker(tree_);
    tree_->clear();
    itemsByStableId_.clear();
    itemsByStableId_.reserve(rows.size());

    const stepcompare::viewer::ResultRowSnapshot* rootA = nullptr;
    const stepcompare::viewer::ResultRowSnapshot* rootB = nullptr;
    for (const auto& row : rows) {
        if (isRoot(row, "preview/A")) {
            rootA = &row;
        } else if (isRoot(row, "preview/B")) {
            rootB = &row;
        }
    }
    const auto addSideRoot = [this](const QChar side,
                                    const stepcompare::viewer::ResultRowSnapshot* row) {
        const auto label = row == nullptr
                               ? tr("%1: Chưa chọn file").arg(side)
                               : tr("%1: %2")
                                     .arg(side)
                                     .arg(QString::fromStdString(row->label));
        auto* item = new QTreeWidgetItem(
            {label, row == nullptr ? QString{} : resultLabel(row->change)});
        auto font = item->font(0);
        font.setBold(true);
        item->setFont(0, font);
        item->setBackground(0, QBrush(QColor(QStringLiteral("#dceaf3"))));
        item->setBackground(1, QBrush(QColor(QStringLiteral("#dceaf3"))));
        if (row != nullptr) {
            item->setData(0, Qt::UserRole,
                          QString::fromStdString(row->stableId.value()));
            styleItem(*item, row->change);
            itemsByStableId_.emplace(row->stableId.value(), item);
        }
        tree_->addTopLevelItem(item);
        return item;
    };
    auto* sideA = addSideRoot(QLatin1Char('A'), rootA);
    auto* sideB = addSideRoot(QLatin1Char('B'), rootB);

    for (const auto& row : rows) {
        if (isRoot(row, "preview/A") || isRoot(row, "preview/B")) {
            continue;
        }
        auto* item = new QTreeWidgetItem(
            {QString::fromStdString(row.label), resultLabel(row.change)});
        item->setData(0, Qt::UserRole, QString::fromStdString(row.stableId.value()));
        item->setToolTip(0, QString::fromStdString(row.label));
        styleItem(*item, row.change);
        itemsByStableId_.emplace(row.stableId.value(), item);
    }
    for (const auto& row : rows) {
        if (isRoot(row, "preview/A") || isRoot(row, "preview/B")) {
            continue;
        }
        auto* item = itemsByStableId_.at(row.stableId.value());
        if (row.parentStableId) {
            const auto parent = itemsByStableId_.find(row.parentStableId->value());
            if (parent != itemsByStableId_.end()) {
                parent->second->addChild(item);
                continue;
            }
        }
        (row.stableId.value().starts_with("preview/A/") ? sideA : sideB)
            ->addChild(item);
    }
    sideA->setExpanded(rootA != nullptr);
    sideB->setExpanded(rootB != nullptr);
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

void ComponentTreePanel::setActivationHandler(ActivationHandler handler) {
    activationHandler_ = std::move(handler);
}

void ComponentTreePanel::setContextActionHandler(ContextActionHandler handler) {
    contextActionHandler_ = std::move(handler);
}

}  // namespace stepcompare::gui
