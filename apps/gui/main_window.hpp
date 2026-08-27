#pragma once

#include "comparison_readability_model.hpp"

#include <QMainWindow>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>
#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <stepcompare/application/comparison_coordinator.hpp>
#include <stepcompare/viewer/selection_presenter.hpp>
#include <stepcompare/viewer/viewer_state.hpp>

class QLabel;
class QDragEnterEvent;
class QDropEvent;
class QPoint;
class QShowEvent;

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
class SectionViewControls;
class ViewerControlBar;
enum class IsolationCommand;
struct PreviewJobResult;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void showComponentResults(
        std::vector<stepcompare::viewer::ResultRowSnapshot> rows);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void createNewComparison(const QStringList& droppedFiles = {});
    void openDroppedStepFiles(const QStringList& paths);
    bool openStepPath(stepcompare::viewer::ModelSide side,
                      const QString& fileName);
    void showViewerContextMenu(const QPoint& position);
    void applyViewerState();
    void setPresentationMode(stepcompare::viewer::PresentationMode mode);
    void setSceneLayer(stepcompare::viewer::SceneLayer layer);
    void setHeatmapEnabled(bool enabled);
    void applyIsolationCommand(IsolationCommand command);
    void restoreAssembly(bool notify = true);
    void openStep(stepcompare::viewer::ModelSide side);
    void acceptPreviewResult(PreviewJobResult result);
    void refreshPreviewRows();
    void startComparison();
    void acceptComparisonResult(
        stepcompare::application::ComparisonResult result);
    void startFeaturePairComparison(std::string stableIdA,
                                    std::string stableIdB);
    void acceptFeaturePairResult(
        stepcompare::application::FeaturePairComparisonResult result);
    void applyChangedFeatureHighlights(
        const std::vector<stepcompare::reporting::FeatureRow>& features);
    void saveCanonicalReport(bool json);
    void applyCanonicalRowsAndHeatmap();

    stepcompare::viewer::ViewerStateModel viewerState_;
    stepcompare::viewer::OcctViewerWidget* viewer_{};
    ComponentTreePanel* componentTree_{};
    ComparisonResultsPanel* comparisonResults_{};
    SectionViewControls* sectionControls_{};
    ViewerControlBar* viewerControls_{};
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
    std::vector<PreviewPartIdentity> previewPartIdentitiesA_;
    std::vector<PreviewPartIdentity> previewPartIdentitiesB_;
    std::unordered_map<std::string, std::array<double, 16>> occurrenceTransforms_;
    std::optional<std::string> pendingManualPairA_{};
    std::optional<std::pair<std::string, std::string>> activeFeaturePair_{};
    std::unordered_map<std::string,
                       std::vector<stepcompare::reporting::FeatureRow>>
        featurePairCache_{};
    std::u8string inputAUtf8_;
    std::u8string inputBUtf8_;
    QString pendingDroppedBPath_{};
    bool inputAIsSinglePart_{};
    bool inputBIsSinglePart_{};
    std::optional<stepcompare::application::ComparisonResult> comparisonResult_;
};

}  // namespace stepcompare::gui
