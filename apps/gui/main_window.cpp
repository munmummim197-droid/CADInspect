#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shobjidl_core.h>
#endif

#include "main_window.hpp"

#include "component_tree_panel.hpp"
#include "comparison_runner.hpp"
#include "comparison_readability_model.hpp"
#include "comparison_results_panel.hpp"
#include "dropped_step_files.hpp"
#include "preview_status_widget.hpp"
#include "section_view_controls.hpp"
#include "step_preview_loader.hpp"
#include "step_preview_scene_adapter.hpp"
#include "viewer_actions.hpp"
#include "viewer_control_bar.hpp"
#include "pair_isolation_model.hpp"

#include <stepcompare/viewer/occt_viewer_widget.hpp>
#include <stepcompare/reporting/writers.hpp>

#include <QFileDialog>
#include <QFileInfo>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QShowEvent>
#include <QStringList>
#include <QStatusBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>
#include <QUrl>

#include <TopLoc_Location.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
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

gp_Trsf occtTransform(const std::array<double, 16>& matrix) {
    gp_Trsf result;
    result.SetValues(matrix[0], matrix[1], matrix[2], matrix[3],
                     matrix[4], matrix[5], matrix[6], matrix[7],
                     matrix[8], matrix[9], matrix[10], matrix[11]);
    return result;
}

std::optional<TopLoc_Location> pairAlignment(
    const std::unordered_map<std::string, std::array<double, 16>>& transforms,
    const std::string& stableIdA,
    const std::string& stableIdB) {
    const auto foundA = transforms.find(stableIdA);
    const auto foundB = transforms.find(stableIdB);
    if (foundA == transforms.end() || foundB == transforms.end()) {
        return std::nullopt;
    }
    gp_Trsf bToA = occtTransform(foundA->second);
    bToA.Multiply(occtTransform(foundB->second).Inverted());
    return TopLoc_Location(bToA);
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

std::vector<PreviewPartIdentity> previewPartIdentities(
    const stepcompare::import::ImportedModel& model,
    const stepcompare::viewer::ModelSide side) {
    std::unordered_map<std::string, QString> prototypeNames;
    prototypeNames.reserve(model.prototypes.size());
    for (const auto& prototype : model.prototypes) {
        prototypeNames.emplace(prototype.id,
                               QString::fromUtf8(prototype.nameUtf8));
    }
    std::vector<PreviewPartIdentity> result;
    result.reserve(model.nodes.size());
    for (const auto& node : model.nodes) {
        if (!node.prototypeId) {
            continue;
        }
        const auto found = prototypeNames.find(*node.prototypeId);
        QString partName = found == prototypeNames.end() ? QString{} : found->second;
        if (partName.isEmpty()) {
            partName = QString::fromUtf8(node.nameUtf8);
        }
        result.push_back({
            .stableId = previewStableId(side, node.id),
            .prototypeId = *node.prototypeId,
            .partName = std::move(partName),
            .side = side,
        });
    }
    return result;
}

bool isSinglePartModel(const stepcompare::import::ImportedModel& model) {
    return model.nodes.size() == 1U && model.prototypes.size() == 1U &&
           !model.nodes.front().isAssembly &&
           model.nodes.front().prototypeId.has_value();
}

std::string featurePairCacheKey(const std::string_view stableIdA,
                                const std::string_view stableIdB) {
    return std::string(stableIdA) + '\x1f' + std::string(stableIdB);
}

void applyOfficialWindowIcon(QWidget& window) {
    window.setWindowIcon(QApplication::windowIcon());
#ifdef _WIN32
    // Qt's application icon is kept for all platforms. On Windows, also set
    // the native HWND icons explicitly so taskbar/Alt-Tab do not fall back to
    // the generic Qt window icon, including for Ctrl+N child windows.
    constexpr int officialIconResourceId = 101;
    const auto module = GetModuleHandleW(nullptr);
    const auto largeIcon = static_cast<HICON>(LoadImageW(
        module,
        MAKEINTRESOURCEW(officialIconResourceId),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    const auto smallIcon = static_cast<HICON>(LoadImageW(
        module,
        MAKEINTRESOURCEW(officialIconResourceId),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    const auto nativeWindow = reinterpret_cast<HWND>(window.winId());
    if (largeIcon != nullptr) {
        SendMessageW(nativeWindow, WM_SETICON, ICON_BIG,
                     reinterpret_cast<LPARAM>(largeIcon));
    }
    if (smallIcon != nullptr) {
        SendMessageW(nativeWindow, WM_SETICON, ICON_SMALL,
                     reinterpret_cast<LPARAM>(smallIcon));
    }

    // An explicit per-window AppUserModelID makes Windows honor the explicit
    // relaunch icon for the taskbar group, even when no installed shortcut is
    // available yet (portable/package candidate builds).
    IPropertyStore* propertyStore = nullptr;
    if (SUCCEEDED(SHGetPropertyStoreForWindow(
            nativeWindow, IID_PPV_ARGS(&propertyStore))) &&
        propertyStore != nullptr) {
        const auto setStringProperty =
            [propertyStore](const PROPERTYKEY& key, const wchar_t* text) {
                PROPVARIANT value;
                PropVariantInit(&value);
                const auto initialized = InitPropVariantFromString(text, &value);
                if (SUCCEEDED(initialized)) {
                    static_cast<void>(propertyStore->SetValue(key, value));
                }
                static_cast<void>(PropVariantClear(&value));
            };

        setStringProperty(PKEY_AppUserModel_ID,
                          L"CADInspect.Project.Desktop");

        std::array<wchar_t, 32768> executablePath{};
        const auto pathLength = GetModuleFileNameW(
            module, executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        if (pathLength > 0 && pathLength < executablePath.size()) {
            std::wstring iconResource(executablePath.data(), pathLength);
            iconResource.append(L",-101");
            setStringProperty(PKEY_AppUserModel_RelaunchIconResource,
                              iconResource.c_str());
        }
        static_cast<void>(propertyStore->Commit());
        propertyStore->Release();
    }
#endif
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("CADInspect"));
    resize(1280, 800);
    setAcceptDrops(true);

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
    auto* viewerPane = new QWidget(splitter);
    viewerPane->setObjectName(QStringLiteral("viewerPane"));
    auto* viewerLayout = new QVBoxLayout(viewerPane);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(0);
    viewerControls_ = new ViewerControlBar(
        [this](const auto mode) { setPresentationMode(mode); },
        [this](const auto layer) { setSceneLayer(layer); },
        [this](const bool enabled) { setHeatmapEnabled(enabled); },
        [this](const auto command) { applyIsolationCommand(command); },
        [this] { viewer_->fitAll(); },
        viewerPane);
    viewer_ = new stepcompare::viewer::OcctViewerWidget(viewerPane);
    viewer_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(viewer_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& position) { showViewerContextMenu(position); });
    viewerLayout->addWidget(viewerControls_);
    viewerLayout->addWidget(viewer_, 1);
    splitter->addWidget(componentTree_);
    splitter->addWidget(viewerPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 960});
    comparisonResults_ = new ComparisonResultsPanel(workspaceSplitter);
    workspaceSplitter->addWidget(splitter);
    workspaceSplitter->addWidget(comparisonResults_);
    workspaceSplitter->setStretchFactor(0, 3);
    workspaceSplitter->setStretchFactor(1, 2);
    workspaceSplitter->setSizes({470, 300});
    sectionControls_ = new SectionViewControls(
        [this](const stepcompare::viewer::SectionSettings settings) {
            viewerState_.setSectionSettings(settings);
            applyViewerState();
            statusBar()->showMessage(
                tr("Section: %1 · offset %2% · %3 · áp dụng %4")
                    .arg(QString::fromLatin1(
                             stepcompare::viewer::toString(settings.direction).data()))
                    .arg(static_cast<int>(settings.normalizedOffset * 100.0))
                    .arg(settings.flipped ? tr("đảo hướng") : tr("hướng chuẩn"))
                    .arg(QString::fromLatin1(
                        stepcompare::viewer::toString(settings.target).data())));
        },
        central);
    layout->addWidget(sectionControls_);
    layout->addWidget(workspaceSplitter, 1);
    setCentralWidget(central);

    actions_ = std::make_unique<ViewerActions>(
        *this,
        [this] { createNewComparison(); },
        [this] { openStep(stepcompare::viewer::ModelSide::A); },
        [this] { openStep(stepcompare::viewer::ModelSide::B); },
        [this] { startComparison(); },
        [this] { saveCanonicalReport(true); },
        [this] { saveCanonicalReport(false); },
        [this](const bool enabled) { setHeatmapEnabled(enabled); },
        [this](const auto presentation) { setPresentationMode(presentation); },
        [this](const auto layer) { setSceneLayer(layer); },
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
        [this](stepcompare::application::FeaturePairComparisonResult result) {
            acceptFeaturePairResult(std::move(result));
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
    componentTree_->setActivationHandler([this](std::string stableId) {
        selectionPresenter_->onRowSelection(stableId, false);
        applyIsolationCommand(IsolationCommand::ShowOnlyPair);
    });
    componentTree_->setContextActionHandler(
        [this](std::string stableId,
               const ComponentTreeContextAction action) {
            if (action == ComponentTreeContextAction::RestoreAssembly) {
                restoreAssembly();
                return;
            }
            if (stableId.empty()) {
                return;
            }
            selectionPresenter_->onRowSelection(stableId,
                                                action == ComponentTreeContextAction::Locate);
            switch (action) {
                case ComponentTreeContextAction::Locate:
                    statusBar()->showMessage(
                        tr("Đã locate, zoom và highlight occurrence"));
                    break;
                case ComponentTreeContextAction::ShowOnlyA:
                    applyIsolationCommand(IsolationCommand::ShowOnlyA);
                    break;
                case ComponentTreeContextAction::ShowOnlyB:
                    applyIsolationCommand(IsolationCommand::ShowOnlyB);
                    break;
                case ComponentTreeContextAction::ShowOnlyPair:
                    applyIsolationCommand(IsolationCommand::ShowOnlyPair);
                    break;
                case ComponentTreeContextAction::RestoreAssembly:
                    break;
            }
        });
    comparisonResults_->setSelectionHandler(
        [this](std::string stableId, const bool locate) {
            const stepcompare::viewer::StableSelectionId selection{stableId};
            componentTree_->selectStableId(selection);
            selectionPresenter_->onRowSelection(stableId, false);
            if (locate) {
                applyIsolationCommand(IsolationCommand::ShowOnlyPair);
            } else {
                statusBar()->showMessage(
                    tr("Đã đồng bộ occurrence giữa bảng Part, cây và 3D Viewer"));
            }
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
        if (stableId.empty()) {
            selectionPresenter_->clearSelection();
            componentTree_->clearSelection();
            pendingManualPairA_.reset();
            statusBar()->showMessage(tr("Đã bỏ chọn occurrence"));
            return;
        }
        selectionPresenter_->onViewerSelection(stableId);
        statusBar()->showMessage(
            tr("Selected: %1").arg(QString::fromStdString(stableId)));
    });

    statusBar()->showMessage(tr("Ready — drag: rotate/pan/zoom; wheel: zoom"));
    applyViewerState();
    applyOfficialWindowIcon(*this);
}

MainWindow::~MainWindow() = default;

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    // Apply after the real top-level HWND exists and Windows has created its
    // taskbar representation. This also covers every Ctrl+N window.
    applyOfficialWindowIcon(*this);
}

void MainWindow::createNewComparison(const QStringList& droppedFiles) {
    auto* window = new MainWindow();
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    window->show();
    if (!droppedFiles.isEmpty()) {
        window->openDroppedStepFiles(droppedFiles);
    }
    statusBar()->showMessage(
        tr("Đã mở cửa sổ so sánh mới; phiên hiện tại được giữ nguyên"));
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event == nullptr || !event->mimeData()->hasUrls()) {
        return;
    }
    QStringList paths;
    for (const auto& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) {
            return;
        }
        paths.push_back(url.toLocalFile());
    }
    const auto plan = planDroppedStepFiles(
        paths,
        !inputAUtf8_.empty(),
        !inputBUtf8_.empty(),
        (comparisonRunner_ && comparisonRunner_->busy()) ||
            (previewLoader_ && previewLoader_->busy()));
    if (plan.accepted()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    if (event == nullptr || !event->mimeData()->hasUrls()) {
        return;
    }
    QStringList paths;
    for (const auto& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) {
            statusBar()->showMessage(tr("Chỉ hỗ trợ kéo-thả file STEP cục bộ"));
            return;
        }
        paths.push_back(url.toLocalFile());
    }
    openDroppedStepFiles(paths);
    event->acceptProposedAction();
}

void MainWindow::openDroppedStepFiles(const QStringList& paths) {
    for (const auto& path : paths) {
        if (!QFileInfo(path).isFile()) {
            statusBar()->showMessage(
                tr("Không mở được file kéo-thả: %1").arg(path));
            return;
        }
    }
    const auto plan = planDroppedStepFiles(
        paths,
        !inputAUtf8_.empty(),
        !inputBUtf8_.empty(),
        (comparisonRunner_ && comparisonRunner_->busy()) ||
            (previewLoader_ && previewLoader_->busy()));
    if (!plan.accepted()) {
        statusBar()->showMessage(plan.rejectionReason);
        return;
    }
    switch (plan.target) {
        case DroppedStepOpenTarget::CurrentA:
            static_cast<void>(openStepPath(stepcompare::viewer::ModelSide::A,
                                           plan.paths.front()));
            break;
        case DroppedStepOpenTarget::CurrentB:
            static_cast<void>(openStepPath(stepcompare::viewer::ModelSide::B,
                                           plan.paths.front()));
            break;
        case DroppedStepOpenTarget::CurrentPair:
            pendingDroppedBPath_ = plan.paths.at(1);
            if (!openStepPath(stepcompare::viewer::ModelSide::A,
                              plan.paths.front())) {
                pendingDroppedBPath_.clear();
            }
            break;
        case DroppedStepOpenTarget::NewWindowA:
        case DroppedStepOpenTarget::NewWindowPair:
            createNewComparison(plan.paths);
            break;
        case DroppedStepOpenTarget::Reject:
            break;
    }
}

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
    static_cast<void>(openStepPath(side, fileName));
}

bool MainWindow::openStepPath(const stepcompare::viewer::ModelSide side,
                              const QString& fileName) {
    if ((comparisonRunner_ && comparisonRunner_->busy()) ||
        (previewLoader_ && previewLoader_->busy())) {
        statusBar()->showMessage(
            tr("Finish or cancel the active OCCT operation before opening another file"));
        return false;
    }
    if (!isSupportedDroppedStepPath(fileName) || !QFileInfo(fileName).isFile()) {
        statusBar()->showMessage(tr("File STEP không hợp lệ: %1").arg(fileName));
        return false;
    }
    restoreAssembly(false);
    selectionPresenter_->clearSelection();
    componentTree_->clearSelection();
    const std::string staleSidePrefix =
        side == stepcompare::viewer::ModelSide::A ? "preview/A/" : "preview/B/";
    std::erase_if(occurrenceTransforms_,
                  [&staleSidePrefix](const auto& entry) {
                      return entry.first.starts_with(staleSidePrefix);
                  });
    comparisonResult_.reset();
    activeFeaturePair_.reset();
    featurePairCache_.clear();
    comparisonSummary_->setText(tr("KẾT QUẢ ĐÃ MẤT HIỆU LỰC DO INPUT MỚI"));
    comparisonSummary_->setStyleSheet(QStringLiteral(
        "QLabel { background:#eef3f7; color:#182532; font-weight:700; padding:6px; }"));
    comparisonResults_->clearReport();
    if (side == stepcompare::viewer::ModelSide::A) {
        inputAUtf8_.clear();
        previewPartIdentitiesA_.clear();
        inputAIsSinglePart_ = false;
    } else {
        inputBUtf8_.clear();
        previewPartIdentitiesB_.clear();
        inputBIsSinglePart_ = false;
    }
    viewer_->clearDeviationColors();
    viewer_->clearChangedFeatureHighlights();
    const QByteArray utf8 = fileName.toUtf8();
    std::u8string sourcePath(
        reinterpret_cast<const char8_t*>(utf8.constData()),
        reinterpret_cast<const char8_t*>(utf8.constData() + utf8.size()));
    if (!previewLoader_->start(side, std::move(sourcePath))) {
        statusBar()->showMessage(tr("Another STEP import is still running"));
        return false;
    }
    statusBar()->showMessage(
        tr("Đang mở File %1: %2")
            .arg(side == stepcompare::viewer::ModelSide::A ? QStringLiteral("A")
                                                           : QStringLiteral("B"),
                 QFileInfo(fileName).fileName()));
    return true;
}

void MainWindow::acceptPreviewResult(PreviewJobResult result) {
    const auto sourcePath = result.importResult.model.sourcePathUtf8;
    const bool singlePart = isSinglePartModel(result.importResult.model);
    auto partIdentities = previewPartIdentities(result.importResult.model,
                                                result.side);
    auto plan = previewSceneAdapter_->display(result.importResult.model,
                                              result.side,
                                              result.meshSummary.policy,
                                              *viewer_);
    const std::string sidePrefix = result.side == stepcompare::viewer::ModelSide::A
                                       ? "preview/A/"
                                       : "preview/B/";
    std::erase_if(occurrenceTransforms_,
                  [&sidePrefix](const auto& entry) {
                      return entry.first.starts_with(sidePrefix);
                  });
    for (const auto& occurrence : plan.occurrences) {
        occurrenceTransforms_.insert_or_assign(
            occurrence.stableId.value(), occurrence.worldTransform);
    }
    if (result.side == stepcompare::viewer::ModelSide::A) {
        inputAUtf8_ = sourcePath;
        previewRowsA_ = std::move(plan.rows);
        previewPartIdentitiesA_ = std::move(partIdentities);
        inputAIsSinglePart_ = singlePart;
    } else {
        inputBUtf8_ = sourcePath;
        previewRowsB_ = std::move(plan.rows);
        previewPartIdentitiesB_ = std::move(partIdentities);
        inputBIsSinglePart_ = singlePart;
    }
    std::vector<PreviewPartIdentity> combinedPartIdentities;
    combinedPartIdentities.reserve(previewPartIdentitiesA_.size() +
                                   previewPartIdentitiesB_.size());
    combinedPartIdentities.insert(combinedPartIdentities.end(),
                                  previewPartIdentitiesA_.begin(),
                                  previewPartIdentitiesA_.end());
    combinedPartIdentities.insert(combinedPartIdentities.end(),
                                  previewPartIdentitiesB_.begin(),
                                  previewPartIdentitiesB_.end());
    comparisonResults_->setPartIdentities(std::move(combinedPartIdentities));
    refreshPreviewRows();
    statusBar()->showMessage(
        tr("Preview %1: %2 prototype mesh, %3 occurrence reuse, %4 triangles")
            .arg(QString::fromLatin1(
                previewQualityTierName(result.meshSummary.policy.tier)))
            .arg(result.meshSummary.meshedPrototypeCount)
            .arg(result.meshSummary.reusedOccurrenceCount)
            .arg(result.meshSummary.triangleCount));
    if (result.side == stepcompare::viewer::ModelSide::A &&
        !pendingDroppedBPath_.isEmpty()) {
        const QString pathB = std::exchange(pendingDroppedBPath_, QString{});
        QTimer::singleShot(0, this, [this, pathB] {
            if (!openStepPath(stepcompare::viewer::ModelSide::B, pathB)) {
                statusBar()->showMessage(
                    tr("Không thể mở File B đã kéo-thả: %1").arg(pathB));
            }
        });
        return;
    }
    if (!inputAUtf8_.empty() && !inputBUtf8_.empty()) {
        startComparison();
    }
}

void MainWindow::showViewerContextMenu(const QPoint& position) {
    QMenu menu(viewer_);
    auto* newComparison = menu.addAction(tr("So sánh mới"));
    auto* openA = menu.addAction(tr("Mở File A..."));
    auto* openB = menu.addAction(tr("Mở File B..."));
    menu.addSeparator();
    const bool hasSelection = selectionPresenter_ &&
                              selectionPresenter_->selectedId().has_value();
    auto* locate = menu.addAction(tr("Locate / Zoom / Highlight"));
    locate->setEnabled(hasSelection);
    auto* isolateMenu = menu.addMenu(tr("Cô lập occurrence"));
    auto* showOnlyA = isolateMenu->addAction(tr("Show Only A"));
    auto* showOnlyB = isolateMenu->addAction(tr("Show Only B"));
    auto* showOnlyPair = isolateMenu->addAction(tr("Show Only Pair"));
    showOnlyA->setEnabled(hasSelection);
    showOnlyB->setEnabled(hasSelection);
    showOnlyPair->setEnabled(hasSelection);
    auto* restore = isolateMenu->addAction(tr("Show All / Restore Assembly"));

    auto* viewMenu = menu.addMenu(tr("View"));
    auto* shaded = viewMenu->addAction(tr("Shaded"));
    auto* shadedEdges = viewMenu->addAction(tr("Shaded + Edges"));
    auto* wireframe = viewMenu->addAction(tr("Wireframe"));
    auto* xray = viewMenu->addAction(tr("Transparent / X-Ray"));
    auto* section = viewMenu->addAction(tr("Section View"));
    auto* fitAll = menu.addAction(tr("Fit All"));

    QAction* chosen = menu.exec(viewer_->mapToGlobal(position));
    if (chosen == newComparison) {
        createNewComparison();
    } else if (chosen == openA) {
        openStep(stepcompare::viewer::ModelSide::A);
    } else if (chosen == openB) {
        openStep(stepcompare::viewer::ModelSide::B);
    } else if (chosen == locate && selectionPresenter_->selectedId()) {
        selectionPresenter_->onRowSelection(
            selectionPresenter_->selectedId()->value(), true);
    } else if (chosen == showOnlyA) {
        applyIsolationCommand(IsolationCommand::ShowOnlyA);
    } else if (chosen == showOnlyB) {
        applyIsolationCommand(IsolationCommand::ShowOnlyB);
    } else if (chosen == showOnlyPair) {
        applyIsolationCommand(IsolationCommand::ShowOnlyPair);
    } else if (chosen == restore) {
        restoreAssembly();
    } else if (chosen == shaded) {
        setPresentationMode(stepcompare::viewer::PresentationMode::Shaded);
    } else if (chosen == shadedEdges) {
        setPresentationMode(
            stepcompare::viewer::PresentationMode::ShadedWithEdges);
    } else if (chosen == wireframe) {
        setPresentationMode(stepcompare::viewer::PresentationMode::Wireframe);
    } else if (chosen == xray) {
        setPresentationMode(
            stepcompare::viewer::PresentationMode::TransparentXRay);
    } else if (chosen == section) {
        setPresentationMode(stepcompare::viewer::PresentationMode::Section);
    } else if (chosen == fitAll) {
        viewer_->fitAll();
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
    request.featureEvidenceScope =
        stepcompare::application::FeatureEvidenceScope::SinglePartOnly;
    if (!comparisonRunner_->start(std::move(request))) {
        statusBar()->showMessage(tr("A canonical comparison is already running"));
    }
}

void MainWindow::acceptComparisonResult(
    stepcompare::application::ComparisonResult result) {
    activeFeaturePair_.reset();
    featurePairCache_.clear();
    viewer_->clearChangedFeatureHighlights();
    comparisonResult_ = std::move(result);
    const auto& report = comparisonResult_->report;
    const auto presentation = presentOverallVerdict(report);
    comparisonSummary_->setText(
        tr("%1  |  %2  |  Cache: %3")
            .arg(presentation.title, presentation.detail,
                 report.cache.hit ? tr("HIT") : tr("MISS")));
    comparisonSummary_->setStyleSheet(summaryStyle(presentation.kind));
    comparisonResults_->setReport(report);
    if (!(inputAIsSinglePart_ && inputBIsSinglePart_)) {
        comparisonResults_->setAssemblyFeatureDeferred();
    }
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

void MainWindow::startFeaturePairComparison(std::string stableIdA,
                                            std::string stableIdB) {
    if (!comparisonResult_ || inputAIsSinglePart_ && inputBIsSinglePart_) {
        return;
    }
    activeFeaturePair_ = std::pair{std::move(stableIdA), std::move(stableIdB)};
    const auto& [activeA, activeB] = *activeFeaturePair_;
    comparisonResults_->setFeaturePairLoading(activeA, activeB);
    viewer_->clearChangedFeatureHighlights();
    comparisonResult_->report.features.clear();

    const std::string key = featurePairCacheKey(activeA, activeB);
    if (const auto cached = featurePairCache_.find(key);
        cached != featurePairCache_.end()) {
        comparisonResult_->report.features = cached->second;
        comparisonResults_->setFeaturePairResult(cached->second, activeA, activeB);
        applyChangedFeatureHighlights(cached->second);
        previewStatus_->setOperationStatus(
            tr("Feature pair comparison — Cache HIT"), 100, false);
        statusBar()->showMessage(
            tr("Đã khôi phục Feature của đúng pair từ cache"));
        return;
    }

    stepcompare::application::FeaturePairComparisonRequest request;
    request.inputAUtf8 = inputAUtf8_;
    request.inputBUtf8 = inputBUtf8_;
    request.componentIdA = std::string(occurrenceNodeId(activeA));
    request.componentIdB = std::string(occurrenceNodeId(activeB));
    const auto& tolerances = comparisonResult_->report.tolerances;
    request.tolerances.positionMm = tolerances.positionMm;
    request.tolerances.surfaceMm = tolerances.surfaceMm;
    request.tolerances.angularDegrees = tolerances.angularDegrees;
    request.tolerances.booleanFuzzyMm = tolerances.booleanFuzzyMm;
    request.tolerances.relativeProperty = tolerances.relativeProperty;
    if (!comparisonRunner_->startFeaturePair(std::move(request))) {
        comparisonResults_->setFeaturePairError(activeA, activeB);
        statusBar()->showMessage(
            tr("CHECK — một tác vụ OCCT khác đang chạy; chưa so sánh Feature pair"));
    }
}

void MainWindow::acceptFeaturePairResult(
    stepcompare::application::FeaturePairComparisonResult result) {
    if (!activeFeaturePair_ || !comparisonResult_) {
        return;
    }
    const auto& [stableIdA, stableIdB] = *activeFeaturePair_;
    if (occurrenceNodeId(stableIdA) != result.componentIdA ||
        occurrenceNodeId(stableIdB) != result.componentIdB) {
        return;
    }
    previewStatus_->setOperationStatus(
        tr("Feature pair comparison %1")
            .arg(result.status ==
                         stepcompare::application::ComparisonRunStatus::Completed
                     ? tr("COMPLETED")
                     : tr("CHECK")),
        100,
        false);
    if (result.status !=
        stepcompare::application::ComparisonRunStatus::Completed) {
        comparisonResult_->report.features.clear();
        viewer_->clearChangedFeatureHighlights();
        comparisonResults_->setFeaturePairError(stableIdA, stableIdB);
        statusBar()->showMessage(
            tr("CHECK — feature evidence của pair không hoàn tất; không suy luận PASS"));
        return;
    }

    const std::string key = featurePairCacheKey(stableIdA, stableIdB);
    featurePairCache_.insert_or_assign(key, result.features);
    comparisonResult_->report.features = result.features;
    comparisonResults_->setFeaturePairResult(result.features,
                                             stableIdA,
                                             stableIdB);
    applyChangedFeatureHighlights(result.features);
    const auto changed = std::ranges::count_if(
        result.features,
        [](const auto& feature) { return feature.result == "FAIL"; });
    statusBar()->showMessage(
        tr("Feature Pair hoàn tất: %1 feature, %2 vùng thay đổi được highlight")
            .arg(static_cast<qulonglong>(result.features.size()))
            .arg(static_cast<qulonglong>(changed)));
}

void MainWindow::applyChangedFeatureHighlights(
    const std::vector<stepcompare::reporting::FeatureRow>& features) {
    std::unordered_map<std::string, std::vector<std::uint32_t>> facesByOwner;
    for (const auto& feature : features) {
        if (feature.result != "FAIL") {
            continue;
        }
        if (!feature.ownerComponentIdA.empty() &&
            !feature.faceIndicesA.empty()) {
            auto& faces = facesByOwner[previewStableId(
                stepcompare::viewer::ModelSide::A,
                feature.ownerComponentIdA)];
            faces.insert(faces.end(),
                         feature.faceIndicesA.begin(),
                         feature.faceIndicesA.end());
        }
        if (!feature.ownerComponentIdB.empty() &&
            !feature.faceIndicesB.empty()) {
            auto& faces = facesByOwner[previewStableId(
                stepcompare::viewer::ModelSide::B,
                feature.ownerComponentIdB)];
            faces.insert(faces.end(),
                         feature.faceIndicesB.begin(),
                         feature.faceIndicesB.end());
        }
    }

    std::vector<stepcompare::viewer::ChangedFeatureHighlightAssignment>
        assignments;
    assignments.reserve(facesByOwner.size());
    for (auto& [owner, faces] : facesByOwner) {
        std::ranges::sort(faces);
        faces.erase(std::unique(faces.begin(), faces.end()), faces.end());
        assignments.push_back({
            .ownerStableId = stepcompare::viewer::StableSelectionId{owner},
            .faceIndices = std::move(faces),
        });
    }
    if (!viewer_->setChangedFeatureHighlights(assignments)) {
        viewer_->clearChangedFeatureHighlights();
        statusBar()->showMessage(
            tr("Feature result hợp lệ nhưng không ánh xạ được face highlight vào preview"));
    }
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

void MainWindow::setPresentationMode(
    const stepcompare::viewer::PresentationMode mode) {
    viewerState_.setPresentationMode(mode);
    sectionControls_->setSectionModeActive(
        mode == stepcompare::viewer::PresentationMode::Section);
    if (actions_) {
        actions_->setPresentationMode(mode);
    }
    viewerControls_->setPresentationMode(mode);
    applyViewerState();
    statusBar()->showMessage(
        tr("Chế độ hiển thị: %1")
            .arg(QString::fromLatin1(stepcompare::viewer::toString(mode).data())));
}

void MainWindow::setSceneLayer(const stepcompare::viewer::SceneLayer layer) {
    viewerState_.setLayer(layer);
    if (actions_) {
        actions_->setLayer(layer);
    }
    viewerControls_->setLayer(layer);
    applyViewerState();
}

void MainWindow::setHeatmapEnabled(const bool enabled) {
    viewer_->setDeviationColoringEnabled(enabled);
    const bool active = viewer_->deviationColoringEnabled();
    comparisonResults_->setHeatmapState(
        active,
        comparisonResult_ && comparisonResult_->report.deepDeviation.available);
    if (actions_) {
        actions_->setHeatmapEnabled(active);
    }
    viewerControls_->setHeatmapEnabled(active);
    statusBar()->showMessage(
        enabled && !active
            ? tr("Heatmap unavailable: no validated deviation evidence")
            : active ? tr("Heatmap enabled") : tr("Heatmap disabled"));
}

void MainWindow::applyIsolationCommand(const IsolationCommand command) {
    using stepcompare::viewer::StableSelectionId;
    if (command == IsolationCommand::RestoreAssembly) {
        restoreAssembly();
        return;
    }
    if (!selectionPresenter_ || !selectionPresenter_->selectedId()) {
        statusBar()->showMessage(
            tr("CHECK / MATCH_AMBIGUOUS — hãy chọn một occurrence trong cây hoặc 3D Viewer"));
        return;
    }

    const bool assemblyFeatureMode =
        !(inputAIsSinglePart_ && inputBIsSinglePart_);
    if (command == IsolationCommand::ShowOnlyPair && assemblyFeatureMode &&
        comparisonRunner_ && comparisonRunner_->busy()) {
        statusBar()->showMessage(
            tr("Đang so sánh Feature của pair hiện tại; vui lòng chờ hoặc Cancel"));
        return;
    }
    if (command != IsolationCommand::ShowOnlyPair && assemblyFeatureMode) {
        activeFeaturePair_.reset();
        if (comparisonResult_) {
            comparisonResult_->report.features.clear();
            comparisonResults_->setAssemblyFeatureDeferred();
        }
        viewer_->clearChangedFeatureHighlights();
    }

    const std::string selected = selectionPresenter_->selectedId()->value();
    if (!isOccurrenceStableId(selected) ||
        !occurrenceTransforms_.contains(selected)) {
        statusBar()->showMessage(
            tr("CHECK / MATCH_AMBIGUOUS — node đã chọn không phải occurrence"));
        return;
    }

    if (command != IsolationCommand::ShowOnlyPair) {
        pendingManualPairA_.reset();
    }

    std::string stableIdA;
    std::string stableIdB;
    bool manualPair = false;
    if (command == IsolationCommand::ShowOnlyPair && pendingManualPairA_ &&
        !isSideAStableId(selected)) {
        stableIdA = *pendingManualPairA_;
        stableIdB = selected;
        manualPair = true;
    } else if ((command == IsolationCommand::ShowOnlyA && isSideAStableId(selected)) ||
               (command == IsolationCommand::ShowOnlyB && !isSideAStableId(selected))) {
        if (command == IsolationCommand::ShowOnlyA) {
            stableIdA = selected;
        } else {
            stableIdB = selected;
        }
    } else {
        if (!comparisonResult_) {
            statusBar()->showMessage(
                tr("CHECK / MATCH_AMBIGUOUS — chưa có canonical component matching"));
            return;
        }
        const auto pair = resolveCanonicalPair(comparisonResult_->report, selected);
        if (!pair.resolved()) {
            if (command == IsolationCommand::ShowOnlyPair && isSideAStableId(selected)) {
                pendingManualPairA_ = selected;
                statusBar()->showMessage(tr(
                    "CHECK / MATCH_AMBIGUOUS — chọn occurrence B thủ công rồi dùng Show Only Pair lần nữa"));
            } else {
                statusBar()->showMessage(
                    tr("CHECK / MATCH_AMBIGUOUS — không có evidence cặp occurrence đủ tin cậy"));
            }
            return;
        }
        stableIdA = pair.stableIdA;
        stableIdB = pair.stableIdB;
    }

    std::vector<StableSelectionId> isolated;
    if (command != IsolationCommand::ShowOnlyB && !stableIdA.empty()) {
        isolated.emplace_back(stableIdA);
    }
    if (command != IsolationCommand::ShowOnlyA && !stableIdB.empty()) {
        isolated.emplace_back(stableIdB);
    }
    if (isolated.empty()) {
        statusBar()->showMessage(
            tr("CHECK / MATCH_AMBIGUOUS — occurrence tương ứng không khả dụng"));
        return;
    }

    for (const auto& stableId : isolated) {
        if (!occurrenceTransforms_.contains(stableId.value())) {
            statusBar()->showMessage(
                tr("CHECK / MATCH_AMBIGUOUS — occurrence không tồn tại trong scene hiện tại"));
            return;
        }
    }

    std::optional<TopLoc_Location> alignment;
    if (!stableIdA.empty() && !stableIdB.empty()) {
        alignment = pairAlignment(
            occurrenceTransforms_, stableIdA, stableIdB);
        if (!alignment) {
            statusBar()->showMessage(
                tr("CHECK / MATCH_AMBIGUOUS — thiếu transform occurrence để aligned pair"));
            return;
        }
    }
    viewer_->clearAlignedLocations();
    if (alignment) {
        viewer_->setAlignedLocation(StableSelectionId{stableIdB}, *alignment);
    }
    if (!viewer_->setIsolatedStableIds(isolated)) {
        viewer_->clearAlignedLocations();
        statusBar()->showMessage(
            tr("CHECK / MATCH_AMBIGUOUS — occurrence không tồn tại trong scene hiện tại"));
        return;
    }

    pendingManualPairA_.reset();
    viewerControls_->setIsolationActive(true);
    viewer_->fitAll();
    const std::string highlighted =
        std::ranges::any_of(isolated, [&selected](const auto& stableId) {
            return stableId.value() == selected;
        })
            ? selected
            : isolated.front().value();
    const StableSelectionId highlightedId{highlighted};
    componentTree_->selectStableId(highlightedId);
    comparisonResults_->selectStableId(highlightedId);
    selectionPresenter_->onRowSelection(highlighted, false);
    const QString coordinateMode =
        viewerState_.coordinates() == stepcompare::viewer::CoordinateMode::Aligned
            ? tr("ALIGNED")
            : tr("ABSOLUTE");
    statusBar()->showMessage(
        manualPair
            ? tr("Show Only Pair thủ công — %1 — giữ nguyên canonical result")
                  .arg(coordinateMode)
            : tr("Đã cô lập occurrence — %1 — dùng canonical occurrence identity")
                  .arg(coordinateMode));
    if (command == IsolationCommand::ShowOnlyPair && assemblyFeatureMode &&
        !stableIdA.empty() && !stableIdB.empty()) {
        startFeaturePairComparison(stableIdA, stableIdB);
    }
}

void MainWindow::restoreAssembly(const bool notify) {
    const bool assemblyFeatureMode =
        !(inputAIsSinglePart_ && inputBIsSinglePart_);
    if (assemblyFeatureMode && activeFeaturePair_) {
        if (comparisonRunner_ && comparisonRunner_->busy()) {
            static_cast<void>(comparisonRunner_->cancel());
        }
        activeFeaturePair_.reset();
        if (comparisonResult_) {
            comparisonResult_->report.features.clear();
            comparisonResults_->setAssemblyFeatureDeferred();
        }
    }
    viewer_->clearChangedFeatureHighlights();
    viewer_->clearIsolation();
    viewer_->clearAlignedLocations();
    pendingManualPairA_.reset();
    viewerControls_->setIsolationActive(false);
    if (notify) {
        viewer_->fitAll();
        statusBar()->showMessage(
            tr("Đã phục hồi toàn bộ Assembly; selection và canonical result được giữ nguyên"));
    }
}

void MainWindow::applyViewerState() {
    coordinateBanner_->setText(QString::fromLatin1(viewerState_.coordinateBanner().data(),
                                                    viewerState_.coordinateBanner().size()));
    viewer_->applyState(viewerState_);
}

}  // namespace stepcompare::gui
