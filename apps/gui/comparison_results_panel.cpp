#include "comparison_results_panel.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QModelIndex>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableView>
#include <QTabWidget>
#include <QVBoxLayout>

#include <utility>

namespace stepcompare::gui {
namespace {

QString millimeters(const double value) {
    static const QLocale locale(QLocale::Vietnamese, QLocale::Vietnam);
    return QStringLiteral("%1 mm").arg(locale.toString(value, 'f', 2));
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

    partPage_ = new QWidget(tabs_);
    auto* partLayout = new QVBoxLayout(partPage_);
    partLayout->setContentsMargins(0, 0, 0, 0);
    partLayout->setSpacing(4);
    auto* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel(tr("Bộ lọc Part:"), partPage_));
    filterCombo_ = new QComboBox(partPage_);
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
    partCount_ = new QLabel(tr("0 Part"), partPage_);
    partCount_->setObjectName(QStringLiteral("partComparisonCount"));
    filterLayout->addStretch(1);
    filterLayout->addWidget(partCount_);
    partLayout->addLayout(filterLayout);

    auto* masterDetail = new QSplitter(Qt::Vertical, partPage_);
    partModel_ = new PartComparisonModel(masterDetail);
    partProxy_ = new ComponentFilterProxyModel(masterDetail);
    partProxy_->setSourceModel(partModel_);
    partsTable_ = new QTableView(masterDetail);
    partsTable_->setObjectName(QStringLiteral("partComparisonTable"));
    partsTable_->setModel(partProxy_);
    configureTable(*partsTable_);
    partsTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    // Keep the summary useful at the application's minimum supported height:
    // the header plus at least two Part rows must remain visible before the
    // occurrence detail receives the rest of the splitter space.
    partsTable_->setMinimumHeight(82);
    partsTable_->setSortingEnabled(true);
    partsTable_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    partsTable_->horizontalHeader()->setSectionResizeMode(
        PartComparisonModel::Part, QHeaderView::Stretch);
    partsTable_->horizontalHeader()->setSectionResizeMode(
        PartComparisonModel::Notes, QHeaderView::Stretch);
    for (int column = PartComparisonModel::QuantityA;
         column < PartComparisonModel::ColumnCount; ++column) {
        if (column != PartComparisonModel::Notes) {
            partsTable_->horizontalHeader()->setSectionResizeMode(
                column, QHeaderView::ResizeToContents);
        }
    }

    auto* occurrencePage = new QWidget(masterDetail);
    auto* occurrenceLayout = new QVBoxLayout(occurrencePage);
    occurrenceLayout->setContentsMargins(0, 4, 0, 0);
    occurrenceLayout->setSpacing(3);
    occurrenceCount_ = new QLabel(tr("Chọn một Part để xem occurrence"), occurrencePage);
    occurrenceCount_->setObjectName(QStringLiteral("occurrenceComparisonCount"));
    occurrenceCount_->setStyleSheet(
        QStringLiteral("QLabel { font-weight:600; color:#27445d; padding:2px; }"));
    occurrenceLayout->addWidget(occurrenceCount_);
    componentModel_ = new ComponentComparisonModel(occurrencePage);
    occurrencesTable_ = new QTableView(occurrencePage);
    occurrencesTable_->setObjectName(QStringLiteral("occurrenceComparisonTable"));
    occurrencesTable_->setModel(componentModel_);
    configureTable(*occurrencesTable_);
    occurrencesTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    occurrencesTable_->setSortingEnabled(true);
    occurrencesTable_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    occurrencesTable_->horizontalHeader()->setSectionResizeMode(
        ComponentComparisonModel::Component, QHeaderView::Stretch);
    occurrencesTable_->horizontalHeader()->setSectionResizeMode(
        ComponentComparisonModel::Status, QHeaderView::ResizeToContents);
    for (int column = ComponentComparisonModel::DeltaX;
         column < ComponentComparisonModel::ColumnCount; ++column) {
        occurrencesTable_->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }
    occurrenceLayout->addWidget(occurrencesTable_);
    masterDetail->addWidget(partsTable_);
    masterDetail->addWidget(occurrencePage);
    masterDetail->setChildrenCollapsible(false);
    masterDetail->setStretchFactor(0, 3);
    masterDetail->setStretchFactor(1, 1);
    masterDetail->setSizes({150, 90});
    partLayout->addWidget(masterDetail, 1);

    heatmapLegend_ = new QLabel(partPage_);
    heatmapLegend_->setObjectName(QStringLiteral("heatmapNumericalLegend"));
    heatmapLegend_->setWordWrap(true);
    heatmapLegend_->setStyleSheet(QStringLiteral(
        "QLabel { padding: 4px 8px; border: 1px solid #c8d4de; "
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0, "
        "stop:0 #dff4e5, stop:0.55 #fff2b8, stop:1 #ffd8d0); color:#182532; }"));
    partLayout->addWidget(heatmapLegend_);
    tabs_->addTab(partPage_, tr("So sánh Part"));

    featurePage_ = new QWidget(tabs_);
    auto* featureLayout = new QVBoxLayout(featurePage_);
    featureLayout->setContentsMargins(0, 0, 0, 0);
    featureLayout->setSpacing(4);
    featureContext_ = new QLabel(featurePage_);
    featureContext_->setObjectName(QStringLiteral("featureComparisonContext"));
    featureContext_->setWordWrap(true);
    featureContext_->setStyleSheet(QStringLiteral(
        "QLabel { padding:5px 8px; border:1px solid #c8d4de; "
        "background:#eef5fa; color:#20384c; font-weight:600; }"));
    featureLayout->addWidget(featureContext_);
    featureModel_ = new FeatureComparisonModel(featurePage_);
    featuresTable_ = new QTableView(featurePage_);
    featuresTable_->setObjectName(QStringLiteral("featureComparisonTable"));
    featuresTable_->setModel(featureModel_);
    configureTable(*featuresTable_);
    featuresTable_->setContextMenuPolicy(Qt::CustomContextMenu);
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
    tabs_->addTab(featurePage_, tr("So sánh Feature"));
    root->addWidget(tabs_);

    connect(filterCombo_, &QComboBox::currentIndexChanged, this, [this](const int index) {
        applyFilter(static_cast<ComponentFilter>(filterCombo_->itemData(index).toInt()));
    });
    connect(partsTable_->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            [this](const QModelIndex& current) {
                showPartDetails(current, false);
            });
    connect(partsTable_, &QTableView::doubleClicked, this,
            [this](const QModelIndex& index) { showPartDetails(index, true); });
    connect(occurrencesTable_->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            [this](const QModelIndex& current) { publishSelection(current, false); });
    connect(occurrencesTable_, &QTableView::doubleClicked, this,
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
    connect(partsTable_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& position) {
                const QModelIndex index = partsTable_->indexAt(position);
                if (!index.isValid()) {
                    return;
                }
                partsTable_->setCurrentIndex(index);
                QMenu menu(partsTable_);
                auto* details = menu.addAction(tr("Xem danh sách occurrence"));
                auto* isolate = menu.addAction(
                    tr("Show Only Pair — occurrence đại diện"));
                QAction* chosen =
                    menu.exec(partsTable_->viewport()->mapToGlobal(position));
                if (chosen == details) {
                    showPartDetails(index, false);
                } else if (chosen == isolate) {
                    showPartDetails(index, true);
                }
            });
    connect(occurrencesTable_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& position) {
                const QModelIndex index = occurrencesTable_->indexAt(position);
                if (!index.isValid()) {
                    return;
                }
                occurrencesTable_->setCurrentIndex(index);
                QMenu menu(occurrencesTable_);
                auto* synchronize =
                    menu.addAction(tr("Đồng bộ Tree ↔ 3D Viewer"));
                auto* isolate = menu.addAction(tr("Show Only Pair"));
                QAction* chosen = menu.exec(
                    occurrencesTable_->viewport()->mapToGlobal(position));
                if (chosen == synchronize) {
                    publishSelection(index, false);
                } else if (chosen == isolate) {
                    publishSelection(index, true);
                }
            });
    connect(featuresTable_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& position) {
                const QModelIndex index = featuresTable_->indexAt(position);
                if (!index.isValid()) {
                    return;
                }
                featuresTable_->setCurrentIndex(index);
                QMenu menu(featuresTable_);
                auto* locate =
                    menu.addAction(tr("Locate / Zoom / Highlight Feature"));
                if (menu.exec(featuresTable_->viewport()->mapToGlobal(position)) ==
                    locate) {
                    publishFeatureSelection(index, true);
                }
            });

    refreshCount();
    refreshHeatmapLegend();
}

void ComparisonResultsPanel::setReport(
    const stepcompare::reporting::Report& report) {
    parameterModel_->setReport(report);
    partModel_->setReport(report, partIdentities_);
    componentModel_->clearReport();
    featureModel_->setReport(report);
    featureContext_->setText(
        report.features.empty()
            ? tr("Không nhận diện được Feature có evidence hình học trong cặp Part đơn.")
            : tr("Part đơn A/B — Feature được so sánh tự động."));
    heatmapMaximumMm_ = report.deepDeviation.maximumMm;
    heatmapToleranceMm_ = report.tolerances.surfaceMm;
    heatmapEvidenceAvailable_ = report.deepDeviation.available;
    applyParameterSpans();
    applyFilter(partProxy_->componentFilter());
    refreshHeatmapLegend();
}

void ComparisonResultsPanel::setAssemblyFeatureDeferred() {
    featureModel_->clearReport();
    featureContext_->setText(tr(
        "Assembly — chọn một occurrence rồi bấm Show Only Pair để chỉ so sánh Feature của đúng pair A/B."));
}

void ComparisonResultsPanel::setFeaturePairLoading(
    const std::string_view stableIdA,
    const std::string_view stableIdB) {
    featureModel_->clearReport();
    featureContext_->setText(
        tr("Đang so sánh Feature của pair:\nA: %1\nB: %2")
            .arg(QString::fromUtf8(stableIdA.data(),
                                   static_cast<qsizetype>(stableIdA.size())),
                 QString::fromUtf8(stableIdB.data(),
                                   static_cast<qsizetype>(stableIdB.size()))));
}

void ComparisonResultsPanel::setFeaturePairResult(
    const std::vector<stepcompare::reporting::FeatureRow>& features,
    const std::string_view stableIdA,
    const std::string_view stableIdB) {
    featureModel_->setFeatures(features);
    featureContext_->setText(
        features.empty()
            ? tr("Pair A/B đã phân tích nhưng chưa nhận diện được Feature có evidence hình học.\nA: %1\nB: %2")
                  .arg(QString::fromUtf8(stableIdA.data(),
                                         static_cast<qsizetype>(stableIdA.size())),
                       QString::fromUtf8(stableIdB.data(),
                                         static_cast<qsizetype>(stableIdB.size())))
            : tr("Chỉ hiển thị Feature của pair đang isolate — %1 dòng. FAIL được highlight trực tiếp trong 3D Viewer.\nA: %2\nB: %3")
                  .arg(features.size())
                  .arg(QString::fromUtf8(stableIdA.data(),
                                         static_cast<qsizetype>(stableIdA.size())),
                       QString::fromUtf8(stableIdB.data(),
                                         static_cast<qsizetype>(stableIdB.size()))));
}

void ComparisonResultsPanel::setFeaturePairError(
    const std::string_view stableIdA,
    const std::string_view stableIdB) {
    featureModel_->clearReport();
    featureContext_->setText(
        tr("CHECK — không tạo được feature evidence cho pair; không suy luận PASS.\nA: %1\nB: %2")
            .arg(QString::fromUtf8(stableIdA.data(),
                                   static_cast<qsizetype>(stableIdA.size())),
                 QString::fromUtf8(stableIdB.data(),
                                   static_cast<qsizetype>(stableIdB.size()))));
}

void ComparisonResultsPanel::setPartIdentities(
    std::vector<PreviewPartIdentity> identities) {
    partIdentities_ = std::move(identities);
}

void ComparisonResultsPanel::clearReport() {
    parameterModel_->clearReport();
    partModel_->clearReport();
    componentModel_->clearReport();
    featureModel_->clearReport();
    featureContext_->setText(tr("Chưa có kết quả Feature."));
    heatmapMaximumMm_ = 0.0;
    heatmapToleranceMm_ = 0.0;
    heatmapEvidenceAvailable_ = false;
    occurrenceCount_->setText(tr("Chọn một Part để xem occurrence"));
    refreshCount();
    refreshHeatmapLegend();
}

void ComparisonResultsPanel::selectStableId(
    const stepcompare::viewer::StableSelectionId& stableId) {
    QModelIndex partSourceIndex = partModel_->indexForStableId(stableId.value());
    if (!partSourceIndex.isValid()) {
        return;
    }
    QModelIndex proxyIndex = partProxy_->mapFromSource(partSourceIndex);
    if (!proxyIndex.isValid()) {
        const QSignalBlocker blocker(filterCombo_);
        filterCombo_->setCurrentIndex(0);
        partProxy_->setComponentFilter(ComponentFilter::All);
        proxyIndex = partProxy_->mapFromSource(partSourceIndex);
        refreshCount();
    }
    if (!proxyIndex.isValid()) {
        return;
    }
    {
        const QSignalBlocker blocker(partsTable_->selectionModel());
        partsTable_->setCurrentIndex(proxyIndex);
        partsTable_->selectRow(proxyIndex.row());
        partsTable_->scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
    }
    showPartDetails(proxyIndex, false);
    const QModelIndex occurrenceIndex =
        componentModel_->indexForStableId(stableId.value());
    if (occurrenceIndex.isValid()) {
        const QSignalBlocker occurrenceBlocker(
            occurrencesTable_->selectionModel());
        occurrencesTable_->setCurrentIndex(occurrenceIndex);
        occurrencesTable_->selectRow(occurrenceIndex.row());
        occurrencesTable_->scrollTo(
            occurrenceIndex, QAbstractItemView::PositionAtCenter);
    }
    tabs_->setCurrentWidget(partPage_);

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
    partProxy_->setComponentFilter(filter);
    refreshCount();
    if (partProxy_->rowCount() > 0) {
        const QModelIndex first = partProxy_->index(0, 0);
        partsTable_->setCurrentIndex(first);
        partsTable_->selectRow(0);
        showPartDetails(first, false);
    } else {
        componentModel_->clearReport();
        occurrenceCount_->setText(tr("Không có Part phù hợp bộ lọc"));
    }
}

void ComparisonResultsPanel::showPartDetails(
    const QModelIndex& proxyIndex,
    const bool locateRepresentative) {
    if (!proxyIndex.isValid()) {
        componentModel_->clearReport();
        occurrenceCount_->setText(tr("Chọn một Part để xem occurrence"));
        return;
    }
    const QModelIndex sourceIndex = partProxy_->mapToSource(proxyIndex);
    const auto& occurrences = partModel_->occurrences(sourceIndex);
    componentModel_->setRows(occurrences);
    const QString partName =
        partModel_->data(partModel_->index(sourceIndex.row(),
                                           PartComparisonModel::Part))
            .toString();
    occurrenceCount_->setText(
        tr("Occurrence của Part: %1 — %2 dòng")
            .arg(partName)
            .arg(componentModel_->rowCount()));
    if (locateRepresentative && selectionHandler_) {
        const std::string stableId = partModel_->preferredStableId(sourceIndex);
        if (!stableId.empty()) {
            selectionHandler_(stableId, true);
        }
    }
}

void ComparisonResultsPanel::publishSelection(const QModelIndex& index,
                                              const bool locate) {
    if (!index.isValid() || !selectionHandler_) {
        return;
    }
    const std::string stableId = componentModel_->preferredStableId(index);
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
    partCount_->setText(
        tr("Hiển thị %1 / %2 Part")
            .arg(partProxy_->rowCount())
            .arg(partModel_->rowCount()));
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
