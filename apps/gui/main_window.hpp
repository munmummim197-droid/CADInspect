#pragma once

#include <QMainWindow>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <stepcompare/application/comparison_coordinator.hpp>
#include <stepcompare/viewer/selection_presenter.hpp>
#include <stepcompare/viewer/viewer_state.hpp>

class QLabel;

namespace stepcompare::viewer {
class OcctViewerWidget;
}

namespace stepcompare::gui {

class ViewerActions;
class ComponentTreePanel;
class PreviewStatusWidget;
class StepPreviewLoader;
class StepPreviewSceneAdapter;
class ComparisonRunner;
class ComparisonResultsPanel;
struct PreviewJobResult;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void showComponentResults(
        std::vector<stepcompare::viewer::ResultRowSnapshot> rows);

private:
    void applyViewerState();
    void openStep(stepcompare::viewer::ModelSide side);
    void acceptPreviewResult(PreviewJobResult result);
    void refreshPreviewRows();
    void startComparison();
    void acceptComparisonResult(
        stepcompare::application::ComparisonResult result);
    void saveCanonicalReport(bool json);
    void applyCanonicalRowsAndHeatmap();

    stepcompare::viewer::ViewerStateModel viewerState_;
    stepcompare::viewer::OcctViewerWidget* viewer_{};
    ComponentTreePanel* componentTree_{};
    ComparisonResultsPanel* comparisonResults_{};
    QLabel* coordinateBanner_{};
    QLabel* comparisonSummary_{};
    PreviewStatusWidget* previewStatus_{};
    std::unique_ptr<ViewerActions> actions_;
    std::unique_ptr<StepPreviewLoader> previewLoader_;
    std::unique_ptr<ComparisonRunner> comparisonRunner_;
    std::unique_ptr<StepPreviewSceneAdapter> previewSceneAdapter_;
    std::unique_ptr<stepcompare::viewer::ViewerTreeSelectionPresenter>
        selectionPresenter_;
    std::vector<stepcompare::viewer::ResultRowSnapshot> previewRowsA_;
    std::vector<stepcompare::viewer::ResultRowSnapshot> previewRowsB_;
    std::u8string inputAUtf8_;
    std::u8string inputBUtf8_;
    std::optional<stepcompare::application::ComparisonResult> comparisonResult_;
};

}  // namespace stepcompare::gui
