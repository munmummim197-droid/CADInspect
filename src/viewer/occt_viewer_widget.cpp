#include <stepcompare/viewer/occt_viewer_widget.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_NeutralWindow.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Quantity_Color.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_TypeOfOrientation.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#include <QMouseEvent>
#include <QPaintEngine>
#include <QResizeEvent>
#include <QShowEvent>
#include <QString>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace stepcompare::viewer {
namespace {

V3d_TypeOfOrientation occtOrientation(const CameraOrientation orientation) {
    switch (orientation) {
        case CameraOrientation::Front:
            return V3d_TypeOfOrientation_Zup_Front;
        case CameraOrientation::Back:
            return V3d_TypeOfOrientation_Zup_Back;
        case CameraOrientation::Left:
            return V3d_TypeOfOrientation_Zup_Left;
        case CameraOrientation::Right:
            return V3d_TypeOfOrientation_Zup_Right;
        case CameraOrientation::Top:
            return V3d_TypeOfOrientation_Zup_Top;
        case CameraOrientation::Bottom:
            return V3d_TypeOfOrientation_Zup_Bottom;
        case CameraOrientation::Isometric:
            return V3d_TypeOfOrientation_Zup_AxoRight;
    }
    return V3d_TypeOfOrientation_Zup_AxoRight;
}

}  // namespace

class OcctViewerWidget::Impl final {
public:
    struct Entry final {
        Handle(AIS_Shape) presentation;
        ModelSide side{ModelSide::A};
        bool differs{};
        std::optional<TopLoc_Location> alignedLocation;
    };

    Handle(Aspect_DisplayConnection) displayConnection;
    Handle(OpenGl_GraphicDriver) graphicDriver;
    Handle(V3d_Viewer) viewer;
    Handle(V3d_View) view;
    Handle(AIS_InteractiveContext) context;
    Handle(Aspect_NeutralWindow) window;
    std::unordered_map<std::string, Entry> entries;
    std::unordered_set<std::string> differenceStableIds;
    QPoint mousePressPosition;
    QPoint lastMousePosition;
    Qt::MouseButtons pressedButtons{};
    ViewerStateModel state;
    std::function<void(std::string)> selectionChangedHandler;

    void initialize(const WId nativeWindowId, const int width, const int height) {
        displayConnection = new Aspect_DisplayConnection();
        graphicDriver = new OpenGl_GraphicDriver(displayConnection);
        viewer = new V3d_Viewer(graphicDriver);
        viewer->SetDefaultLights();
        viewer->SetLightOn();
        context = new AIS_InteractiveContext(viewer);
        context->SetDisplayMode(AIS_Shaded, false);
        view = viewer->CreateView();

        window = new Aspect_NeutralWindow();
        window->SetNativeHandle(reinterpret_cast<Aspect_Drawable>(nativeWindowId));
        window->SetSize(std::max(width, 1), std::max(height, 1));
        view->SetWindow(window);
        if (!window->IsMapped()) {
            window->Map();
        }
        view->SetBackgroundColor(Quantity_NOC_GRAY20);
        view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER,
                             Quantity_NOC_WHITE,
                             0.08,
                             V3d_ZBUFFER);
        view->SetProj(occtOrientation(state.orientation()));
    }

    void refreshPresentations() {
        const auto visibility = state.visibility();
        for (auto& [stableId, entry] : entries) {
            static_cast<void>(stableId);
            const bool sideVisible = entry.side == ModelSide::A ? visibility.showA
                                                                : visibility.showB;
            const bool visible = sideVisible && (!visibility.differencesOnly || entry.differs);
            if (visible) {
                context->Display(entry.presentation, false);
            } else {
                context->Erase(entry.presentation, false);
            }

            if (state.coordinates() == CoordinateMode::Aligned && entry.alignedLocation) {
                context->SetLocation(entry.presentation, *entry.alignedLocation);
            } else {
                context->ResetLocation(entry.presentation);
            }

            if (entry.differs) {
                context->SetColor(entry.presentation,
                                  entry.side == ModelSide::A ? Quantity_NOC_ORANGE
                                                             : Quantity_NOC_ORANGERED,
                                  false);
                context->UnsetTransparency(entry.presentation, false);
            } else if (entry.side == ModelSide::A) {
                context->SetColor(entry.presentation, Quantity_NOC_STEELBLUE, false);
                if (state.layer() == SceneLayer::Overlay) {
                    context->SetTransparency(entry.presentation, 0.35, false);
                } else {
                    context->UnsetTransparency(entry.presentation, false);
                }
            } else {
                context->SetColor(entry.presentation, Quantity_NOC_RED, false);
                if (state.layer() == SceneLayer::Overlay) {
                    context->SetTransparency(entry.presentation, 0.35, false);
                } else {
                    context->UnsetTransparency(entry.presentation, false);
                }
            }
        }
        context->UpdateCurrentViewer();
    }

    std::optional<std::string> selectedStableId() const {
        if (!context->HasDetected()) {
            return std::nullopt;
        }
        const auto detected = context->DetectedInteractive();
        for (const auto& [stableId, entry] : entries) {
            if (entry.presentation == detected) {
                return stableId;
            }
        }
        return std::nullopt;
    }
};

OcctViewerWidget::OcctViewerWidget(QWidget* parent)
    : QWidget(parent), impl_(std::make_unique<Impl>()) {
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    impl_->initialize(winId(), width(), height());
}

OcctViewerWidget::~OcctViewerWidget() = default;

void OcctViewerWidget::displayShape(const TopoDS_Shape& shape,
                                    const ModelSide side,
                                    StableSelectionId stableId,
                                    const bool differs) {
    const auto key = stableId.value();
    removeShape(stableId);
    if (differs) {
        impl_->differenceStableIds.emplace(key);
    }

    Handle(AIS_Shape) presentation = new AIS_Shape(shape);
    impl_->entries.emplace(key,
                           Impl::Entry{.presentation = presentation,
                                       .side = side,
                                       .differs = differs ||
                                                  impl_->differenceStableIds.contains(key),
                                       .alignedLocation = std::nullopt});
    impl_->refreshPresentations();
}

void OcctViewerWidget::removeShape(const StableSelectionId& stableId) {
    const auto found = impl_->entries.find(stableId.value());
    if (found == impl_->entries.end()) {
        return;
    }
    impl_->context->Remove(found->second.presentation, false);
    impl_->entries.erase(found);
    impl_->context->UpdateCurrentViewer();
}

void OcctViewerWidget::clearShapes(const ModelSide side) {
    for (auto iterator = impl_->entries.begin(); iterator != impl_->entries.end();) {
        if (iterator->second.side == side) {
            impl_->context->Remove(iterator->second.presentation, false);
            iterator = impl_->entries.erase(iterator);
        } else {
            ++iterator;
        }
    }
    impl_->context->UpdateCurrentViewer();
}

void OcctViewerWidget::clearShapes() {
    impl_->context->RemoveAll(false);
    impl_->entries.clear();
    impl_->context->UpdateCurrentViewer();
}

void OcctViewerWidget::setDifferenceState(const StableSelectionId& stableId,
                                          const bool differs) {
    if (differs) {
        impl_->differenceStableIds.emplace(stableId.value());
    } else {
        impl_->differenceStableIds.erase(stableId.value());
    }
    const auto found = impl_->entries.find(stableId.value());
    if (found == impl_->entries.end()) {
        return;
    }
    found->second.differs = differs;
    impl_->refreshPresentations();
}

void OcctViewerWidget::setDifferenceStates(
    const std::span<const StableSelectionId> changedStableIds) {
    std::unordered_set<std::string> changed;
    changed.reserve(changedStableIds.size());
    for (const auto& stableId : changedStableIds) {
        changed.emplace(stableId.value());
    }
    impl_->differenceStableIds = changed;
    for (auto& [stableId, entry] : impl_->entries) {
        entry.differs = changed.contains(stableId);
    }
    impl_->refreshPresentations();
}

void OcctViewerWidget::clearDifferenceStates() {
    impl_->differenceStableIds.clear();
    for (auto& [stableId, entry] : impl_->entries) {
        static_cast<void>(stableId);
        entry.differs = false;
    }
    impl_->refreshPresentations();
}

void OcctViewerWidget::setAlignedLocation(const StableSelectionId& stableId,
                                          const TopLoc_Location& bToA) {
    const auto found = impl_->entries.find(stableId.value());
    if (found == impl_->entries.end()) {
        return;
    }
    if (found->second.side == ModelSide::B) {
        found->second.alignedLocation = bToA;
    }
    impl_->refreshPresentations();
}

void OcctViewerWidget::clearAlignedLocation(const StableSelectionId& stableId) {
    const auto found = impl_->entries.find(stableId.value());
    if (found == impl_->entries.end()) {
        return;
    }
    found->second.alignedLocation.reset();
    impl_->refreshPresentations();
}

void OcctViewerWidget::applyState(const ViewerStateModel& state) {
    impl_->state = state;
    impl_->refreshPresentations();
}

void OcctViewerWidget::selectStableId(const StableSelectionId& stableId,
                                      const bool fitSelection) {
    const auto found = impl_->entries.find(stableId.value());
    if (found == impl_->entries.end()) {
        return;
    }
    impl_->context->SetSelected(found->second.presentation, true);
    if (fitSelection) {
        impl_->context->FitSelected(impl_->view);
    }
}

void OcctViewerWidget::clearSelection() {
    impl_->context->ClearSelected(true);
}

void OcctViewerWidget::setSelectionChangedHandler(
    std::function<void(std::string)> handler) {
    impl_->selectionChangedHandler = std::move(handler);
}

void OcctViewerWidget::fitAll() {
    impl_->view->FitAll();
}

void OcctViewerWidget::resetView() {
    impl_->state.setOrientation(CameraOrientation::Isometric);
    setCameraOrientation(CameraOrientation::Isometric);
    fitAll();
}

void OcctViewerWidget::setCameraOrientation(const CameraOrientation orientation) {
    impl_->state.setOrientation(orientation);
    impl_->view->SetProj(occtOrientation(orientation));
    impl_->view->FitAll();
}

QPaintEngine* OcctViewerWidget::paintEngine() const {
    return nullptr;
}

void OcctViewerWidget::paintEvent(QPaintEvent* event) {
    static_cast<void>(event);
    impl_->view->Redraw();
}

void OcctViewerWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    impl_->window->SetSize(std::max(event->size().width(), 1),
                           std::max(event->size().height(), 1));
    impl_->view->MustBeResized();
}

void OcctViewerWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // The first useful native client size is available only after the splitter
    // has laid out and shown this HWND.
    impl_->window->SetSize(std::max(width(), 1), std::max(height(), 1));
    impl_->view->MustBeResized();
}

void OcctViewerWidget::mousePressEvent(QMouseEvent* event) {
    impl_->mousePressPosition = event->position().toPoint();
    impl_->lastMousePosition = event->position().toPoint();
    impl_->pressedButtons = event->buttons();
    if (event->button() == Qt::LeftButton && !(event->modifiers() & Qt::ShiftModifier)) {
        impl_->view->StartRotation(impl_->lastMousePosition.x(),
                                   impl_->lastMousePosition.y());
    }
    event->accept();
}

void OcctViewerWidget::mouseMoveEvent(QMouseEvent* event) {
    const auto current = event->position().toPoint();
    const auto delta = current - impl_->lastMousePosition;
    if (impl_->pressedButtons & Qt::MiddleButton ||
        ((impl_->pressedButtons & Qt::LeftButton) &&
         (event->modifiers() & Qt::ShiftModifier))) {
        impl_->view->Pan(delta.x(), -delta.y());
    } else if (impl_->pressedButtons & Qt::RightButton) {
        const auto factor = std::exp(static_cast<double>(delta.y()) * -0.01);
        impl_->view->SetZoom(factor, true);
    } else if (impl_->pressedButtons & Qt::LeftButton) {
        impl_->view->Rotation(current.x(), current.y());
    } else {
        impl_->context->MoveTo(current.x(), current.y(), impl_->view, true);
    }
    impl_->lastMousePosition = current;
    event->accept();
}

void OcctViewerWidget::mouseReleaseEvent(QMouseEvent* event) {
    impl_->pressedButtons = event->buttons();
    const auto point = event->position().toPoint();
    if (event->button() == Qt::LeftButton &&
        (point - impl_->mousePressPosition).manhattanLength() <= 2) {
        impl_->context->MoveTo(point.x(), point.y(), impl_->view, false);
        impl_->context->SelectDetected();
        if (impl_->selectionChangedHandler) {
            impl_->selectionChangedHandler(
                impl_->selectedStableId().value_or(std::string{}));
        }
    }
    event->accept();
}

void OcctViewerWidget::wheelEvent(QWheelEvent* event) {
    const auto steps = static_cast<double>(event->angleDelta().y()) / 120.0;
    impl_->view->SetZoom(std::pow(1.15, steps), true);
    event->accept();
}

}  // namespace stepcompare::viewer
