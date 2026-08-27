#include "viewer_control_bar.hpp"

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolButton>

#include <array>
#include <utility>

namespace stepcompare::gui {
namespace {

QToolButton* menuButton(const QString& text,
                        const QString& objectName,
                        QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setText(text);
    button->setPopupMode(QToolButton::InstantPopup);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setArrowType(Qt::DownArrow);
    button->setMinimumHeight(28);
    return button;
}

}  // namespace

ViewerControlBar::ViewerControlBar(PresentationHandler presentationHandler,
                                   LayerHandler layerHandler,
                                   HeatmapHandler heatmapHandler,
                                   IsolationHandler isolationHandler,
                                   CommandHandler fitAllHandler,
                                   QWidget* parent)
    : QFrame(parent) {
    setObjectName(QStringLiteral("viewerControlBar"));
    setStyleSheet(QStringLiteral(
        "QFrame#viewerControlBar { background:#e8eef3; border-bottom:1px solid #aebdca; }"
        "QToolButton { color:#17324a; background:#f8fafc; border:1px solid #a9b9c7; "
        "border-radius:4px; padding:3px 9px; font-weight:600; }"
        "QToolButton:hover { background:#ffffff; border-color:#4e87b0; }"
        "QToolButton:pressed { background:#d9e8f2; }"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(7, 4, 7, 4);
    layout->setSpacing(6);

    auto* viewButton = menuButton(tr("View"), QStringLiteral("viewerViewMenu"), this);
    viewButton->setAccessibleName(tr("View — chọn chế độ hiển thị 3D"));
    auto* viewMenu = new QMenu(viewButton);
    auto* presentationGroup = new QActionGroup(viewMenu);
    presentationGroup->setExclusive(true);
    const auto addPresentation = [&](const QString& label,
                                     const stepcompare::viewer::PresentationMode mode) {
        auto* action = viewMenu->addAction(label);
        action->setCheckable(true);
        presentationGroup->addAction(action);
        connect(action, &QAction::triggered, this,
                [handler = presentationHandler, mode] { handler(mode); });
        return action;
    };
    shadedAction_ = addPresentation(
        tr("Shaded"), stepcompare::viewer::PresentationMode::Shaded);
    shadedEdgesAction_ = addPresentation(
        tr("Shaded + Edges"),
        stepcompare::viewer::PresentationMode::ShadedWithEdges);
    wireframeAction_ = addPresentation(
        tr("Wireframe"), stepcompare::viewer::PresentationMode::Wireframe);
    transparentAction_ = addPresentation(
        tr("Transparent / X-Ray"),
        stepcompare::viewer::PresentationMode::TransparentXRay);
    sectionAction_ = addPresentation(
        tr("Section View"), stepcompare::viewer::PresentationMode::Section);
    viewMenu->addSeparator();
    differenceAction_ = viewMenu->addAction(tr("Difference"));
    differenceAction_->setCheckable(true);
    connect(differenceAction_, &QAction::triggered, this,
            [handler = layerHandler] { handler(stepcompare::viewer::SceneLayer::Difference); });
    heatmapAction_ = viewMenu->addAction(tr("Heatmap"));
    heatmapAction_->setCheckable(true);
    connect(heatmapAction_, &QAction::toggled, this,
            [handler = std::move(heatmapHandler)](const bool enabled) {
                handler(enabled);
            });
    viewButton->setMenu(viewMenu);
    layout->addWidget(viewButton);

    auto* isolateButton = menuButton(
        tr("Isolate"), QStringLiteral("viewerIsolationMenu"), this);
    isolateButton->setAccessibleName(tr("Isolate — cô lập occurrence trong assembly"));
    auto* isolateMenu = new QMenu(isolateButton);
    const std::array isolationActions{
        std::pair{tr("Show Only A"), IsolationCommand::ShowOnlyA},
        std::pair{tr("Show Only B"), IsolationCommand::ShowOnlyB},
        std::pair{tr("Show Only Pair"), IsolationCommand::ShowOnlyPair},
    };
    for (const auto& [label, command] : isolationActions) {
        auto* action = isolateMenu->addAction(label);
        connect(action, &QAction::triggered, this,
                [handler = isolationHandler, command] { handler(command); });
    }
    isolateMenu->addSeparator();
    restoreAction_ = isolateMenu->addAction(tr("Show All / Restore Assembly"));
    connect(restoreAction_, &QAction::triggered, this,
            [handler = std::move(isolationHandler)] {
                handler(IsolationCommand::RestoreAssembly);
            });
    isolateButton->setMenu(isolateMenu);
    layout->addWidget(isolateButton);

    auto* fitButton = new QToolButton(this);
    fitButton->setObjectName(QStringLiteral("viewerFitAllButton"));
    fitButton->setText(tr("Fit All"));
    fitButton->setToolTip(tr("Căn vừa toàn bộ hình đang hiển thị"));
    connect(fitButton, &QToolButton::clicked, this,
            [handler = std::move(fitAllHandler)] { handler(); });
    layout->addWidget(fitButton);
    layout->addStretch(1);

    auto* cubeHint = new QLabel(tr("Orientation cube: TOP · FRONT · RIGHT"), this);
    cubeHint->setObjectName(QStringLiteral("orientationCubeHint"));
    cubeHint->setStyleSheet(QStringLiteral("color:#52697b; padding-right:8px;"));
    layout->addWidget(cubeHint);

    setPresentationMode(stepcompare::viewer::PresentationMode::ShadedWithEdges);
    setLayer(stepcompare::viewer::SceneLayer::Overlay);
    setHeatmapEnabled(false);
    setIsolationActive(false);
}

void ViewerControlBar::setPresentationMode(
    const stepcompare::viewer::PresentationMode mode) {
    QAction* target = nullptr;
    switch (mode) {
        case stepcompare::viewer::PresentationMode::Shaded:
            target = shadedAction_;
            break;
        case stepcompare::viewer::PresentationMode::ShadedWithEdges:
            target = shadedEdgesAction_;
            break;
        case stepcompare::viewer::PresentationMode::Wireframe:
            target = wireframeAction_;
            break;
        case stepcompare::viewer::PresentationMode::TransparentXRay:
            target = transparentAction_;
            break;
        case stepcompare::viewer::PresentationMode::Section:
            target = sectionAction_;
            break;
    }
    if (target != nullptr) {
        const QSignalBlocker blocker(target);
        target->setChecked(true);
    }
}

void ViewerControlBar::setLayer(const stepcompare::viewer::SceneLayer layer) {
    const QSignalBlocker blocker(differenceAction_);
    differenceAction_->setChecked(layer == stepcompare::viewer::SceneLayer::Difference);
}

void ViewerControlBar::setHeatmapEnabled(const bool enabled) {
    const QSignalBlocker blocker(heatmapAction_);
    heatmapAction_->setChecked(enabled);
}

void ViewerControlBar::setIsolationActive(const bool active) {
    restoreAction_->setEnabled(active);
}

}  // namespace stepcompare::gui
