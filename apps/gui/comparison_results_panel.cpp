#include "comparison_results_panel.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLocale>
#include <QModelIndex>
#include <QSignalBlocker>
#include <QTableView>
#include <QTabWidget>
#include <QVBoxLayout>

#include <utility>

namespace stepcompare::gui {
namespace {

QString millimeters(const double value) {
    static const QLocale locale(QLocale::Vietnamese, QLocale::Vietnam);
    return QStringLiteral("%1 mm").arg(locale.toString(value, 'f', 4));
}

void configureTable(QTableView& table) {
    table.setEditTriggers(QAbstractItemView::NoEditTriggers);
    table.setSelectionBehavior(QAbstractItemView::SelectRows);
    table.setSelectionMode(QAbstractItemView::SingleSelection);
    table.setAlternatingRowColors(true);
    table.setWordWrap(false);
    table.setTextElideMode(Qt::ElideMiddle);
    table.verticalHeader()->setDefaultSectionSize(24);
    table.verticalHeader()->setVisible(false);
    table.horizontalHeader()->setHighlightSections(false);
}

}  // namespace

ComparisonResultsPanel::ComparisonResultsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("comparisonResultsPanel"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    tabs_ = new QTabWidget(this);
    tabs_->setObjectName(QStringLiteral("comparisonTabs"));

    auto* parametersPage = new QWidget(tabs_);
    auto* parametersLayout = new QVBoxLayout(parametersPage);
    parametersLayout->setContentsMargins(0, 0, 0, 0);
    parameterModel_ = new ComparisonParameterModel(parametersPage);
    parametersTable_ = new QTableView(parametersPage);
    parametersTable_->setObjectName(QStringLiteral("comparisonParametersTable"));
    parametersTable_->setModel(parameterModel_);
    configureTable(*parametersTable_);
    parametersTable_->horizontalHeader()->setSectionResizeMode(
        ComparisonParameterModel::Parameter, QHeaderView::ResizeToContents);
    parametersTable_->horizontalHeader()->setSectionResizeMode(
        ComparisonParameterModel::FileA, QHeaderView::Stretch);
    parametersTable_->horizontalHeader()->setSectionResizeMode(
        ComparisonParameterModel::FileB, QHeaderView::Stretch);
    parametersTable_->horizontalHeader()->setSectionResizeMode(
        ComparisonParameterModel::DifferenceBMinusA, QHeaderView::ResizeToContents);
    parametersTable_->horizontalHeader()->setSectionResizeMode(
        ComparisonParameterModel::Tolerance, QHeaderView::ResizeToContents);
    parametersTable_->horizontalHeader()->setSectionResizeMode(
        ComparisonParameterModel::Result, QHeaderView::ResizeToContents);
    parametersLayout->addWidget(parametersTable_);
    tabs_->addTab(parametersPage, tr("Thông số so sánh"));

    componentPage_ = new QWidget(tabs_);
    auto* componentLayout = new QVBoxLayout(componentPage_);
    componentLayout->setContentsMargins(0, 0, 0, 0);
    componentLayout->setSpacing(4);
    auto* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel(tr("Bộ lọc:"), componentPage_));
    filterCombo_ = new QComboBox(componentPage_);
    filterCombo_->setObjectName(QStringLiteral("componentFilterCombo"));
    const std::pair<const char*, ComponentFilter> filters[] = {
        {"Tất cả", ComponentFilter::All},
        {"Chỉ khác biệt", ComponentFilter::DifferencesOnly},
        {"Moved", ComponentFilter::Moved},
        {"Rotated", ComponentFilter::Rotated},
        {"Geometry Changed", ComponentFilter::GeometryChanged},
        {"Missing", ComponentFilter::Missing},
        {"New", ComponentFilter::Added},
        {"Ambiguous", ComponentFilter::Ambiguous},
    };
    for (const auto& [label, filter] : filters) {
        filterCombo_->addItem(tr(label), static_cast<int>(filter));
    }
    filterLayout->addWidget(filterCombo_);
    componentCount_ = new QLabel(tr("0 linh kiện"), componentPage_);
    componentCount_->setObjectName(QStringLiteral("componentComparisonCount"));
    filterLayout->addStretch(1);
    filterLayout->addWidget(componentCount_);
    componentLayout->addLayout(filterLayout);

    componentModel_ = new ComponentComparisonModel(componentPage_);
    componentProxy_ = new ComponentFilterProxyModel(componentPage_);
    componentProxy_->setSourceModel(componentModel_);
    componentsTable_ = new QTableView(componentPage_);
    componentsTable_->setObjectName(QStringLiteral("componentComparisonTable"));
    componentsTable_->setModel(componentProxy_);
    configureTable(*componentsTable_);
    componentsTable_->setSortingEnabled(true);
    componentsTable_->horizontalHeader()->setSectionResizeMode(
        ComponentComparisonModel::Component, QHeaderView::Stretch);
    componentsTable_->horizontalHeader()->setSectionResizeMode(
        ComponentComparisonModel::Status, QHeaderView::ResizeToContents);
    for (int column = ComponentComparisonModel::DeltaX;
         column < ComponentComparisonModel::ColumnCount;
         ++column) {
        componentsTable_->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }
    componentLayout->addWidget(componentsTable_);

    heatmapLegend_ = new QLabel(componentPage_);
    heatmapLegend_->setObjectName(QStringLiteral("heatmapNumericalLegend"));
    heatmapLegend_->setWordWrap(true);
    heatmapLegend_->setStyleSheet(QStringLiteral(
        "QLabel { padding: 4px 8px; border: 1px solid #c8d4de; "
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0, "
        "stop:0 #dff4e5, stop:0.55 #fff2b8, stop:1 #ffd8d0); color:#182532; }"));
    componentLayout->addWidget(heatmapLegend_);
    tabs_->addTab(componentPage_, tr("So sánh linh kiện"));

    featurePage_ = new QWidget(tabs_);
    auto* featureLayout = new QVBoxLayout(featurePage_);
    featureLayout->setContentsMargins(0, 0, 0, 0);
    featureModel_ = new FeatureComparisonModel(featurePage_);
    featuresTable_ = new QTableView(featurePage_);
    featuresTable_->setObjectName(QStringLiteral("featureComparisonTable"));
    featuresTable_->setModel(featureModel_);
    configureTable(*featuresTable_);
    featuresTable_->setWordWrap(true);
    featuresTable_->verticalHeader()->setDefaultSectionSize(72);
    featuresTable_->setSortingEnabled(false);
    featuresTable_->horizontalHeader()->setSectionResizeMode(
        FeatureComparisonModel::Feature, QHeaderView::ResizeToContents);
    featuresTable_->horizontalHeader()->setSectionResizeMode(
        FeatureComparisonModel::Type, QHeaderView::ResizeToContents);
    featuresTable_->horizontalHeader()->setSectionResizeMode(
        FeatureComparisonModel::FileA, QHeaderView::Stretch);
    featuresTable_->horizontalHeader()->setSectionResizeMode(
        FeatureComparisonModel::FileB, QHeaderView::Stretch);
    featuresTable_->horizontalHeader()->setSectionResizeMode(
        FeatureComparisonModel::DifferenceBMinusA, QHeaderView::Stretch);
    featuresTable_->horizontalHeader()->setSectionResizeMode(
        FeatureComparisonModel::Tolerance, QHeaderView::ResizeToContents);
    featuresTable_->horizontalHeader()->setSectionResizeMode(
        FeatureComparisonModel::Result, QHeaderView::ResizeToContents);
    featureLayout->addWidget(featuresTable_);
    tabs_->addTab(featurePage_, tr("So sánh feature"));
    root->addWidget(tabs_);

    connect(filterCombo_, &QComboBox::currentIndexChanged, this, [this](const int index) {
        applyFilter(static_cast<ComponentFilter>(filterCombo_->itemData(index).toInt()));
    });
    connect(componentsTable_->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            [this](const QModelIndex& current) { publishSelection(current, false); });
    connect(componentsTable_, &QTableView::doubleClicked, this,
            [this](const QModelIndex& index) { publishSelection(index, true); });
    connect(featuresTable_->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            [this](const QModelIndex& current) {
                publishFeatureSelection(current, false);
            });
    connect(featuresTable_, &QTableView::doubleClicked, this,
            [this](const QModelIndex& index) {
                publishFeatureSelection(index, true);
            });

    refreshCount();
    refreshHeatmapLegend();
}

void ComparisonResultsPanel::setReport(
    const stepcompare::reporting::Report& report) {
    parameterModel_->setReport(report);
    componentModel_->setReport(report);
    featureModel_->setReport(report);
    heatmapMaximumMm_ = report.deepDeviation.maximumMm;
    heatmapToleranceMm_ = report.tolerances.surfaceMm;
    heatmapEvidenceAvailable_ = report.deepDeviation.available;
    applyParameterSpans();
    applyFilter(componentProxy_->componentFilter());
    refreshHeatmapLegend();
}

void ComparisonResultsPanel::clearReport() {
    parameterModel_->clearReport();
    componentModel_->clearReport();
    featureModel_->clearReport();
    heatmapMaximumMm_ = 0.0;
    heatmapToleranceMm_ = 0.0;
    heatmapEvidenceAvailable_ = false;
    refreshCount();
    refreshHeatmapLegend();
}

void ComparisonResultsPanel::selectStableId(
    const stepcompare::viewer::StableSelectionId& stableId) {
    QModelIndex sourceIndex = componentModel_->indexForStableId(stableId.value());
    if (!sourceIndex.isValid()) {
        return;
    }
    QModelIndex proxyIndex = componentProxy_->mapFromSource(sourceIndex);
    if (!proxyIndex.isValid()) {
        const QSignalBlocker blocker(filterCombo_);
        filterCombo_->setCurrentIndex(0);
        componentProxy_->setComponentFilter(ComponentFilter::All);
        proxyIndex = componentProxy_->mapFromSource(sourceIndex);
        refreshCount();
    }
    if (!proxyIndex.isValid()) {
        return;
    }
    const QSignalBlocker blocker(componentsTable_->selectionModel());
    componentsTable_->setCurrentIndex(proxyIndex);
    componentsTable_->selectRow(proxyIndex.row());
    componentsTable_->scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
    tabs_->setCurrentWidget(componentPage_);

    const QModelIndex featureIndex = featureModel_->indexForOwnerStableId(stableId.value());
    if (featureIndex.isValid()) {
        const QSignalBlocker featureBlocker(featuresTable_->selectionModel());
        featuresTable_->setCurrentIndex(featureIndex);
        featuresTable_->selectRow(featureIndex.row());
        featuresTable_->scrollTo(featureIndex,
                                 QAbstractItemView::PositionAtCenter);
    }
}

void ComparisonResultsPanel::setSelectionHandler(SelectionHandler handler) {
    selectionHandler_ = std::move(handler);
}

void ComparisonResultsPanel::setFeatureSelectionHandler(
    FeatureSelectionHandler handler) {
    featureSelectionHandler_ = std::move(handler);
}

void ComparisonResultsPanel::setHeatmapState(const bool enabled,
                                             const bool evidenceAvailable) {
    heatmapEnabled_ = enabled;
    heatmapEvidenceAvailable_ = evidenceAvailable;
    refreshHeatmapLegend();
}

void ComparisonResultsPanel::applyParameterSpans() {
    parametersTable_->clearSpans();
    for (int row = 0; row < parameterModel_->rowCount(); ++row) {
        if (parameterModel_->isGroupHeader(row)) {
            parametersTable_->setSpan(
                row, 0, 1, ComparisonParameterModel::ColumnCount);
        }
    }
}

void ComparisonResultsPanel::applyFilter(const ComponentFilter filter) {
    componentProxy_->setComponentFilter(filter);
    refreshCount();
}

void ComparisonResultsPanel::publishSelection(const QModelIndex& proxyIndex,
                                              const bool locate) {
    if (!proxyIndex.isValid() || !selectionHandler_) {
        return;
    }
    const QModelIndex sourceIndex = componentProxy_->mapToSource(proxyIndex);
    const std::string stableId = componentModel_->preferredStableId(sourceIndex);
    if (!stableId.empty()) {
        selectionHandler_(stableId, locate);
    }
}

void ComparisonResultsPanel::publishFeatureSelection(const QModelIndex& index,
                                                     const bool locate) {
    if (!index.isValid() || !featureSelectionHandler_) {
        return;
    }
    auto target = featureModel_->selectionTarget(index);
    if (!target.ownerStableId.empty()) {
        featureSelectionHandler_(std::move(target.ownerStableId),
                                 std::move(target.faceIndices), locate);
    }
}

void ComparisonResultsPanel::refreshCount() {
    componentCount_->setText(
        tr("Hiển thị %1 / %2 linh kiện")
            .arg(componentProxy_->rowCount())
            .arg(componentModel_->rowCount()));
}

void ComparisonResultsPanel::refreshHeatmapLegend() {
    if (!heatmapEvidenceAvailable_) {
        heatmapLegend_->setText(
            tr("Heatmap: CHECK — chưa có numerical deviation evidence hợp lệ."));
        return;
    }
    heatmapLegend_->setText(
        tr("Heatmap %1 — Max deviation: %2 | Dung sai bề mặt: %3 | "
           "Xanh: trong dung sai · Vàng: gần dung sai · Đỏ: vượt dung sai")
            .arg(heatmapEnabled_ ? tr("BẬT") : tr("TẮT"))
            .arg(millimeters(heatmapMaximumMm_))
            .arg(millimeters(heatmapToleranceMm_)));
}

}  // namespace stepcompare::gui
