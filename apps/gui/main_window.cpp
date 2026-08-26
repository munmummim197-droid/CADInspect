#include "main_window.hpp"

#include "viewer_actions.hpp"

#include <stepcompare/viewer/occt_viewer_widget.hpp>

#include <QLabel>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace stepcompare::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("StepCompare DEV V1"));
    resize(1280, 800);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    coordinateBanner_ = new QLabel(central);
    coordinateBanner_->setAlignment(Qt::AlignCenter);
    coordinateBanner_->setMinimumHeight(32);
    coordinateBanner_->setStyleSheet(
        QStringLiteral("QLabel { background: #18324a; color: white; font-weight: 700; }"));
    viewer_ = new stepcompare::viewer::OcctViewerWidget(central);
    layout->addWidget(coordinateBanner_);
    layout->addWidget(viewer_, 1);
    setCentralWidget(central);

    actions_ = std::make_unique<ViewerActions>(
        *this,
        [this](const auto layer) {
            viewerState_.setLayer(layer);
            applyViewerState();
        },
        [this](const auto coordinates) {
                viewerState_.setCoordinates(coordinates);
                applyViewerState();
        },
        [this](const auto orientation) { viewer_->setCameraOrientation(orientation); },
        [this] { viewer_->fitAll(); },
        [this] { viewer_->resetView(); });
    viewer_->setSelectionChangedHandler([this](std::string stableId) {
        statusBar()->showMessage(
            tr("Selected: %1").arg(QString::fromStdString(stableId)));
    });

    statusBar()->showMessage(tr("Ready — drag: rotate/pan/zoom; wheel: zoom"));
    applyViewerState();
}

MainWindow::~MainWindow() = default;

void MainWindow::applyViewerState() {
    coordinateBanner_->setText(QString::fromLatin1(viewerState_.coordinateBanner().data(),
                                                    viewerState_.coordinateBanner().size()));
    viewer_->applyState(viewerState_);
}

}  // namespace stepcompare::gui
