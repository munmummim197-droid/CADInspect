#include "viewer_actions.hpp"

#include <QAction>
#include <QActionGroup>
#include <QMainWindow>
#include <QToolBar>

namespace stepcompare::gui {

ViewerActions::ViewerActions(QMainWindow& window,
                             LayerHandler layerHandler,
                             CoordinatesHandler coordinatesHandler,
                             OrientationHandler orientationHandler,
                             CommandHandler fitAllHandler,
                             CommandHandler resetViewHandler)
    : QObject(&window) {
    toolbar_ = window.addToolBar(QObject::tr("3D Viewer"));
    toolbar_->setObjectName(QStringLiteral("viewerToolbar"));

    auto* layerGroup = new QActionGroup(this);
    layerGroup->setExclusive(true);
    const std::array layerActions{
        std::pair{QObject::tr("A ONLY"), stepcompare::viewer::SceneLayer::AOnly},
        std::pair{QObject::tr("B ONLY"), stepcompare::viewer::SceneLayer::BOnly},
        std::pair{QObject::tr("OVERLAY"), stepcompare::viewer::SceneLayer::Overlay},
        std::pair{QObject::tr("DIFFERENCE"), stepcompare::viewer::SceneLayer::Difference},
    };
    for (const auto& [label, layer] : layerActions) {
        auto* action = toolbar_->addAction(label);
        action->setCheckable(true);
        action->setChecked(layer == stepcompare::viewer::SceneLayer::Overlay);
        layerGroup->addAction(action);
        connect(action,
                &QAction::triggered,
                this,
                [handler = layerHandler, layer] { handler(layer); });
    }

    toolbar_->addSeparator();
    auto* coordinatesGroup = new QActionGroup(this);
    coordinatesGroup->setExclusive(true);
    const std::array coordinateActions{
        std::pair{QObject::tr("ABSOLUTE"), stepcompare::viewer::CoordinateMode::Absolute},
        std::pair{QObject::tr("ALIGNED"), stepcompare::viewer::CoordinateMode::Aligned},
    };
    for (const auto& [label, coordinates] : coordinateActions) {
        auto* action = toolbar_->addAction(label);
        action->setCheckable(true);
        action->setChecked(coordinates == stepcompare::viewer::CoordinateMode::Absolute);
        coordinatesGroup->addAction(action);
        connect(action, &QAction::triggered, this, [handler = coordinatesHandler, coordinates] {
            handler(coordinates);
        });
    }

    toolbar_->addSeparator();
    const std::array orientationActions{
        std::pair{QObject::tr("Front"), stepcompare::viewer::CameraOrientation::Front},
        std::pair{QObject::tr("Back"), stepcompare::viewer::CameraOrientation::Back},
        std::pair{QObject::tr("Left"), stepcompare::viewer::CameraOrientation::Left},
        std::pair{QObject::tr("Right"), stepcompare::viewer::CameraOrientation::Right},
        std::pair{QObject::tr("Top"), stepcompare::viewer::CameraOrientation::Top},
        std::pair{QObject::tr("Bottom"), stepcompare::viewer::CameraOrientation::Bottom},
        std::pair{QObject::tr("Isometric"), stepcompare::viewer::CameraOrientation::Isometric},
    };
    for (const auto& [label, orientation] : orientationActions) {
        auto* action = toolbar_->addAction(label);
        connect(action, &QAction::triggered, this, [handler = orientationHandler, orientation] {
            handler(orientation);
        });
    }

    toolbar_->addSeparator();
    connect(toolbar_->addAction(QObject::tr("Fit All")),
            &QAction::triggered,
            this,
            [handler = std::move(fitAllHandler)] { handler(); });
    connect(toolbar_->addAction(QObject::tr("Reset View")),
            &QAction::triggered,
            this,
            [handler = std::move(resetViewHandler)] { handler(); });
}

}  // namespace stepcompare::gui
