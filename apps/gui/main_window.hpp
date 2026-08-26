#pragma once

#include <QMainWindow>

#include <memory>
#include <vector>

#include <stepcompare/viewer/selection_presenter.hpp>
#include <stepcompare/viewer/viewer_state.hpp>

class QLabel;

namespace stepcompare::viewer {
class OcctViewerWidget;
}

namespace stepcompare::gui {

class ViewerActions;
class ComponentTreePanel;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void showComponentResults(
        std::vector<stepcompare::viewer::ResultRowSnapshot> rows);

private:
    void applyViewerState();

    stepcompare::viewer::ViewerStateModel viewerState_;
    stepcompare::viewer::OcctViewerWidget* viewer_{};
    ComponentTreePanel* componentTree_{};
    QLabel* coordinateBanner_{};
    std::unique_ptr<ViewerActions> actions_;
    std::unique_ptr<stepcompare::viewer::ViewerTreeSelectionPresenter>
        selectionPresenter_;
};

}  // namespace stepcompare::gui
