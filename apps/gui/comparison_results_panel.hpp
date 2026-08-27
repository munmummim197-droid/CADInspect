#pragma once

#include "comparison_readability_model.hpp"

#include <QWidget>

#include <functional>
#include <string>
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
    void clearReport();
    void selectStableId(const stepcompare::viewer::StableSelectionId& stableId);
    void setSelectionHandler(SelectionHandler handler);
    void setFeatureSelectionHandler(FeatureSelectionHandler handler);
    void setHeatmapState(bool enabled, bool evidenceAvailable);

private:
    void applyParameterSpans();
    void applyFilter(ComponentFilter filter);
    void publishSelection(const QModelIndex& proxyIndex, bool locate);
    void publishFeatureSelection(const QModelIndex& index, bool locate);
    void refreshCount();
    void refreshHeatmapLegend();

    QTabWidget* tabs_{};
    QWidget* componentPage_{};
    QWidget* featurePage_{};
    QTableView* parametersTable_{};
    QTableView* componentsTable_{};
    QTableView* featuresTable_{};
    QComboBox* filterCombo_{};
    QLabel* componentCount_{};
    QLabel* heatmapLegend_{};
    ComparisonParameterModel* parameterModel_{};
    ComponentComparisonModel* componentModel_{};
    ComponentFilterProxyModel* componentProxy_{};
    FeatureComparisonModel* featureModel_{};
    SelectionHandler selectionHandler_{};
    FeatureSelectionHandler featureSelectionHandler_{};
    double heatmapMaximumMm_{};
    double heatmapToleranceMm_{};
    bool heatmapEvidenceAvailable_{};
    bool heatmapEnabled_{};
};

}  // namespace stepcompare::gui
