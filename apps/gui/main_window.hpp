#pragma once

#include <QMainWindow>

#include <memory>

#include <stepcompare/viewer/viewer_state.hpp>

class QLabel;

namespace stepcompare::viewer {
class OcctViewerWidget;
}

namespace stepcompare::gui {

class ViewerActions;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void applyViewerState();

    stepcompare::viewer::ViewerStateModel viewerState_;
    stepcompare::viewer::OcctViewerWidget* viewer_{};
    QLabel* coordinateBanner_{};
    std::unique_ptr<ViewerActions> actions_;
};

}  // namespace stepcompare::gui
