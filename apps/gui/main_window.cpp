#include "main_window.hpp"

#include "component_tree_panel.hpp"
#include "comparison_runner.hpp"
#include "comparison_readability_model.hpp"
#include "comparison_results_panel.hpp"
#include "preview_status_widget.hpp"
#include "step_preview_loader.hpp"
#include "step_preview_scene_adapter.hpp"
#include "viewer_actions.hpp"

#include <stepcompare/viewer/occt_viewer_widget.hpp>
#include <stepcompare/reporting/writers.hpp>

#include <QFileDialog>
#include <QLabel>
#include <QStringList>
#include <QStatusBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace stepcompare::gui {
namespace {

std::string previewStableId(const stepcompare::viewer::ModelSide side,
                            const std::string& nodeId) {
    return side == stepcompare::viewer::ModelSide::A
               ? "preview/A/" + nodeId
               : "preview/B/" + nodeId;
}

QString summaryStyle(const OverallDisplayKind kind) {
    switch (kind) {
        case OverallDisplayKind::Same:
            return QStringLiteral(
                "QLabel { background:#e2f4e8; color:#155d34; font-weight:700; "
                "padding:6px; border-bottom:2px solid #4a9b69; }");
        case OverallDisplayKind::SameGeometryDifferentPosition:
        case OverallDisplayKind::GeometryChanged:
            return QStringLiteral(
                "QLabel { background:#fff0ec; color:#8f291b; font-weight:700; "
                "padding:6px; border-bottom:2px solid #c95c49; }");
        case OverallDisplayKind::Ambiguous:
            return QStringLiteral(
                "QLabel { background:#fff6d9; color:#795000; font-weight:700; "
                "padding:6px; border-bottom:2px solid #c4931f; }");
        case OverallDisplayKind::Error:
            return QStringLiteral(
                "QLabel { background:#f5e7ea; color:#7e1529; font-weight:700; "
                "padding:6px; border-bottom:2px solid #a92943; }");
    }
    return {};
}

}  // namespace

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
    comparisonSummary_ = new QLabel(tr("CHƯA CÓ KẾT QUẢ SO SÁNH"), central);
    comparisonSummary_->setAlignment(Qt::AlignCenter);
    comparisonSummary_->setMinimumHeight(42);
    comparisonSummary_->setWordWrap(true);
    comparisonSummary_->setStyleSheet(QStringLiteral(
        "QLabel { background: #eef3f7; color: #182532; font-weight: 600; }"));
    layout->addWidget(comparisonSummary_);
    previewStatus_ = new PreviewStatusWidget(
        [this] {
            if (comparisonRunner_ && comparisonRunner_->busy()) {
                static_cast<void>(comparisonRunner_->cancel());
            } else if (previewLoader_) {
                static_cast<void>(previewLoader_->cancel());
            }
        },
        central);
    layout->addWidget(previewStatus_);
    auto* workspaceSplitter = new QSplitter(Qt::Vertical, central);
    auto* splitter = new QSplitter(Qt::Horizontal, workspaceSplitter);
    // OcctViewerWidget owns a native HWND. Construct it with its final native
    // parent so OCCT does not retain geometry from the pre-splitter parent.
    componentTree_ = new ComponentTreePanel(splitter);
    viewer_ = new stepcompare::viewer::OcctViewerWidget(splitter);
    splitter->addWidget(componentTree_);
    splitter->addWidget(viewer_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 960});
    comparisonResults_ = new ComparisonResultsPanel(workspaceSplitter);
    workspaceSplitter->addWidget(splitter);
    workspaceSplitter->addWidget(comparisonResults_);
    workspaceSplitter->setStretchFactor(0, 3);
    workspaceSplitter->setStretchFactor(1, 2);
    workspaceSplitter->setSizes({470, 300});
    layout->addWidget(workspaceSplitter, 1);
    setCentralWidget(central);

    actions_ = std::make_unique<ViewerActions>(
        *this,
        [this] { openStep(stepcompare::viewer::ModelSide::A); },
        [this] { openStep(stepcompare::viewer::ModelSide::B); },
        [this] { startComparison(); },
        [this] { saveCanonicalReport(true); },
        [this] { saveCanonicalReport(false); },
        [this](const bool enabled) {
            viewer_->setDeviationColoringEnabled(enabled);
            comparisonResults_->setHeatmapState(
                viewer_->deviationColoringEnabled(),
                comparisonResult_ && comparisonResult_->report.deepDeviation.available);
            statusBar()->showMessage(
                enabled && !viewer_->deviationColoringEnabled()
                    ? tr("Heatmap unavailable: no validated deviation evidence")
                    : enabled ? tr("Heatmap enabled") : tr("Heatmap disabled"));
        },
        [this](const auto presentation) {
            viewerState_.setPresentationMode(presentation);
            applyViewerState();
            statusBar()->showMessage(
                tr("Chế độ hiển thị: %1")
                    .arg(QString::fromLatin1(
                        stepcompare::viewer::toString(presentation).data())));
        },
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
    comparisonRunner_ = std::make_unique<ComparisonRunner>(
        [this](const int percent, std::string message) {
            previewStatus_->setOperationStatus(
                QString::fromStdString(std::move(message)), percent, percent < 100);
        },
        [this](stepcompare::application::ComparisonResult result) {
            acceptComparisonResult(std::move(result));
        },
        this);
    selectionPresenter_ =
        std::make_unique<stepcompare::viewer::ViewerTreeSelectionPresenter>(
            [this](const auto& stableId) {
                componentTree_->selectStableId(stableId);
                comparisonResults_->selectStableId(stableId);
            },
            [this](const auto& request) {
                if (request.highlightSelection) {
                    viewer_->selectStableId(request.stableId, request.fitSelection);
                }
            });
    componentTree_->setSelectionHandler([this](std::string stableId) {
        comparisonResults_->selectStableId(
            stepcompare::viewer::StableSelectionId{stableId});
        selectionPresenter_->onRowSelection(stableId);
    });
    comparisonResults_->setSelectionHandler(
        [this](std::string stableId, const bool locate) {
            const stepcompare::viewer::StableSelectionId selection{stableId};
            componentTree_->selectStableId(selection);
            selectionPresenter_->onRowSelection(stableId, locate);
            statusBar()->showMessage(
                locate ? tr("Đã locate, zoom và highlight linh kiện")
                       : tr("Đã đồng bộ linh kiện giữa bảng, cây và 3D Viewer"));
        });
    comparisonResults_->setFeatureSelectionHandler(
        [this](std::string ownerStableId,
               std::vector<std::uint32_t> faceIndices,
               const bool locate) {
            const stepcompare::viewer::StableSelectionId owner{ownerStableId};
            componentTree_->selectStableId(owner);
            viewer_->selectFeature(owner, faceIndices, locate);
            statusBar()->showMessage(
                locate
                    ? tr("Đã locate, zoom và highlight feature trong 3D Viewer")
                    : tr("Đã đồng bộ feature, part/assembly tree và 3D Viewer"));
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
    if ((comparisonRunner_ && comparisonRunner_->busy()) ||
        (previewLoader_ && previewLoader_->busy())) {
        statusBar()->showMessage(
            tr("Finish or cancel the active OCCT operation before opening another file"));
        return;
    }
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        side == stepcompare::viewer::ModelSide::A ? tr("Open STEP File A")
                                                  : tr("Open STEP File B"),
        {},
        tr("STEP files (*.step *.stp);;All files (*)"));
    if (fileName.isEmpty()) {
        return;
    }
    comparisonResult_.reset();
    comparisonSummary_->setText(tr("KẾT QUẢ ĐÃ MẤT HIỆU LỰC DO INPUT MỚI"));
    comparisonSummary_->setStyleSheet(QStringLiteral(
        "QLabel { background:#eef3f7; color:#182532; font-weight:700; padding:6px; }"));
    comparisonResults_->clearReport();
    viewer_->clearDeviationColors();
    const QByteArray utf8 = fileName.toUtf8();
    std::u8string sourcePath(
        reinterpret_cast<const char8_t*>(utf8.constData()),
        reinterpret_cast<const char8_t*>(utf8.constData() + utf8.size()));
    if (!previewLoader_->start(side, std::move(sourcePath))) {
        statusBar()->showMessage(tr("Another STEP import is still running"));
    }
}

void MainWindow::acceptPreviewResult(PreviewJobResult result) {
    const auto sourcePath = result.importResult.model.sourcePathUtf8;
    auto rows = previewSceneAdapter_->display(result.importResult.model,
                                              result.side,
                                              result.meshSummary.policy,
                                              *viewer_);
    if (result.side == stepcompare::viewer::ModelSide::A) {
        inputAUtf8_ = sourcePath;
        previewRowsA_ = std::move(rows);
    } else {
        inputBUtf8_ = sourcePath;
        previewRowsB_ = std::move(rows);
    }
    refreshPreviewRows();
    statusBar()->showMessage(
        tr("Preview %1: %2 prototype mesh, %3 occurrence reuse, %4 triangles")
            .arg(QString::fromLatin1(
                previewQualityTierName(result.meshSummary.policy.tier)))
            .arg(result.meshSummary.meshedPrototypeCount)
            .arg(result.meshSummary.reusedOccurrenceCount)
            .arg(result.meshSummary.triangleCount));
    if (!inputAUtf8_.empty() && !inputBUtf8_.empty()) {
        startComparison();
    }
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

void MainWindow::startComparison() {
    if (inputAUtf8_.empty() || inputBUtf8_.empty()) {
        statusBar()->showMessage(tr("Load both STEP A and STEP B before comparison"));
        return;
    }
    if (previewLoader_ && previewLoader_->busy()) {
        statusBar()->showMessage(
            tr("STEP preview import is still running; comparison was not started"));
        return;
    }
    stepcompare::application::ComparisonRequest request;
    request.inputAUtf8 = inputAUtf8_;
    request.inputBUtf8 = inputBUtf8_;
    request.deep = true;
    if (!comparisonRunner_->start(std::move(request))) {
        statusBar()->showMessage(tr("A canonical comparison is already running"));
    }
}

void MainWindow::acceptComparisonResult(
    stepcompare::application::ComparisonResult result) {
    comparisonResult_ = std::move(result);
    const auto& report = comparisonResult_->report;
    const auto presentation = presentOverallVerdict(report);
    comparisonSummary_->setText(
        tr("%1  |  %2  |  Cache: %3")
            .arg(presentation.title, presentation.detail,
                 report.cache.hit ? tr("HIT") : tr("MISS")));
    comparisonSummary_->setStyleSheet(summaryStyle(presentation.kind));
    comparisonResults_->setReport(report);
    previewStatus_->setOperationStatus(
        tr("Canonical comparison %1").arg(
            QString::fromStdString(report.execution.status)),
        100,
        false);
    applyCanonicalRowsAndHeatmap();
    comparisonResults_->setHeatmapState(
        viewer_->deviationColoringEnabled(), report.deepDeviation.available);
    statusBar()->showMessage(tr("Canonical comparison result published"));
}

void MainWindow::applyCanonicalRowsAndHeatmap() {
    if (!comparisonResult_) {
        return;
    }
    std::vector<stepcompare::viewer::ResultRowSnapshot> rows;
    rows.reserve(previewRowsA_.size() + previewRowsB_.size());
    rows.insert(rows.end(), previewRowsA_.begin(), previewRowsA_.end());
    rows.insert(rows.end(), previewRowsB_.begin(), previewRowsB_.end());
    std::unordered_map<std::string, std::size_t> rowIndex;
    rowIndex.reserve(rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        rowIndex.emplace(rows[index].stableId.value(), index);
    }

    std::vector<stepcompare::viewer::DeviationColorAssignment> deviations;
    for (const auto& component : comparisonResult_->report.components) {
        const auto change = componentChangeKind(component);
        for (const auto& [side, nodeId] : {
                 std::pair{stepcompare::viewer::ModelSide::A, component.idA},
                 std::pair{stepcompare::viewer::ModelSide::B, component.idB}}) {
            if (nodeId.empty()) {
                continue;
            }
            const auto stableId = previewStableId(side, nodeId);
            const auto found = rowIndex.find(stableId);
            if (found != rowIndex.end()) {
                rows[found->second].change = change;
                if (component.deviation.available) {
                    deviations.push_back({
                        stepcompare::viewer::StableSelectionId{stableId},
                        component.deviation.maximumMm});
                }
            }
        }
    }
    showComponentResults(std::move(rows));

    if (deviations.empty()) {
        viewer_->clearDeviationColors();
        return;
    }
    const auto scale = stepcompare::viewer::makeDeviationColorScale(
        comparisonResult_->report.deepDeviation.maximumMm,
        comparisonResult_->report.tolerances.surfaceMm);
    if (!scale || !viewer_->setDeviationColors(deviations, *scale)) {
        viewer_->clearDeviationColors();
        statusBar()->showMessage(
            tr("Heatmap rejected invalid or incomplete deviation evidence"));
    }
}

void MainWindow::saveCanonicalReport(const bool json) {
    if (!comparisonResult_) {
        statusBar()->showMessage(tr("No canonical comparison result to save"));
        return;
    }
    const auto path = QFileDialog::getSaveFileName(
        this,
        json ? tr("Save canonical JSON report") : tr("Save canonical CSV report"),
        json ? QStringLiteral("stepcompare-report.json")
             : QStringLiteral("stepcompare-report.csv"),
        json ? tr("JSON files (*.json)") : tr("CSV files (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    try {
        std::ofstream output(std::filesystem::path(path.toStdWString()),
                             std::ios::binary | std::ios::trunc);
        if (json) {
            stepcompare::reporting::writeJson(comparisonResult_->report, output);
        } else {
            stepcompare::reporting::writeCsv(comparisonResult_->report, output);
        }
        statusBar()->showMessage(output ? tr("Canonical report saved")
                                        : tr("Canonical report write failed"));
    } catch (...) {
        statusBar()->showMessage(tr("Canonical report write failed"));
    }
}

void MainWindow::applyViewerState() {
    coordinateBanner_->setText(QString::fromLatin1(viewerState_.coordinateBanner().data(),
                                                    viewerState_.coordinateBanner().size()));
    viewer_->applyState(viewerState_);
}

}  // namespace stepcompare::gui
