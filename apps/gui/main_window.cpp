#include "main_window.hpp"

#include "component_tree_panel.hpp"
#include "viewer_actions.hpp"

#include <stepcompare/viewer/occt_viewer_widget.hpp>

#include <QLabel>
#include <QStatusBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>
#include <vector>

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
    componentTree_ = new ComponentTreePanel(central);
    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->addWidget(componentTree_);
    splitter->addWidget(viewer_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 960});
    layout->addWidget(splitter, 1);
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
    selectionPresenter_ =
        std::make_unique<stepcompare::viewer::ViewerTreeSelectionPresenter>(
            [this](const auto& stableId) { componentTree_->selectStableId(stableId); },
            [this](const auto& request) {
                if (request.highlightSelection) {
                    viewer_->selectStableId(request.stableId, request.fitSelection);
                }
            });
    componentTree_->setSelectionHandler([this](std::string stableId) {
        selectionPresenter_->onRowSelection(stableId);
    });
    viewer_->setSelectionChangedHandler([this](std::string stableId) {
        selectionPresenter_->onViewerSelection(stableId);
        statusBar()->showMessage(
            tr("Selected: %1").arg(QString::fromStdString(stableId)));
    });

    statusBar()->showMessage(tr("Ready — drag: rotate/pan/zoom; wheel: zoom"));
    applyViewerState();
}

MainWindow::~MainWindow() = default;

void MainWindow::showComponentResults(
    std::vector<stepcompare::viewer::ResultRowSnapshot> rows) {
    selectionPresenter_->publishRows(std::move(rows));
    componentTree_->setRows(selectionPresenter_->rows());
    std::vector<stepcompare::viewer::StableSelectionId> changedStableIds;
    changedStableIds.reserve(selectionPresenter_->rows().size());
    for (const auto& row : selectionPresenter_->rows()) {
        if (stepcompare::viewer::isChanged(row.change)) {
            changedStableIds.push_back(row.stableId);
        }
    }
    viewer_->setDifferenceStates(changedStableIds);
}

void MainWindow::applyViewerState() {
    coordinateBanner_->setText(QString::fromLatin1(viewerState_.coordinateBanner().data(),
                                                    viewerState_.coordinateBanner().size()));
    viewer_->applyState(viewerState_);
}

}  // namespace stepcompare::gui
