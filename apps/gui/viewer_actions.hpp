#pragma once

#include <QObject>

#include <array>
#include <functional>

#include <stepcompare/viewer/viewer_state.hpp>

class QAction;
class QComboBox;
class QMainWindow;
class QToolBar;

namespace stepcompare::gui {

class ViewerActions final : public QObject {
public:
    using LayerHandler =
        std::function<void(stepcompare::viewer::SceneLayer)>;
    using CoordinatesHandler =
        std::function<void(stepcompare::viewer::CoordinateMode)>;
    using PresentationHandler =
        std::function<void(stepcompare::viewer::PresentationMode)>;
    using OrientationHandler =
        std::function<void(stepcompare::viewer::CameraOrientation)>;
    using CommandHandler = std::function<void()>;

    ViewerActions(QMainWindow& window,
                  CommandHandler newComparisonHandler,
                  CommandHandler openAHandler,
                  CommandHandler openBHandler,
                  CommandHandler compareHandler,
                  CommandHandler saveJsonHandler,
                  CommandHandler saveCsvHandler,
                  std::function<void(bool)> heatmapHandler,
                  PresentationHandler presentationHandler,
                  LayerHandler layerHandler,
                  CoordinatesHandler coordinatesHandler,
                  OrientationHandler orientationHandler,
                  CommandHandler fitAllHandler,
                  CommandHandler resetViewHandler);

    void setPresentationMode(stepcompare::viewer::PresentationMode mode);
    void setLayer(stepcompare::viewer::SceneLayer layer);
    void setHeatmapEnabled(bool enabled);

private:
    QToolBar* toolbar_{};
    QComboBox* presentationCombo_{};
    QAction* heatmapAction_{};
    std::array<QAction*, 4> layerActions_{};
};

}  // namespace stepcompare::gui
