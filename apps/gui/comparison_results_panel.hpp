#pragma once

#include "comparison_readability_model.hpp"

#include <QWidget>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <stepcompare/reporting/report.hpp>
#include <stepcompare/viewer/viewer_state.hpp>

class QComboBox;
class QLabel;
class QTableView;
class QTabWidget;

namespace stepcompare::gui {

class ComparisonResultsPanel final : public QWidget {
public:
    using SelectionHandler = std::function<void(std::string, bool)>;
    using FeatureSelectionHandler =
        std::function<void(std::string, std::vector<std::uint32_t>, bool)>;

    explicit ComparisonResultsPanel(QWidget* parent = nullptr);

    void setReport(const stepcompare::reporting::Report& report);
    void setAssemblyFeatureDeferred();
    void setFeaturePairLoading(std::string_view stableIdA,
                               std::string_view stableIdB);
    void setFeaturePairResult(
        const std::vector<stepcompare::reporting::FeatureRow>& features,
        std::string_view stableIdA,
        std::string_view stableIdB);
    void setFeaturePairError(std::string_view stableIdA,
                             std::string_view stableIdB);
    void setPartIdentities(std::vector<PreviewPartIdentity> identities);
    void clearReport();
    void selectStableId(const stepcompare::viewer::StableSelectionId& stableId);
    void setSelectionHandler(SelectionHandler handler);
    void setFeatureSelectionHandler(FeatureSelectionHandler handler);
    void setHeatmapState(bool enabled, bool evidenceAvailable);

private:
    void applyParameterSpans();
    void applyFilter(ComponentFilter filter);
    void showPartDetails(const QModelIndex& proxyIndex, bool locateRepresentative);
    void publishSelection(const QModelIndex& index, bool locate);
    void publishFeatureSelection(const QModelIndex& index, bool locate);
    void refreshCount();
    void refreshHeatmapLegend();

    QTabWidget* tabs_{};
    QWidget* partPage_{};
    QWidget* featurePage_{};
    QTableView* parametersTable_{};
    QTableView* partsTable_{};
    QTableView* occurrencesTable_{};
    QTableView* featuresTable_{};
    QComboBox* filterCombo_{};
    QLabel* partCount_{};
    QLabel* occurrenceCount_{};
    QLabel* heatmapLegend_{};
    QLabel* featureContext_{};
    ComparisonParameterModel* parameterModel_{};
    PartComparisonModel* partModel_{};
    ComponentFilterProxyModel* partProxy_{};
    ComponentComparisonModel* componentModel_{};
    FeatureComparisonModel* featureModel_{};
    SelectionHandler selectionHandler_{};
    FeatureSelectionHandler featureSelectionHandler_{};
    double heatmapMaximumMm_{};
    double heatmapToleranceMm_{};
    bool heatmapEvidenceAvailable_{};
    bool heatmapEnabled_{};
    std::vector<PreviewPartIdentity> partIdentities_{};
};

}  // namespace stepcompare::gui
