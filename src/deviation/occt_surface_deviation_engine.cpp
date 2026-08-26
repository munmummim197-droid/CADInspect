#include <stepcompare/deviation/occt_surface_deviation_engine.hpp>

#include "adapters/occt/occt_geometry_payload.hpp"

#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace stepcompare::deviation {
namespace {

constexpr double pi = 3.141592653589793238462643383279502884;

class TriangulationError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Vector3 final {
    double x{};
    double y{};
    double z{};
};

[[nodiscard]] constexpr Vector3 operator+(const Vector3& left,
                                          const Vector3& right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] constexpr Vector3 operator-(const Vector3& left,
                                          const Vector3& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] constexpr Vector3 operator*(const Vector3& value,
                                          double scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] constexpr double dot(const Vector3& left,
                                   const Vector3& right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] constexpr double squaredLength(const Vector3& value) noexcept {
    return dot(value, value);
}

[[nodiscard]] Vector3 toVector(const gp_Pnt& point) noexcept {
    return {point.X(), point.Y(), point.Z()};
}

struct Bounds final {
    Vector3 minimum{std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity()};
    Vector3 maximum{-std::numeric_limits<double>::infinity(),
                    -std::numeric_limits<double>::infinity(),
                    -std::numeric_limits<double>::infinity()};

    void include(const Vector3& point) noexcept {
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        minimum.z = std::min(minimum.z, point.z);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
        maximum.z = std::max(maximum.z, point.z);
    }

    void include(const Bounds& other) noexcept {
        include(other.minimum);
        include(other.maximum);
    }
};

struct Triangle final {
    Vector3 a{};
    Vector3 b{};
    Vector3 c{};
    Bounds bounds{};
    Vector3 centroid{};
};

[[nodiscard]] Triangle triangle(Vector3 a, Vector3 b, Vector3 c) {
    Triangle value{a, b, c};
    value.bounds.include(a);
    value.bounds.include(b);
    value.bounds.include(c);
    value.centroid = (a + b + c) * (1.0 / 3.0);
    return value;
}

[[nodiscard]] double squaredDistanceToBounds(const Vector3& point,
                                             const Bounds& bounds) noexcept {
    const auto axisDistance = [](double value, double minimum, double maximum) {
        if (value < minimum) {
            return minimum - value;
        }
        if (value > maximum) {
            return value - maximum;
        }
        return 0.0;
    };
    const double x = axisDistance(point.x, bounds.minimum.x, bounds.maximum.x);
    const double y = axisDistance(point.y, bounds.minimum.y, bounds.maximum.y);
    const double z = axisDistance(point.z, bounds.minimum.z, bounds.maximum.z);
    return x * x + y * y + z * z;
}

[[nodiscard]] double squaredDistanceToSegment(const Vector3& point,
                                              const Vector3& start,
                                              const Vector3& end) noexcept {
    const auto segment = end - start;
    const auto lengthSquared = squaredLength(segment);
    if (lengthSquared <= std::numeric_limits<double>::epsilon()) {
        return squaredLength(point - start);
    }
    const auto position =
        std::clamp(dot(point - start, segment) / lengthSquared, 0.0, 1.0);
    return squaredLength(point - (start + segment * position));
}

// Closest-point regions from Real-Time Collision Detection, with an explicit
// degenerate fallback for imperfect CAD triangulations.
[[nodiscard]] double squaredDistanceToTriangle(const Vector3& point,
                                               const Triangle& value) noexcept {
    const auto ab = value.b - value.a;
    const auto ac = value.c - value.a;
    const auto ap = point - value.a;
    const double d1 = dot(ab, ap);
    const double d2 = dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) {
        return squaredLength(ap);
    }

    const auto bp = point - value.b;
    const double d3 = dot(ab, bp);
    const double d4 = dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3) {
        return squaredLength(bp);
    }

    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        const double position = d1 / (d1 - d3);
        return squaredLength(point - (value.a + ab * position));
    }

    const auto cp = point - value.c;
    const double d5 = dot(ab, cp);
    const double d6 = dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6) {
        return squaredLength(cp);
    }

    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        const double position = d2 / (d2 - d6);
        return squaredLength(point - (value.a + ac * position));
    }

    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        const auto edge = value.c - value.b;
        const double position =
            (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return squaredLength(point - (value.b + edge * position));
    }

    const double denominator = va + vb + vc;
    if (!std::isfinite(denominator) ||
        std::abs(denominator) <= std::numeric_limits<double>::epsilon()) {
        return std::min({squaredDistanceToSegment(point, value.a, value.b),
                         squaredDistanceToSegment(point, value.b, value.c),
                         squaredDistanceToSegment(point, value.c, value.a)});
    }
    const double inverse = 1.0 / denominator;
    const double v = vb * inverse;
    const double w = vc * inverse;
    return squaredLength(point - (value.a + ab * v + ac * w));
}

class Cancellation final {
public:
    explicit Cancellation(const std::function<bool()>& callback)
        : callback_(callback) {}

    [[nodiscard]] bool poll() const {
        return callback_ && callback_();
    }

private:
    const std::function<bool()>& callback_;
};

class TriangleBvh final {
public:
    explicit TriangleBvh(const std::vector<Triangle>& triangles)
        : triangles_(triangles), indices_(triangles.size()) {
        std::iota(indices_.begin(), indices_.end(), std::size_t{});
        nodes_.reserve(triangles.size() * 2U);
    }

    [[nodiscard]] bool build(const Cancellation& cancellation) {
        if (triangles_.empty() || cancellation.poll()) {
            return false;
        }
        static_cast<void>(buildNode(0, indices_.size(), cancellation));
        return !cancelled_;
    }

    [[nodiscard]] double nearestSquared(const Vector3& point,
                                        const Cancellation& cancellation,
                                        std::size_t& evaluations) const {
        double best = std::numeric_limits<double>::infinity();
        nearestNode(0, point, cancellation, best, evaluations);
        return best;
    }

    [[nodiscard]] bool cancelled() const noexcept { return queryCancelled_; }

private:
    struct Node final {
        Bounds bounds{};
        std::size_t begin{};
        std::size_t end{};
        std::size_t left{invalidIndex};
        std::size_t right{invalidIndex};

        static constexpr std::size_t invalidIndex =
            std::numeric_limits<std::size_t>::max();
        [[nodiscard]] bool leaf() const noexcept {
            return left == invalidIndex;
        }
    };

    [[nodiscard]] std::size_t buildNode(std::size_t begin, std::size_t end,
                                        const Cancellation& cancellation) {
        if (cancellation.poll()) {
            cancelled_ = true;
            return Node::invalidIndex;
        }
        Node node{};
        node.begin = begin;
        node.end = end;
        Bounds centroidBounds{};
        for (std::size_t index = begin; index < end; ++index) {
            node.bounds.include(triangles_[indices_[index]].bounds);
            centroidBounds.include(triangles_[indices_[index]].centroid);
        }
        const auto nodeIndex = nodes_.size();
        nodes_.push_back(node);
        constexpr std::size_t leafSize = 2;
        if (end - begin <= leafSize) {
            return nodeIndex;
        }

        const Vector3 extent = centroidBounds.maximum - centroidBounds.minimum;
        std::size_t axis = 0;
        if (extent.y > extent.x && extent.y >= extent.z) {
            axis = 1;
        } else if (extent.z > extent.x && extent.z > extent.y) {
            axis = 2;
        }
        const auto coordinate = [this, axis](std::size_t triangleIndex) {
            const auto& center = triangles_[triangleIndex].centroid;
            return axis == 0 ? center.x : (axis == 1 ? center.y : center.z);
        };
        const auto middle = begin + (end - begin) / 2U;
        std::nth_element(indices_.begin() + static_cast<std::ptrdiff_t>(begin),
                         indices_.begin() + static_cast<std::ptrdiff_t>(middle),
                         indices_.begin() + static_cast<std::ptrdiff_t>(end),
                         [&coordinate](std::size_t left, std::size_t right) {
                             return coordinate(left) < coordinate(right);
                         });
        const auto left = buildNode(begin, middle, cancellation);
        const auto right = buildNode(middle, end, cancellation);
        nodes_[nodeIndex].left = left;
        nodes_[nodeIndex].right = right;
        return nodeIndex;
    }

    void nearestNode(std::size_t nodeIndex, const Vector3& point,
                     const Cancellation& cancellation, double& best,
                     std::size_t& evaluations) const {
        if (queryCancelled_ || cancellation.poll()) {
            queryCancelled_ = true;
            return;
        }
        const auto& node = nodes_[nodeIndex];
        if (squaredDistanceToBounds(point, node.bounds) > best) {
            return;
        }
        if (node.leaf()) {
            for (std::size_t index = node.begin; index < node.end; ++index) {
                ++evaluations;
                best = std::min(best, squaredDistanceToTriangle(
                                          point, triangles_[indices_[index]]));
            }
            return;
        }

        const auto leftDistance =
            squaredDistanceToBounds(point, nodes_[node.left].bounds);
        const auto rightDistance =
            squaredDistanceToBounds(point, nodes_[node.right].bounds);
        if (leftDistance <= rightDistance) {
            nearestNode(node.left, point, cancellation, best, evaluations);
            if (rightDistance <= best) {
                nearestNode(node.right, point, cancellation, best, evaluations);
            }
        } else {
            nearestNode(node.right, point, cancellation, best, evaluations);
            if (leftDistance <= best) {
                nearestNode(node.left, point, cancellation, best, evaluations);
            }
        }
    }

    const std::vector<Triangle>& triangles_;
    std::vector<std::size_t> indices_;
    std::vector<Node> nodes_;
    bool cancelled_{};
    mutable bool queryCancelled_{};
};

void diagnostic(SurfaceDeviationResult& result,
                SurfaceDeviationDiagnosticCode code, std::string message) {
    result.diagnostics.push_back({code, std::move(message)});
}

[[nodiscard]] bool validOptions(const SurfaceDeviationOptions& options) {
    return std::isfinite(options.toleranceMm) && options.toleranceMm >= 0.0 &&
           std::isfinite(options.meshDeflectionMm) &&
           options.meshDeflectionMm > 0.0 &&
           std::isfinite(options.meshAngularDeflectionDegrees) &&
           options.meshAngularDeflectionDegrees > 0.0 &&
           options.meshAngularDeflectionDegrees <= 180.0 &&
           std::isfinite(options.percentile) && options.percentile > 0.0 &&
           options.percentile <= 100.0 &&
           options.maximumSamplesPerDirection > 0;
}

[[nodiscard]] bool toRigidTransform(const import::RigidTransformMm& source,
                                    gp_Trsf& destination) {
    if (!std::all_of(source.matrix.begin(), source.matrix.end(),
                     [](double value) { return std::isfinite(value); })) {
        return false;
    }
    constexpr double tolerance = 1.0e-9;
    if (std::abs(source.matrix[12]) > tolerance ||
        std::abs(source.matrix[13]) > tolerance ||
        std::abs(source.matrix[14]) > tolerance ||
        std::abs(source.matrix[15] - 1.0) > tolerance) {
        return false;
    }
    for (std::size_t first = 0; first < 3; ++first) {
        for (std::size_t second = 0; second < 3; ++second) {
            double product = 0.0;
            for (std::size_t row = 0; row < 3; ++row) {
                product += source.matrix[row * 4 + first] *
                           source.matrix[row * 4 + second];
            }
            const auto expected = first == second ? 1.0 : 0.0;
            if (std::abs(product - expected) > tolerance) {
                return false;
            }
        }
    }
    const auto& m = source.matrix;
    const double determinant =
        m[0] * (m[5] * m[10] - m[6] * m[9]) -
        m[1] * (m[4] * m[10] - m[6] * m[8]) +
        m[2] * (m[4] * m[9] - m[5] * m[8]);
    if (std::abs(determinant - 1.0) > tolerance) {
        return false;
    }
    destination.SetValues(m[0], m[1], m[2], m[3],
                          m[4], m[5], m[6], m[7],
                          m[8], m[9], m[10], m[11]);
    return true;
}

[[nodiscard]] TopoDS_Shape meshedCopy(
    const TopoDS_Shape& source, const gp_Trsf* transform,
    const SurfaceDeviationOptions& options) {
    TopoDS_Shape copy = BRepBuilderAPI_Copy(source, true, false).Shape();
    if (transform != nullptr) {
        copy = BRepBuilderAPI_Transform(copy, *transform, true).Shape();
    }
    BRepTools::Clean(copy);
    const double angularRadians =
        options.meshAngularDeflectionDegrees * pi / 180.0;
    BRepMesh_IncrementalMesh mesh(copy,
                                  options.meshDeflectionMm,
                                  false,
                                  angularRadians,
                                  false);
    if (!mesh.IsDone()) {
        throw TriangulationError(
            "OCCT controlled triangulation did not finish");
    }
    return copy;
}

[[nodiscard]] std::vector<Triangle> extractTriangles(
    const TopoDS_Shape& shape, const Cancellation& cancellation,
    bool& cancelled) {
    std::vector<Triangle> triangles;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More();
         explorer.Next()) {
        if (cancellation.poll()) {
            cancelled = true;
            return {};
        }
        const TopoDS_Face& face = TopoDS::Face(explorer.Current());
        TopLoc_Location location;
        const Handle(Poly_Triangulation) triangulation =
            BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) {
            continue;
        }
        const gp_Trsf localToWorld = location.Transformation();
        for (int index = 1;
             index <= triangulation->NbTriangles(); ++index) {
            if ((index & 255) == 0 && cancellation.poll()) {
                cancelled = true;
                return {};
            }
            int first{};
            int second{};
            int third{};
            triangulation->Triangle(index).Get(first, second, third);
            gp_Pnt a = triangulation->Node(first);
            gp_Pnt b = triangulation->Node(second);
            gp_Pnt c = triangulation->Node(third);
            a.Transform(localToWorld);
            b.Transform(localToWorld);
            c.Transform(localToWorld);
            triangles.push_back(triangle(toVector(a), toVector(b), toVector(c)));
        }
    }
    return triangles;
}

[[nodiscard]] std::vector<Vector3> makeSamples(
    const std::vector<Triangle>& triangles, std::size_t maximumSamples) {
    if (triangles.empty()) {
        return {};
    }
    const auto maximumSafeTriangles =
        std::numeric_limits<std::size_t>::max() / 4U;
    const auto total = triangles.size() > maximumSafeTriangles
                           ? std::numeric_limits<std::size_t>::max()
                           : triangles.size() * 4U;
    const auto count = std::min(total, maximumSamples);
    std::vector<Vector3> samples;
    samples.reserve(count);
    for (std::size_t sampleIndex = 0; sampleIndex < count; ++sampleIndex) {
        const auto ordinal = static_cast<std::size_t>(
            static_cast<long double>(sampleIndex) *
            static_cast<long double>(total) /
            static_cast<long double>(count));
        const auto& source = triangles[ordinal / 4U];
        switch (ordinal % 4U) {
        case 0:
            samples.push_back(source.a);
            break;
        case 1:
            samples.push_back(source.b);
            break;
        case 2:
            samples.push_back(source.c);
            break;
        default:
            samples.push_back(source.centroid);
            break;
        }
    }
    return samples;
}

[[nodiscard]] bool appendDistances(const std::vector<Vector3>& samples,
                                   const TriangleBvh& target,
                                   const Cancellation& cancellation,
                                   std::vector<double>& distances,
                                   std::size_t& evaluations,
                                   std::size_t& processedSamples) {
    for (std::size_t index = 0; index < samples.size(); ++index) {
        if ((index & 63U) == 0U && cancellation.poll()) {
            return false;
        }
        const auto squared =
            target.nearestSquared(samples[index], cancellation, evaluations);
        if (target.cancelled()) {
            return false;
        }
        if (!std::isfinite(squared) || squared < 0.0) {
            throw std::runtime_error("BVH nearest-triangle query failed");
        }
        distances.push_back(std::sqrt(squared));
        ++processedSamples;
    }
    return true;
}

[[nodiscard]] SurfaceDeviationResult compareImpl(
    const SurfaceDeviationRequest& request) {
    SurfaceDeviationResult result{};
    const Cancellation cancellation(request.isCancelled);
    if (cancellation.poll()) {
        result.status = SurfaceDeviationStatus::Cancelled;
        diagnostic(result,
                   SurfaceDeviationDiagnosticCode::Cancelled,
                   "Surface deviation was cancelled before processing");
        return result;
    }
    if (!validOptions(request.options)) {
        diagnostic(result,
                   SurfaceDeviationDiagnosticCode::InvalidOptions,
                   "Surface deviation options must be finite and within their "
                   "documented positive ranges");
        return result;
    }

    const TopoDS_Shape* sourceA =
        adapters::occt::tryGetShape(request.geometryA);
    const TopoDS_Shape* sourceB =
        adapters::occt::tryGetShape(request.geometryB);
    if (sourceA == nullptr || sourceB == nullptr || sourceA->IsNull() ||
        sourceB->IsNull()) {
        diagnostic(result,
                   SurfaceDeviationDiagnosticCode::MissingGeometryPayload,
                   "Surface deviation requires two non-null OCCT payloads");
        return result;
    }

    gp_Trsf transformBToA;
    if (!toRigidTransform(request.transformBToA, transformBToA)) {
        diagnostic(result,
                   SurfaceDeviationDiagnosticCode::InvalidAlignmentTransform,
                   "B-to-A alignment must be a finite proper rigid transform");
        return result;
    }

    const TopoDS_Shape shapeA = meshedCopy(*sourceA, nullptr, request.options);
    if (cancellation.poll()) {
        result.status = SurfaceDeviationStatus::Cancelled;
        diagnostic(result, SurfaceDeviationDiagnosticCode::Cancelled,
                   "Surface deviation was cancelled after meshing A");
        return result;
    }
    const TopoDS_Shape shapeB =
        meshedCopy(*sourceB, &transformBToA, request.options);
    if (cancellation.poll()) {
        result.status = SurfaceDeviationStatus::Cancelled;
        diagnostic(result, SurfaceDeviationDiagnosticCode::Cancelled,
                   "Surface deviation was cancelled after meshing B");
        return result;
    }

    bool cancelled = false;
    const auto trianglesA = extractTriangles(shapeA, cancellation, cancelled);
    if (cancelled) {
        result.status = SurfaceDeviationStatus::Cancelled;
        diagnostic(result, SurfaceDeviationDiagnosticCode::Cancelled,
                   "Surface deviation was cancelled while extracting A");
        return result;
    }
    const auto trianglesB = extractTriangles(shapeB, cancellation, cancelled);
    if (cancelled) {
        result.status = SurfaceDeviationStatus::Cancelled;
        diagnostic(result, SurfaceDeviationDiagnosticCode::Cancelled,
                   "Surface deviation was cancelled while extracting B");
        return result;
    }
    result.trianglesA = trianglesA.size();
    result.trianglesB = trianglesB.size();
    if (trianglesA.empty() || trianglesB.empty()) {
        result.status = SurfaceDeviationStatus::NoSurfaceData;
        diagnostic(result,
                   SurfaceDeviationDiagnosticCode::NoTriangles,
                   "One or both payloads contain no triangulatable faces");
        return result;
    }

    TriangleBvh bvhA(trianglesA);
    TriangleBvh bvhB(trianglesB);
    if (!bvhA.build(cancellation) || !bvhB.build(cancellation)) {
        result.status = SurfaceDeviationStatus::Cancelled;
        diagnostic(result, SurfaceDeviationDiagnosticCode::Cancelled,
                   "Surface deviation was cancelled while building BVHs");
        return result;
    }

    const auto samplesA = makeSamples(
        trianglesA, request.options.maximumSamplesPerDirection);
    const auto samplesB = makeSamples(
        trianglesB, request.options.maximumSamplesPerDirection);
    std::vector<double> distances;
    distances.reserve(samplesA.size() + samplesB.size());
    if (!appendDistances(samplesA, bvhB, cancellation, distances,
                         result.triangleDistanceEvaluations,
                         result.samplesAToB) ||
        !appendDistances(samplesB, bvhA, cancellation, distances,
                         result.triangleDistanceEvaluations,
                         result.samplesBToA)) {
        result.status = SurfaceDeviationStatus::Cancelled;
        diagnostic(result, SurfaceDeviationDiagnosticCode::Cancelled,
                   "Surface deviation was cancelled during nearest queries");
        return result;
    }

    const double sum =
        std::accumulate(distances.begin(), distances.end(), 0.0);
    double sumSquares = 0.0;
    for (const double distance : distances) {
        sumSquares += distance * distance;
    }
    result.maximumMm = *std::max_element(distances.begin(), distances.end());
    result.meanMm = sum / static_cast<double>(distances.size());
    result.rmsMm =
        std::sqrt(sumSquares / static_cast<double>(distances.size()));
    std::sort(distances.begin(), distances.end());
    const auto rank = static_cast<std::size_t>(std::ceil(
        request.options.percentile * static_cast<double>(distances.size()) /
        100.0));
    result.percentileMm = distances[std::max<std::size_t>(rank, 1U) - 1U];
    result.status = result.maximumMm <= request.options.toleranceMm
                        ? SurfaceDeviationStatus::WithinTolerance
                        : SurfaceDeviationStatus::DeviationFound;
    return result;
}

}  // namespace

SurfaceDeviationResult OcctSurfaceDeviationEngine::compare(
    const SurfaceDeviationRequest& request) noexcept {
    try {
        return compareImpl(request);
    } catch (const Standard_Failure& failure) {
        SurfaceDeviationResult result{};
        diagnostic(result,
                   SurfaceDeviationDiagnosticCode::OcctFailure,
                   failure.what() != nullptr ? failure.what()
                                             : "Unspecified OCCT failure");
        return result;
    } catch (const TriangulationError& failure) {
        SurfaceDeviationResult result{};
        diagnostic(result,
                   SurfaceDeviationDiagnosticCode::TriangulationFailed,
                   failure.what());
        return result;
    } catch (const std::exception& failure) {
        SurfaceDeviationResult result{};
        diagnostic(result,
                   SurfaceDeviationDiagnosticCode::UnexpectedFailure,
                   failure.what());
        return result;
    } catch (...) {
        SurfaceDeviationResult result{};
        diagnostic(result,
                   SurfaceDeviationDiagnosticCode::UnexpectedFailure,
                   "Unknown failure in surface deviation adapter");
        return result;
    }
}

}  // namespace stepcompare::deviation
