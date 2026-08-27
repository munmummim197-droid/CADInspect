#include <stepcompare/viewer/occt_viewer_widget.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_GradientFillMethod.hxx>
#include <Aspect_NeutralWindow.hxx>
#include <Aspect_TypeOfLine.hxx>
#include <Graphic3d_MaterialAspect.hxx>
#include <Graphic3d_ClipPlane.hxx>
#include <Graphic3d_NameOfMaterial.hxx>
#include <Graphic3d_RenderTransparentMethod.hxx>
#include <Graphic3d_RenderingParams.hxx>
#include <Graphic3d_TypeOfShadingModel.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Quantity_Color.hxx>
#include <TopLoc_Location.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <BRep_Builder.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
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
        std::optional<DeviationColor> deviationColor;
        std::optional<TopLoc_Location> alignedLocation;
        bool drawFeatureEdges{true};
        PresentationMode appliedMode{PresentationMode::ShadedWithEdges};
        bool appliedEdges{true};
    };

    Handle(Aspect_DisplayConnection) displayConnection;
    Handle(OpenGl_GraphicDriver) graphicDriver;
    Handle(V3d_Viewer) viewer;
    Handle(V3d_View) view;
    Handle(AIS_InteractiveContext) context;
    Handle(Aspect_NeutralWindow) window;
    Handle(Graphic3d_ClipPlane) sectionPlane;
    Handle(AIS_Shape) featureHighlight;
    std::unordered_map<std::string, Entry> entries;
    std::unordered_set<std::string> differenceStableIds;
    std::unordered_map<std::string, DeviationColor> deviationColors;
    bool deviationColoringRequested{};
    bool deviationColoringEnabled{};
    QPoint mousePressPosition;
    QPoint lastMousePosition;
    Qt::MouseButtons pressedButtons{};
    ViewerStateModel state;
    std::function<void(std::string)> selectionChangedHandler;

    void removeFeatureHighlight(const bool updateViewer) {
        if (!featureHighlight.IsNull()) {
            context->Remove(featureHighlight, updateViewer);
            featureHighlight.Nullify();
        }
    }

    void initialize(const WId nativeWindowId, const int width, const int height) {
        displayConnection = new Aspect_DisplayConnection();
        graphicDriver = new OpenGl_GraphicDriver(displayConnection);
        viewer = new V3d_Viewer(graphicDriver);
        viewer->SetDefaultShadingModel(Graphic3d_TypeOfShadingModel_Phong);
        viewer->SetDefaultLights();
        viewer->SetLightOn();
        context = new AIS_InteractiveContext(viewer);
        context->SetDisplayMode(AIS_Shaded, false);
        const auto selectionStyle = context->SelectionStyle();
        selectionStyle->SetColor(
            Quantity_Color(1.0, 0.82, 0.05, Quantity_TOC_RGB));
        selectionStyle->SetDisplayMode(AIS_WireFrame);
        selectionStyle->SetLineAspect(new Prs3d_LineAspect(
            Quantity_Color(1.0, 0.82, 0.05, Quantity_TOC_RGB),
            Aspect_TOL_SOLID,
            3.0));
        const auto hoverStyle = context->HighlightStyle();
        hoverStyle->SetColor(
            Quantity_Color(0.15, 0.95, 1.0, Quantity_TOC_RGB));
        hoverStyle->SetDisplayMode(AIS_WireFrame);
        hoverStyle->SetLineAspect(new Prs3d_LineAspect(
            Quantity_Color(0.15, 0.95, 1.0, Quantity_TOC_RGB),
            Aspect_TOL_SOLID,
            2.0));
        view = viewer->CreateView();
        auto& rendering = view->ChangeRenderingParams();
        rendering.NbMsaaSamples = 4;
        rendering.TransparencyMethod = Graphic3d_RTM_BLEND_OIT;
        rendering.ToEnableDepthPrepass = true;
        rendering.LineFeather = 0.8F;

        window = new Aspect_NeutralWindow();
        window->SetNativeHandle(reinterpret_cast<Aspect_Drawable>(nativeWindowId));
        window->SetSize(std::max(width, 1), std::max(height, 1));
        view->SetWindow(window);
        if (!window->IsMapped()) {
            window->Map();
        }
        view->SetBgGradientColors(
            Quantity_Color(0.16, 0.21, 0.28, Quantity_TOC_RGB),
            Quantity_Color(0.035, 0.055, 0.085, Quantity_TOC_RGB),
            Aspect_GradientFillMethod_Vertical,
            false);
        view->SetAutoZFitMode(true, 1.15);
        view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER,
                             Quantity_NOC_WHITE,
                             0.08,
                             V3d_ZBUFFER);
        view->SetProj(occtOrientation(state.orientation()));
    }

    void updateSectionPlane() {
        if (state.presentationMode() != PresentationMode::Section) {
            if (!sectionPlane.IsNull()) {
                view->RemoveClipPlane(sectionPlane);
                sectionPlane.Nullify();
            }
            return;
        }
        Bnd_Box box;
        const auto visibility = state.visibility();
        for (const auto& [stableId, entry] : entries) {
            static_cast<void>(stableId);
            const bool sideVisible = entry.side == ModelSide::A ? visibility.showA
                                                                : visibility.showB;
            if (!sideVisible || (visibility.differencesOnly && !entry.differs)) {
                continue;
            }
            TopoDS_Shape displayed = entry.presentation->Shape();
            if (state.coordinates() == CoordinateMode::Aligned &&
                entry.alignedLocation) {
                displayed = displayed.Moved(*entry.alignedLocation);
            }
            BRepBndLib::Add(displayed, box, false);
        }
        if (box.IsVoid()) {
            return;
        }
        double xMin{};
        double yMin{};
        double zMin{};
        double xMax{};
        double yMax{};
        double zMax{};
        box.Get(xMin, yMin, zMin, xMax, yMax, zMax);
        const gp_Pnt center((xMin + xMax) * 0.5,
                            (yMin + yMax) * 0.5,
                            (zMin + zMax) * 0.5);
        const gp_Dir normal(view->Camera()->Direction());
        if (sectionPlane.IsNull()) {
            sectionPlane = new Graphic3d_ClipPlane(gp_Pln(center, normal));
            sectionPlane->SetCapping(true);
            sectionPlane->SetCappingColor(
                Quantity_Color(0.95, 0.72, 0.20, Quantity_TOC_RGB));
            sectionPlane->SetCappingMaterial(
                Graphic3d_MaterialAspect(Graphic3d_NOM_SATIN));
            view->AddClipPlane(sectionPlane);
        } else {
            sectionPlane->SetEquation(gp_Pln(center, normal));
        }
    }

    void refreshPresentations() {
        updateSectionPlane();
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

            const bool overlay = state.layer() == SceneLayer::Overlay;
            const auto presentationMode = state.presentationMode();
            const bool wireframe = presentationMode == PresentationMode::Wireframe;
            const bool edges = presentationMode == PresentationMode::Section ||
                               (presentationMode ==
                                    PresentationMode::ShadedWithEdges &&
                                entry.drawFeatureEdges);
            if (entry.appliedMode != presentationMode ||
                entry.appliedEdges != edges) {
                context->SetDisplayMode(entry.presentation,
                                        wireframe ? AIS_WireFrame : AIS_Shaded,
                                        false);
                entry.presentation->Attributes()->SetFaceBoundaryDraw(edges);
                entry.presentation->Attributes()->SetUnFreeBoundaryDraw(edges);
                context->Redisplay(entry.presentation, false);
                entry.appliedMode = presentationMode;
                entry.appliedEdges = edges;
            }
            if (deviationColoringEnabled && entry.deviationColor) {
                const auto& rgb = entry.deviationColor->rgb;
                context->SetColor(
                    entry.presentation,
                    Quantity_Color(rgb.red, rgb.green, rgb.blue, Quantity_TOC_RGB),
                    false);
                if (overlay) {
                    context->SetTransparency(entry.presentation,
                                             entry.side == ModelSide::A ? 0.12
                                                                        : 0.30,
                                             false);
                } else {
                    context->UnsetTransparency(entry.presentation, false);
                }
            } else if (entry.differs) {
                context->SetColor(
                    entry.presentation,
                    entry.side == ModelSide::A
                        ? Quantity_Color(0.58, 0.30, 0.92, Quantity_TOC_RGB)
                        : Quantity_Color(1.00, 0.28, 0.08, Quantity_TOC_RGB),
                    false);
                if (overlay) {
                    context->SetTransparency(entry.presentation,
                                             entry.side == ModelSide::A ? 0.10
                                                                        : 0.24,
                                             false);
                } else {
                    context->UnsetTransparency(entry.presentation, false);
                }
            } else if (entry.side == ModelSide::A) {
                context->SetColor(
                    entry.presentation,
                    Quantity_Color(0.10, 0.58, 0.86, Quantity_TOC_RGB),
                    false);
                if (overlay) {
                    context->SetTransparency(entry.presentation, 0.14, false);
                } else {
                    context->UnsetTransparency(entry.presentation, false);
                }
            } else {
                context->SetColor(
                    entry.presentation,
                    Quantity_Color(1.00, 0.58, 0.12, Quantity_TOC_RGB),
                    false);
                if (overlay) {
                    context->SetTransparency(entry.presentation, 0.38, false);
                } else {
                    context->UnsetTransparency(entry.presentation, false);
                }
            }
            if (presentationMode == PresentationMode::TransparentXRay) {
                context->SetTransparency(entry.presentation, 0.68, false);
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
                                    const bool differs,
                                    const bool refresh,
                                    const bool drawFeatureEdges) {
    const auto key = stableId.value();
    removeShape(stableId);
    if (differs) {
        impl_->differenceStableIds.emplace(key);
    }

    Handle(AIS_Shape) presentation = new AIS_Shape(shape);
    presentation->SetMaterial(Graphic3d_MaterialAspect(Graphic3d_NOM_SATIN));
    presentation->Attributes()->SetFaceBoundaryDraw(drawFeatureEdges);
    presentation->Attributes()->SetUnFreeBoundaryDraw(drawFeatureEdges);
    presentation->Attributes()->SetFaceBoundaryAspect(new Prs3d_LineAspect(
        Quantity_Color(0.055, 0.075, 0.10, Quantity_TOC_RGB),
        Aspect_TOL_SOLID,
        0.8));
    const auto deviation = impl_->deviationColors.find(key);
    impl_->entries.emplace(key,
                           Impl::Entry{.presentation = presentation,
                                       .side = side,
                                       .differs = differs ||
                                                  impl_->differenceStableIds.contains(key),
                                       .deviationColor =
                                           deviation == impl_->deviationColors.end()
                                               ? std::nullopt
                                               : std::optional{deviation->second},
                                       .alignedLocation = std::nullopt,
                                       .drawFeatureEdges = drawFeatureEdges,
                                       .appliedMode = impl_->state.presentationMode(),
                                       .appliedEdges = drawFeatureEdges});
    if (refresh) {
        impl_->refreshPresentations();
    }
}

void OcctViewerWidget::refreshPresentations() {
    impl_->refreshPresentations();
}

void OcctViewerWidget::removeShape(const StableSelectionId& stableId) {
    const auto found = impl_->entries.find(stableId.value());
    if (found == impl_->entries.end()) {
        return;
    }
    // Geometry replacement invalidates any result-derived presentation data.
    impl_->deviationColors.clear();
    impl_->deviationColoringEnabled = false;
    impl_->removeFeatureHighlight(false);
    impl_->context->Remove(found->second.presentation, false);
    impl_->entries.erase(found);
    impl_->context->UpdateCurrentViewer();
}

void OcctViewerWidget::clearShapes(const ModelSide side) {
    impl_->deviationColors.clear();
    impl_->deviationColoringEnabled = false;
    impl_->removeFeatureHighlight(false);
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
    impl_->deviationColors.clear();
    impl_->deviationColoringEnabled = false;
    impl_->context->RemoveAll(false);
    impl_->featureHighlight.Nullify();
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

bool OcctViewerWidget::setDeviationColors(
    const std::span<const DeviationColorAssignment> assignments,
    const DeviationColorScale& scale) {
    if (assignments.empty()) {
        return false;
    }
    std::unordered_map<std::string, DeviationColor> next;
    next.reserve(assignments.size());
    for (const auto& assignment : assignments) {
        const auto mapped = mapDeviationToColor(assignment.maximumMm, scale);
        if (!mapped || !impl_->entries.contains(assignment.stableId.value()) ||
            !next.emplace(assignment.stableId.value(), *mapped).second) {
            return false;
        }
    }

    impl_->deviationColors = std::move(next);
    impl_->deviationColoringEnabled = impl_->deviationColoringRequested;
    for (auto& [stableId, entry] : impl_->entries) {
        const auto found = impl_->deviationColors.find(stableId);
        entry.deviationColor = found == impl_->deviationColors.end()
                                   ? std::nullopt
                                   : std::optional{found->second};
    }
    impl_->refreshPresentations();
    return true;
}

void OcctViewerWidget::clearDeviationColors() {
    impl_->deviationColors.clear();
    impl_->deviationColoringEnabled = false;
    for (auto& [stableId, entry] : impl_->entries) {
        static_cast<void>(stableId);
        entry.deviationColor.reset();
    }
    impl_->refreshPresentations();
}

void OcctViewerWidget::setDeviationColoringEnabled(const bool enabled) {
    impl_->deviationColoringRequested = enabled;
    impl_->deviationColoringEnabled = enabled && !impl_->deviationColors.empty();
    impl_->refreshPresentations();
}

bool OcctViewerWidget::deviationColoringEnabled() const noexcept {
    return impl_->deviationColoringEnabled;
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
    impl_->removeFeatureHighlight(false);
    impl_->context->ClearSelected(false);
    impl_->context->SetSelected(found->second.presentation, true);
    if (fitSelection) {
        impl_->context->FitSelected(impl_->view, 0.15, true);
        impl_->view->ZFitAll(1.15);
    }
}

void OcctViewerWidget::selectFeature(
    const StableSelectionId& ownerStableId,
    const std::span<const std::uint32_t> faceIndices,
    const bool fitSelection) {
    const auto found = impl_->entries.find(ownerStableId.value());
    if (found == impl_->entries.end() || faceIndices.empty()) {
        selectStableId(ownerStableId, fitSelection);
        return;
    }

    std::unordered_set<std::uint32_t> requested(faceIndices.begin(),
                                                faceIndices.end());
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    std::uint32_t faceIndex{};
    std::size_t selectedFaces{};
    for (TopExp_Explorer explorer(found->second.presentation->Shape(), TopAbs_FACE);
         explorer.More(); explorer.Next()) {
        ++faceIndex;
        if (requested.contains(faceIndex)) {
            builder.Add(compound, explorer.Current());
            ++selectedFaces;
        }
    }
    if (selectedFaces == 0U) {
        selectStableId(ownerStableId, fitSelection);
        return;
    }

    impl_->removeFeatureHighlight(false);
    impl_->context->ClearSelected(false);
    impl_->featureHighlight = new AIS_Shape(compound);
    impl_->featureHighlight->SetMaterial(
        Graphic3d_MaterialAspect(Graphic3d_NOM_SATIN));
    impl_->featureHighlight->SetColor(
        Quantity_Color(1.0, 0.82, 0.05, Quantity_TOC_RGB));
    impl_->featureHighlight->SetTransparency(0.12);
    impl_->featureHighlight->Attributes()->SetFaceBoundaryDraw(true);
    impl_->featureHighlight->Attributes()->SetFaceBoundaryAspect(
        new Prs3d_LineAspect(
            Quantity_Color(1.0, 0.92, 0.20, Quantity_TOC_RGB),
            Aspect_TOL_SOLID,
            3.0));
    impl_->context->Display(impl_->featureHighlight, false);
    if (impl_->state.coordinates() == CoordinateMode::Aligned &&
        found->second.alignedLocation) {
        impl_->context->SetLocation(impl_->featureHighlight,
                                    *found->second.alignedLocation);
    }
    impl_->context->SetSelected(impl_->featureHighlight, true);
    if (fitSelection) {
        impl_->context->FitSelected(impl_->view, 0.20, true);
        impl_->view->ZFitAll(1.15);
    } else {
        impl_->context->UpdateCurrentViewer();
    }
}

void OcctViewerWidget::clearSelection() {
    impl_->removeFeatureHighlight(false);
    impl_->context->ClearSelected(true);
}

void OcctViewerWidget::setSelectionChangedHandler(
    std::function<void(std::string)> handler) {
    impl_->selectionChangedHandler = std::move(handler);
}

void OcctViewerWidget::fitAll() {
    impl_->view->FitAll(0.08, true);
    impl_->view->ZFitAll(1.15);
}

void OcctViewerWidget::resetView() {
    impl_->state.setOrientation(CameraOrientation::Isometric);
    setCameraOrientation(CameraOrientation::Isometric);
    fitAll();
}

void OcctViewerWidget::setCameraOrientation(const CameraOrientation orientation) {
    impl_->state.setOrientation(orientation);
    impl_->view->SetProj(occtOrientation(orientation));
    fitAll();
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
