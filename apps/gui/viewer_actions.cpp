#include "viewer_actions.hpp"

#include <QAction>
#include <QActionGroup>
#include <QMainWindow>
#include <QComboBox>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QToolBar>

namespace stepcompare::gui {

ViewerActions::ViewerActions(QMainWindow& window,
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
                             CommandHandler resetViewHandler)
    : QObject(&window) {
    (void)orientationHandler;
    toolbar_ = window.addToolBar(QObject::tr("3D Viewer"));
    toolbar_->setObjectName(QStringLiteral("viewerToolbar"));

    auto* fileMenu = window.menuBar()->addMenu(QObject::tr("Tệp"));
    auto* newComparisonAction = new QAction(QObject::tr("So sánh mới"), this);
    newComparisonAction->setShortcut(QKeySequence::New);
    newComparisonAction->setToolTip(
        QObject::tr("Mở cửa sổ so sánh mới và giữ nguyên phiên hiện tại"));
    connect(newComparisonAction,
            &QAction::triggered,
            this,
            [handler = std::move(newComparisonHandler)] { handler(); });
    fileMenu->addAction(newComparisonAction);
    toolbar_->addAction(newComparisonAction);

    auto* openAAction = new QAction(QObject::tr("Open A"), this);
    auto* openBAction = new QAction(QObject::tr("Open B"), this);
    fileMenu->addAction(openAAction);
    fileMenu->addAction(openBAction);
    connect(openAAction,
            &QAction::triggered,
            this,
            [handler = std::move(openAHandler)] { handler(); });
    connect(openBAction,
            &QAction::triggered,
            this,
            [handler = std::move(openBHandler)] { handler(); });
    toolbar_->addAction(openAAction);
    toolbar_->addAction(openBAction);
    connect(toolbar_->addAction(QObject::tr("Compare")),
            &QAction::triggered,
            this,
            [handler = std::move(compareHandler)] { handler(); });
    connect(toolbar_->addAction(QObject::tr("Save JSON")),
            &QAction::triggered,
            this,
            [handler = std::move(saveJsonHandler)] { handler(); });
    connect(toolbar_->addAction(QObject::tr("Save CSV")),
            &QAction::triggered,
            this,
            [handler = std::move(saveCsvHandler)] { handler(); });
    heatmapAction_ = toolbar_->addAction(QObject::tr("HEATMAP"));
    heatmapAction_->setCheckable(true);
    connect(heatmapAction_,
            &QAction::toggled,
            this,
            [handler = std::move(heatmapHandler)](const bool enabled) {
                handler(enabled);
            });
    toolbar_->addSeparator();

    presentationCombo_ = new QComboBox(toolbar_);
    presentationCombo_->setObjectName(QStringLiteral("viewerPresentationMode"));
    presentationCombo_->addItem(QObject::tr("Shaded"),
                               static_cast<int>(stepcompare::viewer::PresentationMode::Shaded));
    presentationCombo_->addItem(
        QObject::tr("Shaded + Edges"),
        static_cast<int>(stepcompare::viewer::PresentationMode::ShadedWithEdges));
    presentationCombo_->addItem(
        QObject::tr("Wireframe"),
        static_cast<int>(stepcompare::viewer::PresentationMode::Wireframe));
    presentationCombo_->addItem(
        QObject::tr("Transparent/X-Ray"),
        static_cast<int>(stepcompare::viewer::PresentationMode::TransparentXRay));
    presentationCombo_->addItem(
        QObject::tr("Section View"),
        static_cast<int>(stepcompare::viewer::PresentationMode::Section));
    presentationCombo_->setCurrentIndex(1);
    toolbar_->addWidget(presentationCombo_);
    connect(presentationCombo_,
            &QComboBox::currentIndexChanged,
            this,
            [handler = std::move(presentationHandler), this](int index) {
                handler(static_cast<stepcompare::viewer::PresentationMode>(
                    presentationCombo_->itemData(index).toInt()));
            });
    toolbar_->addSeparator();

    auto* layerGroup = new QActionGroup(this);
    layerGroup->setExclusive(true);
    const std::array layerActions{
        std::pair{QObject::tr("A ONLY"), stepcompare::viewer::SceneLayer::AOnly},
        std::pair{QObject::tr("B ONLY"), stepcompare::viewer::SceneLayer::BOnly},
        std::pair{QObject::tr("OVERLAY"), stepcompare::viewer::SceneLayer::Overlay},
        std::pair{QObject::tr("DIFFERENCE"), stepcompare::viewer::SceneLayer::Difference},
    };
    std::size_t layerIndex = 0;
    for (const auto& [label, layer] : layerActions) {
        auto* action = toolbar_->addAction(label);
        action->setCheckable(true);
        action->setChecked(layer == stepcompare::viewer::SceneLayer::Overlay);
        layerGroup->addAction(action);
        layerActions_[layerIndex++] = action;
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
    connect(toolbar_->addAction(QObject::tr("Fit All")),
            &QAction::triggered,
            this,
            [handler = std::move(fitAllHandler)] { handler(); });
    connect(toolbar_->addAction(QObject::tr("Reset View")),
            &QAction::triggered,
            this,
            [handler = std::move(resetViewHandler)] { handler(); });
}

void ViewerActions::setPresentationMode(
    const stepcompare::viewer::PresentationMode mode) {
    const QSignalBlocker blocker(presentationCombo_);
    const int index = presentationCombo_->findData(static_cast<int>(mode));
    if (index >= 0) {
        presentationCombo_->setCurrentIndex(index);
    }
}

void ViewerActions::setLayer(const stepcompare::viewer::SceneLayer layer) {
    const auto index = static_cast<std::size_t>(layer);
    if (index < layerActions_.size() && layerActions_[index] != nullptr) {
        const QSignalBlocker blocker(layerActions_[index]);
        layerActions_[index]->setChecked(true);
    }
}

void ViewerActions::setHeatmapEnabled(const bool enabled) {
    const QSignalBlocker blocker(heatmapAction_);
    heatmapAction_->setChecked(enabled);
}

}  // namespace stepcompare::gui
