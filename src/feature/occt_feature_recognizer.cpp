#include <stepcompare/feature/occt_feature_recognizer.hpp>

#include "adapters/occt/occt_geometry_payload.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Standard_Failure.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Ax1.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pln.hxx>
#include <gp_Torus.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace stepcompare::feature {
namespace {

struct CylinderCandidate final {
    std::uint32_t faceIndex{};
    gp_Ax1 axis;
    double radius{};
    double depth{};
    double uSpan{};
    gp_Pnt center;
    bool internal{};
    bool through{};
};

struct PlanarCandidate final {
    std::uint32_t faceIndex{};
    TopoDS_Face face;
    gp_Dir normal;
    double area{};
};

struct Bounds final {
    double minimum[3]{};
    double maximum[3]{};
};

Vector3 vector(const gp_Pnt& value) noexcept {
    return {value.X(), value.Y(), value.Z()};
}

Vector3 vector(const gp_Dir& value) noexcept {
    return {value.X(), value.Y(), value.Z()};
}

double shapeDiagonal(const TopoDS_Shape& shape) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box, false);
    if (box.IsVoid()) {
        return 0.0;
    }
    double xMin{};
    double yMin{};
    double zMin{};
    double xMax{};
    double yMax{};
    double zMax{};
    box.Get(xMin, yMin, zMin, xMax, yMax, zMax);
    return std::hypot(xMax - xMin, yMax - yMin, zMax - zMin);
}

std::optional<Bounds> shapeBounds(const TopoDS_Shape& shape) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box, false);
    if (box.IsVoid()) {
        return std::nullopt;
    }
    Bounds result;
    box.Get(result.minimum[0], result.minimum[1], result.minimum[2],
            result.maximum[0], result.maximum[1], result.maximum[2]);
    return result;
}

bool isInteriorPlanarFace(const gp_Pnt& center,
                          const gp_Dir& normal,
                          const Bounds& bounds,
                          const double tolerance) {
    const double normalComponents[3]{std::abs(normal.X()),
                                     std::abs(normal.Y()),
                                     std::abs(normal.Z())};
    const auto axis = static_cast<std::size_t>(
        std::max_element(std::begin(normalComponents),
                         std::end(normalComponents)) -
        std::begin(normalComponents));
    const double coordinate[3]{center.X(), center.Y(), center.Z()};
    const double clearance = std::max(tolerance, 1.0e-6);
    return coordinate[axis] > bounds.minimum[axis] + clearance &&
           coordinate[axis] < bounds.maximum[axis] - clearance;
}

std::pair<double, gp_Pnt> axialExtent(const TopoDS_Face& face,
                                      const gp_Ax1& axis) {
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    std::size_t count{};
    for (TopExp_Explorer explorer(face, TopAbs_VERTEX); explorer.More();
         explorer.Next()) {
        const gp_Pnt point = BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current()));
        const double projection = gp_Vec(axis.Location(), point).Dot(
            gp_Vec(axis.Direction()));
        minimum = std::min(minimum, projection);
        maximum = std::max(maximum, projection);
        ++count;
    }
    if (count == 0U || !std::isfinite(minimum) || !std::isfinite(maximum)) {
        return {0.0, axis.Location()};
    }
    const double midpoint = (minimum + maximum) * 0.5;
    return {maximum - minimum,
            axis.Location().Translated(gp_Vec(axis.Direction()) * midpoint)};
}

bool parallel(const gp_Dir& first, const gp_Dir& second, double angularRadians) {
    return first.IsParallel(second, std::max(angularRadians, 1.0e-6));
}

double axisDistance(const gp_Ax1& first, const gp_Ax1& second) {
    const gp_Vec delta(first.Location(), second.Location());
    return delta.Crossed(gp_Vec(first.Direction())).Magnitude();
}

double distanceToSegment(const gp_Pnt& point,
                         const gp_Pnt& start,
                         const gp_Pnt& end) {
    const gp_Vec segment(start, end);
    const double squaredLength = segment.SquareMagnitude();
    if (squaredLength <= 1.0e-18) {
        return point.Distance(start);
    }
    const double parameter = std::clamp(
        gp_Vec(start, point).Dot(segment) / squaredLength, 0.0, 1.0);
    return point.Distance(start.Translated(segment * parameter));
}

bool hasFullCircularBoundary(const TopoDS_Face& face) {
    for (TopExp_Explorer edge(face, TopAbs_EDGE); edge.More(); edge.Next()) {
        BRepAdaptor_Curve curve(TopoDS::Edge(edge.Current()));
        if (curve.GetType() == GeomAbs_Circle &&
            std::abs(curve.LastParameter() - curve.FirstParameter()) >
                1.75 * std::numbers::pi) {
            return true;
        }
    }
    return false;
}

bool hasBlindEndCap(
    const TopoDS_Face& cylinderFace,
    const double radius,
    const TopTools_IndexedDataMapOfShapeListOfShape& edgeFaces,
    const double linearToleranceMm) {
    const double expectedArea = std::numbers::pi * radius * radius;
    for (TopExp_Explorer edge(cylinderFace, TopAbs_EDGE); edge.More();
         edge.Next()) {
        if (!edgeFaces.Contains(edge.Current())) {
            continue;
        }
        const auto& adjacent = edgeFaces.FindFromKey(edge.Current());
        for (TopTools_ListIteratorOfListOfShape it(adjacent); it.More(); it.Next()) {
            if (it.Value().IsSame(cylinderFace) ||
                it.Value().ShapeType() != TopAbs_FACE) {
                continue;
            }
            const TopoDS_Face neighbor = TopoDS::Face(it.Value());
            BRepAdaptor_Surface surface(neighbor, true);
            if (surface.GetType() != GeomAbs_Plane) {
                continue;
            }
            std::size_t wireCount{};
            for (TopExp_Explorer wire(neighbor, TopAbs_WIRE); wire.More();
                 wire.Next()) {
                ++wireCount;
            }
            if (wireCount != 1U) {
                continue;
            }
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(neighbor, properties);
            const double allowed = std::max(
                expectedArea * 0.15,
                std::numbers::pi * linearToleranceMm *
                    std::max(radius, linearToleranceMm) * 4.0);
            if (std::abs(properties.Mass() - expectedArea) <= allowed) {
                return true;
            }
        }
    }
    return false;
}

RecognizedFeature cylinderFeature(const CylinderCandidate& candidate,
                                  FeatureType type,
                                  RecognitionEvidence evidence,
                                  double confidence,
                                  std::vector<std::uint32_t> faces) {
    RecognizedFeature result;
    result.stableId = "feature/face/" + std::to_string(candidate.faceIndex);
    result.type = type;
    result.evidence = evidence;
    result.confidence = confidence;
    result.centerLocalMm = vector(candidate.center);
    result.axis = vector(candidate.axis.Direction());
    result.primarySizeMm = candidate.radius * 2.0;
    result.depthMm = candidate.depth;
    result.radiusMm = candidate.radius;
    result.profile = type == FeatureType::Slot
                         ? "OBROUND"
                         : type == FeatureType::Fillet
                               ? "CYLINDRICAL_BLEND"
                               : "CIRCULAR";
    result.through = type == FeatureType::Fillet ? false : candidate.through;
    result.faceIndices = std::move(faces);
    return result;
}

bool shareEdge(const TopoDS_Face& first, const TopoDS_Face& second) {
    for (TopExp_Explorer left(first, TopAbs_EDGE); left.More(); left.Next()) {
        for (TopExp_Explorer right(second, TopAbs_EDGE); right.More();
             right.Next()) {
            if (left.Current().IsSame(right.Current())) {
                return true;
            }
        }
    }
    return false;
}

RecognizedFeature planarRecessFeature(
    const std::vector<PlanarCandidate>& planes,
    const std::vector<std::size_t>& component,
    const FeatureType type,
    const RecognitionEvidence evidence,
    const double confidence) {
    Bnd_Box box;
    std::vector<std::uint32_t> faceIndices;
    faceIndices.reserve(component.size());
    const PlanarCandidate* floor = nullptr;
    for (const auto index : component) {
        const auto& plane = planes[index];
        BRepBndLib::Add(plane.face, box, false);
        faceIndices.push_back(plane.faceIndex);
        if (floor == nullptr || plane.area > floor->area) {
            floor = &plane;
        }
    }
    std::sort(faceIndices.begin(), faceIndices.end());

    double xMin{};
    double yMin{};
    double zMin{};
    double xMax{};
    double yMax{};
    double zMax{};
    box.Get(xMin, yMin, zMin, xMax, yMax, zMax);
    const double spans[3]{xMax - xMin, yMax - yMin, zMax - zMin};
    const gp_Dir normal = floor == nullptr ? gp_Dir(0.0, 0.0, 1.0)
                                           : floor->normal;
    const double components[3]{std::abs(normal.X()),
                               std::abs(normal.Y()),
                               std::abs(normal.Z())};
    const auto depthAxis = static_cast<std::size_t>(
        std::max_element(std::begin(components), std::end(components)) -
        std::begin(components));
    std::vector<double> profileSpans;
    for (std::size_t axis = 0; axis < 3U; ++axis) {
        if (axis != depthAxis) {
            profileSpans.push_back(spans[axis]);
        }
    }
    std::sort(profileSpans.begin(), profileSpans.end());

    RecognizedFeature result;
    result.stableId = "feature/planar/" +
                      std::to_string(faceIndices.empty() ? 0U
                                                         : faceIndices.front());
    result.type = type;
    result.evidence = evidence;
    result.confidence = confidence;
    result.centerLocalMm = {
        (xMin + xMax) * 0.5,
        (yMin + yMax) * 0.5,
        (zMin + zMax) * 0.5,
    };
    // Feature placement is anchored at the opening/profile plane, not at the
    // middle of the removed volume. Changing pocket depth must therefore be a
    // depth change, not a false placement change.
    const double openingCoordinate =
        (depthAxis == 0U ? normal.X() : depthAxis == 1U ? normal.Y()
                                                         : normal.Z()) >= 0.0
            ? (depthAxis == 0U ? xMax : depthAxis == 1U ? yMax : zMax)
            : (depthAxis == 0U ? xMin : depthAxis == 1U ? yMin : zMin);
    if (depthAxis == 0U) {
        result.centerLocalMm.x = openingCoordinate;
    } else if (depthAxis == 1U) {
        result.centerLocalMm.y = openingCoordinate;
    } else {
        result.centerLocalMm.z = openingCoordinate;
    }
    result.axis = vector(normal);
    result.primarySizeMm = profileSpans.empty() ? 0.0 : profileSpans.front();
    result.secondarySizeMm = profileSpans.size() < 2U ? 0.0
                                                       : profileSpans.back();
    result.depthMm = spans[depthAxis];
    result.profile = type == FeatureType::Keyway
                         ? "RECTANGULAR_KEYWAY"
                         : evidence == RecognitionEvidence::GeometryProven
                               ? "RECTANGULAR_BLIND_POCKET"
                               : "PLANAR_RECESS_UNRESOLVED";
    result.through = false;
    result.faceIndices = std::move(faceIndices);
    return result;
}

}  // namespace

FeatureRecognitionResult OcctFeatureRecognizer::recognize(
    const import::GeometryPayloadPtr& geometry,
    const double linearToleranceMm,
    const double angularToleranceDegrees,
    const std::stop_token cancellation) noexcept {
    FeatureRecognitionResult result;
    if (cancellation.stop_requested()) {
        result.cancelled = true;
        result.diagnostics.push_back("FEATURE_RECOGNITION_CANCELLED");
        return result;
    }
    const TopoDS_Shape* shape =
        stepcompare::adapters::occt::tryGetShape(geometry);
    if (shape == nullptr || shape->IsNull() ||
        !std::isfinite(linearToleranceMm) || linearToleranceMm < 0.0 ||
        !std::isfinite(angularToleranceDegrees) ||
        angularToleranceDegrees < 0.0) {
        result.diagnostics.push_back("FEATURE_RECOGNITION_INVALID_INPUT");
        return result;
    }

    try {
        const double diagonal = std::max(shapeDiagonal(*shape), 1.0);
        const double angularRadians = std::max(
            angularToleranceDegrees * std::numbers::pi / 180.0, 1.0e-5);
        std::vector<CylinderCandidate> cylinders;
        std::vector<PlanarCandidate> interiorPlanarFaces;
        std::vector<RecognizedFeature> direct;
        std::uint32_t faceIndex{};
        bool hasExternalFullCylinder{};
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
        TopExp::MapShapesAndAncestors(*shape, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
        const auto bounds = shapeBounds(*shape);

        for (TopExp_Explorer explorer(*shape, TopAbs_FACE); explorer.More();
             explorer.Next()) {
            if (cancellation.stop_requested()) {
                result.cancelled = true;
                result.diagnostics.push_back("FEATURE_RECOGNITION_CANCELLED");
                return result;
            }
            ++faceIndex;
            const TopoDS_Face face = TopoDS::Face(explorer.Current());
            BRepAdaptor_Surface surface(face, true);
            const auto type = surface.GetType();
            if (type == GeomAbs_Cylinder) {
                const gp_Cylinder cylinder = surface.Cylinder();
                const auto [depth, center] = axialExtent(face, cylinder.Axis());
                const bool internal = face.Orientation() == TopAbs_REVERSED;
                GProp_GProps surfaceProperties;
                BRepGProp::SurfaceProperties(face, surfaceProperties);
                const double measuredSpan =
                    depth > std::max(linearToleranceMm, 1.0e-9) &&
                            cylinder.Radius() > 1.0e-9
                        ? surfaceProperties.Mass() /
                              (cylinder.Radius() * depth)
                        : 0.0;
                const double parameterSpan =
                    std::abs(surface.LastUParameter() -
                             surface.FirstUParameter());
                // Trimmed STEP faces may retain a 2π underlying cylinder even
                // when only a semicylindrical slot wall is present. Surface
                // area supplies the effective angular coverage and prevents a
                // slot end from being promoted to a false through hole.
                const double uSpan = hasFullCircularBoundary(face)
                                         ? 2.0 * std::numbers::pi
                                         : measuredSpan > 0.0
                                               ? std::min(measuredSpan,
                                                          parameterSpan)
                                               : parameterSpan;
                // STEP rarely preserves native hole history. A through hole is
                // therefore proven topologically: its cylindrical wall has no
                // single-wire planar disk closing either end. Geometry that
                // cannot satisfy this test is kept blind rather than promoted
                // to a false through-hole PASS.
                const bool through =
                    !hasBlindEndCap(face, cylinder.Radius(), edgeFaces,
                                    linearToleranceMm);
                cylinders.push_back({faceIndex,
                                     cylinder.Axis(),
                                     cylinder.Radius(),
                                     depth,
                                     uSpan,
                                     center,
                                     internal,
                                     through});
                hasExternalFullCylinder |=
                    !internal && uSpan > 1.75 * std::numbers::pi;
            } else if (type == GeomAbs_Torus) {
                const gp_Torus torus = surface.Torus();
                RecognizedFeature feature;
                feature.stableId = "feature/face/" + std::to_string(faceIndex);
                feature.type = FeatureType::Fillet;
                feature.evidence = RecognitionEvidence::GeometryProven;
                feature.confidence = 0.95;
                feature.centerLocalMm = vector(torus.Location());
                feature.axis = vector(torus.Axis().Direction());
                feature.radiusMm = torus.MinorRadius();
                feature.primarySizeMm = torus.MinorRadius() * 2.0;
                feature.profile = "TOROIDAL_BLEND";
                feature.faceIndices = {faceIndex};
                direct.push_back(std::move(feature));
            } else if (type == GeomAbs_Cone) {
                const gp_Cone cone = surface.Cone();
                const double angle = std::abs(cone.SemiAngle()) *
                                     180.0 / std::numbers::pi;
                if (angle > std::max(angularToleranceDegrees, 0.1) &&
                    angle < 89.9) {
                    RecognizedFeature feature;
                    feature.stableId =
                        "feature/face/" + std::to_string(faceIndex);
                    feature.type = FeatureType::Chamfer;
                    feature.evidence = RecognitionEvidence::GeometryProven;
                    feature.confidence = 0.92;
                    feature.centerLocalMm = vector(cone.Location());
                    feature.axis = vector(cone.Axis().Direction());
                    feature.radiusMm = cone.RefRadius();
                    feature.angleDegrees = angle;
                    feature.profile = "CONICAL_CHAMFER";
                    feature.faceIndices = {faceIndex};
                    direct.push_back(std::move(feature));
                }
            } else if (type == GeomAbs_Plane && bounds.has_value()) {
                GProp_GProps properties;
                BRepGProp::SurfaceProperties(face, properties);
                gp_Dir normal = surface.Plane().Axis().Direction();
                if (face.Orientation() == TopAbs_REVERSED) {
                    normal.Reverse();
                }
                if (isInteriorPlanarFace(properties.CentreOfMass(), normal,
                                         *bounds,
                                         std::max(linearToleranceMm,
                                                  diagonal * 1.0e-7))) {
                    interiorPlanarFaces.push_back(
                        {faceIndex, face, normal, properties.Mass()});
                }
            }
        }

        std::vector<bool> consumed(cylinders.size(), false);
        for (std::size_t first = 0; first < cylinders.size(); ++first) {
            if (cancellation.stop_requested()) {
                result.cancelled = true;
                result.diagnostics.push_back("FEATURE_RECOGNITION_CANCELLED");
                return result;
            }
            if (consumed[first] || !cylinders[first].internal ||
                cylinders[first].uSpan > 1.75 * std::numbers::pi) {
                continue;
            }
            for (std::size_t second = first + 1U;
                 second < cylinders.size();
                 ++second) {
                if (consumed[second] || !cylinders[second].internal ||
                    cylinders[second].uSpan > 1.75 * std::numbers::pi ||
                    std::abs(cylinders[first].radius -
                             cylinders[second].radius) >
                        std::max(linearToleranceMm, diagonal * 1.0e-5) ||
                    !parallel(cylinders[first].axis.Direction(),
                              cylinders[second].axis.Direction(),
                              angularRadians)) {
                    continue;
                }
                const double separation =
                    axisDistance(cylinders[first].axis, cylinders[second].axis);
                if (separation <= cylinders[first].radius * 1.5) {
                    continue;
                }
                auto slot = cylinderFeature(
                    cylinders[first],
                    FeatureType::Slot,
                    RecognitionEvidence::GeometryProven,
                    0.93,
                    {cylinders[first].faceIndex, cylinders[second].faceIndex});
                slot.secondarySizeMm =
                    separation + 2.0 * cylinders[first].radius;
                slot.centerLocalMm = {
                    (cylinders[first].center.X() +
                     cylinders[second].center.X()) * 0.5,
                    (cylinders[first].center.Y() +
                     cylinders[second].center.Y()) * 0.5,
                    (cylinders[first].center.Z() +
                     cylinders[second].center.Z()) * 0.5,
                };
                // Boolean STEP exporters may split one curved slot end into
                // several cylindrical faces, including a face whose underlying
                // surface still spans 2π. Consume collinear same-radius pieces
                // that lie on the proven slot segment so none can become a
                // false independent through-hole PASS.
                for (std::size_t other = 0; other < cylinders.size(); ++other) {
                    if (other == first || other == second || consumed[other] ||
                        !cylinders[other].internal ||
                        std::abs(cylinders[other].radius -
                                 cylinders[first].radius) >
                            std::max(linearToleranceMm, diagonal * 1.0e-5) ||
                        !parallel(cylinders[other].axis.Direction(),
                                  cylinders[first].axis.Direction(),
                                  angularRadians)) {
                        continue;
                    }
                    if (distanceToSegment(cylinders[other].center,
                                          cylinders[first].center,
                                          cylinders[second].center) <=
                        cylinders[first].radius * 1.5) {
                        consumed[other] = true;
                        slot.faceIndices.push_back(cylinders[other].faceIndex);
                    }
                }
                direct.push_back(std::move(slot));
                consumed[first] = consumed[second] = true;
                break;
            }
        }

        for (std::size_t first = 0; first < cylinders.size(); ++first) {
            if (cancellation.stop_requested()) {
                result.cancelled = true;
                result.diagnostics.push_back("FEATURE_RECOGNITION_CANCELLED");
                return result;
            }
            if (consumed[first] || !cylinders[first].internal ||
                cylinders[first].uSpan <= 1.75 * std::numbers::pi) {
                continue;
            }
            std::optional<std::size_t> coaxial;
            for (std::size_t second = first + 1U;
                 second < cylinders.size();
                 ++second) {
                if (consumed[second] || !cylinders[second].internal ||
                    cylinders[second].uSpan <= 1.75 * std::numbers::pi ||
                    !parallel(cylinders[first].axis.Direction(),
                              cylinders[second].axis.Direction(),
                              angularRadians) ||
                    axisDistance(cylinders[first].axis, cylinders[second].axis) >
                        std::max(linearToleranceMm, diagonal * 1.0e-5) ||
                    std::abs(cylinders[first].radius -
                             cylinders[second].radius) <=
                        std::max(linearToleranceMm, diagonal * 1.0e-5)) {
                    continue;
                }
                coaxial = second;
                break;
            }
            if (coaxial) {
                const auto& second = cylinders[*coaxial];
                const auto& larger = cylinders[first].radius > second.radius
                                         ? cylinders[first]
                                         : second;
                const auto& smaller = cylinders[first].radius > second.radius
                                          ? second
                                          : cylinders[first];
                auto counterbore = cylinderFeature(
                    larger,
                    FeatureType::Counterbore,
                    RecognitionEvidence::GeometryProven,
                    0.96,
                    {cylinders[first].faceIndex, second.faceIndex});
                counterbore.primarySizeMm = larger.radius * 2.0;
                counterbore.secondarySizeMm = smaller.radius * 2.0;
                counterbore.depthMm = larger.depth;
                counterbore.through = smaller.through;
                counterbore.profile = "STEPPED_CIRCULAR";
                direct.push_back(std::move(counterbore));
                consumed[first] = consumed[*coaxial] = true;
            }
        }

        for (std::size_t index = 0; index < cylinders.size(); ++index) {
            if (cancellation.stop_requested()) {
                result.cancelled = true;
                result.diagnostics.push_back("FEATURE_RECOGNITION_CANCELLED");
                return result;
            }
            if (consumed[index]) {
                continue;
            }
            const auto& cylinder = cylinders[index];
            if (cylinder.internal) {
                const FeatureType type = cylinder.through
                                             ? FeatureType::ThroughHole
                                             : FeatureType::BlindPocket;
                direct.push_back(cylinderFeature(
                    cylinder,
                    type,
                    RecognitionEvidence::GeometryProven,
                    cylinder.through ? 0.95 : 0.88,
                    {cylinder.faceIndex}));
            } else if (cylinder.uSpan < 1.75 * std::numbers::pi &&
                       cylinder.radius < diagonal * 0.20) {
                direct.push_back(cylinderFeature(
                    cylinder,
                    FeatureType::Fillet,
                    RecognitionEvidence::GeometryProven,
                    0.90,
                    {cylinder.faceIndex}));
            }
        }

        std::vector<bool> planarVisited(interiorPlanarFaces.size(), false);
        for (std::size_t seed = 0; seed < interiorPlanarFaces.size(); ++seed) {
            if (planarVisited[seed]) {
                continue;
            }
            if (cancellation.stop_requested()) {
                result.cancelled = true;
                result.diagnostics.push_back("FEATURE_RECOGNITION_CANCELLED");
                return result;
            }
            std::vector<std::size_t> component{seed};
            planarVisited[seed] = true;
            for (std::size_t cursor = 0; cursor < component.size(); ++cursor) {
                for (std::size_t candidate = 0;
                     candidate < interiorPlanarFaces.size(); ++candidate) {
                    if (!planarVisited[candidate] &&
                        shareEdge(interiorPlanarFaces[component[cursor]].face,
                                  interiorPlanarFaces[candidate].face)) {
                        planarVisited[candidate] = true;
                        component.push_back(candidate);
                    }
                }
            }
            if (component.size() < 3U) {
                continue;
            }
            const FeatureType type = hasExternalFullCylinder
                                         ? FeatureType::Keyway
                                         : FeatureType::BlindPocket;
            const std::size_t requiredFaces =
                type == FeatureType::Keyway ? 3U : 5U;
            const bool proven = component.size() >= requiredFaces;
            direct.push_back(planarRecessFeature(
                interiorPlanarFaces,
                component,
                type,
                proven ? RecognitionEvidence::GeometryProven
                       : RecognitionEvidence::Ambiguous,
                proven ? (type == FeatureType::Keyway ? 0.91 : 0.90) : 0.45));
            if (!proven) {
                result.diagnostics.push_back("FEATURE_AMBIGUOUS:PLANAR_RECESS");
            }
        }

        std::sort(direct.begin(), direct.end(), [](const auto& left,
                                                   const auto& right) {
            if (left.type != right.type) {
                return left.type < right.type;
            }
            return left.stableId < right.stableId;
        });
        result.features = std::move(direct);
        result.completed = true;
    } catch (const Standard_Failure&) {
        result.diagnostics.push_back("FEATURE_RECOGNITION_OCCT_FAILURE");
    } catch (...) {
        result.diagnostics.push_back("FEATURE_RECOGNITION_UNEXPECTED_FAILURE");
    }
    return result;
}

}  // namespace stepcompare::feature
