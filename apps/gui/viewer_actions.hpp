#pragma once

#include <QObject>

#include <array>
#include <functional>

#include <stepcompare/viewer/viewer_state.hpp>

class QAction;
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

private:
    QToolBar* toolbar_{};
};

}  // namespace stepcompare::gui
