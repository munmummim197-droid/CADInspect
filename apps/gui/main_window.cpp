#include "main_window.hpp"

#include "component_tree_panel.hpp"
#include "preview_status_widget.hpp"
#include "step_preview_loader.hpp"
#include "step_preview_scene_adapter.hpp"
#include "viewer_actions.hpp"

#include <stepcompare/viewer/occt_viewer_widget.hpp>

#include <QFileDialog>
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
    layout->addWidget(coordinateBanner_);
    previewStatus_ = new PreviewStatusWidget(
        [this] {
            if (previewLoader_) {
                static_cast<void>(previewLoader_->cancel());
            }
        },
        central);
    layout->addWidget(previewStatus_);
    auto* splitter = new QSplitter(Qt::Horizontal, central);
    // OcctViewerWidget owns a native HWND. Construct it with its final native
    // parent so OCCT does not retain geometry from the pre-splitter parent.
    componentTree_ = new ComponentTreePanel(splitter);
    viewer_ = new stepcompare::viewer::OcctViewerWidget(splitter);
    splitter->addWidget(componentTree_);
    splitter->addWidget(viewer_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 960});
    layout->addWidget(splitter, 1);
    setCentralWidget(central);

    actions_ = std::make_unique<ViewerActions>(
        *this,
        [this] { openStep(stepcompare::viewer::ModelSide::A); },
        [this] { openStep(stepcompare::viewer::ModelSide::B); },
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
    previewSceneAdapter_ = std::make_unique<StepPreviewSceneAdapter>();
    previewLoader_ = std::make_unique<StepPreviewLoader>(
        [this](const auto& status) {
            previewStatus_->setStatus(status);
            statusBar()->showMessage(QString::fromUtf8(status.messageUtf8));
        },
        [this](PreviewJobResult result) { acceptPreviewResult(std::move(result)); },
        this);
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

void MainWindow::openStep(const stepcompare::viewer::ModelSide side) {
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        side == stepcompare::viewer::ModelSide::A ? tr("Open STEP File A")
                                                  : tr("Open STEP File B"),
        {},
        tr("STEP files (*.step *.stp);;All files (*)"));
    if (fileName.isEmpty()) {
        return;
    }
    const QByteArray utf8 = fileName.toUtf8();
    std::u8string sourcePath(
        reinterpret_cast<const char8_t*>(utf8.constData()),
        reinterpret_cast<const char8_t*>(utf8.constData() + utf8.size()));
    if (!previewLoader_->start(side, std::move(sourcePath))) {
        statusBar()->showMessage(tr("Another STEP import is still running"));
    }
}

void MainWindow::acceptPreviewResult(PreviewJobResult result) {
    auto rows = previewSceneAdapter_->display(result.importResult.model,
                                              result.side,
                                              *viewer_);
    if (result.side == stepcompare::viewer::ModelSide::A) {
        previewRowsA_ = std::move(rows);
    } else {
        previewRowsB_ = std::move(rows);
    }
    refreshPreviewRows();
}

void MainWindow::refreshPreviewRows() {
    std::vector<stepcompare::viewer::ResultRowSnapshot> combined;
    combined.reserve(previewRowsA_.size() + previewRowsB_.size());
    combined.insert(combined.end(), previewRowsA_.begin(), previewRowsA_.end());
    combined.insert(combined.end(), previewRowsB_.begin(), previewRowsB_.end());
    showComponentResults(std::move(combined));
}

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
