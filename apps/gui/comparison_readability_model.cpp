#include "comparison_readability_model.hpp"

#include <QBrush>
#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QLocale>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace stepcompare::gui {
namespace {

constexpr std::uint32_t kDifference = 1U << 0U;
constexpr std::uint32_t kMoved = 1U << 1U;
constexpr std::uint32_t kRotated = 1U << 2U;
constexpr std::uint32_t kGeometryChanged = 1U << 3U;
constexpr std::uint32_t kMissing = 1U << 4U;
constexpr std::uint32_t kAdded = 1U << 5U;
constexpr std::uint32_t kAmbiguous = 1U << 6U;

QString ui(const char* text) {
    return QCoreApplication::translate("ComparisonReadability", text);
}

const QLocale& displayLocale() {
    static const QLocale locale(QLocale::Vietnamese, QLocale::Vietnam);
    return locale;
}

QString fixedNumber(double value, const int precision) {
    if (!std::isfinite(value)) {
        return ui("Không có dữ liệu");
    }
    const double zeroThreshold = 0.5 * std::pow(10.0, -precision);
    if (std::abs(value) < zeroThreshold) {
        value = 0.0;
    }
    return displayLocale().toString(value, 'f', precision);
}

QString signedNumber(double value, const int precision) {
    const QString formatted = fixedNumber(value, precision);
    if (!std::isfinite(value) || value <= 0.0) {
        return formatted;
    }
    return QStringLiteral("+") + formatted;
}

QString metric(double value, const int precision, const QString& unit) {
    return QStringLiteral("%1 %2").arg(fixedNumber(value, precision), unit);
}

QString signedMetric(double value, const int precision, const QString& unit) {
    return QStringLiteral("%1 %2").arg(signedNumber(value, precision), unit);
}

QString integerMetric(const std::uint64_t value) {
    return displayLocale().toString(static_cast<qulonglong>(value));
}

QString signedIntegerMetric(const std::int64_t value) {
    const QString formatted = displayLocale().toString(static_cast<qlonglong>(value));
    return value > 0 ? QStringLiteral("+") + formatted : formatted;
}

QString passFail(const bool pass) {
    return pass ? QStringLiteral("PASS") : QStringLiteral("FAIL");
}

QString unavailable() {
    return QStringLiteral("—");
}

bool hasReason(const stepcompare::reporting::Report& report,
               const std::string_view reason) {
    return std::ranges::find(report.verdict.reasons, reason) !=
           report.verdict.reasons.end();
}

bool within(const double difference, const double tolerance) {
    return std::isfinite(difference) && std::isfinite(tolerance) &&
           std::abs(difference) <= tolerance;
}

QString shortenedHash(const std::string& hash) {
    const QString value = QString::fromStdString(hash);
    if (value.size() <= 20) {
        return value;
    }
    return value.first(16) + QStringLiteral("…") + value.last(8);
}

QString statusText(const ComponentUiStatus status) {
    switch (status) {
        case ComponentUiStatus::Unchanged:
            return ui("PASS — KHÔNG ĐỔI");
        case ComponentUiStatus::Moved:
            return ui("FAIL — MOVED");
        case ComponentUiStatus::Rotated:
            return ui("FAIL — ROTATED");
        case ComponentUiStatus::MovedAndRotated:
            return ui("FAIL — MOVED + ROTATED");
        case ComponentUiStatus::GeometryChanged:
            return ui("FAIL — HÌNH HỌC THAY ĐỔI");
        case ComponentUiStatus::Missing:
            return ui("FAIL — MISSING");
        case ComponentUiStatus::Added:
            return ui("FAIL — NEW");
        case ComponentUiStatus::Ambiguous:
            return ui("CHECK — MƠ HỒ");
    }
    return ui("CHECK — MƠ HỒ");
}

ComponentUiStatus componentStatus(
    const stepcompare::reporting::ComponentRow& component) {
    if (component.idA.empty()) {
        return ComponentUiStatus::Added;
    }
    if (component.idB.empty()) {
        return ComponentUiStatus::Missing;
    }
    if (component.geometryStatus == "DIFFERENT_PROVEN" ||
        component.positionStatus == "GEOMETRY_CHANGED") {
        return ComponentUiStatus::GeometryChanged;
    }
    if (component.matchStatus == "AMBIGUOUS" ||
        component.positionStatus == "UNKNOWN" ||
        component.geometryStatus == "INCONCLUSIVE") {
        return ComponentUiStatus::Ambiguous;
    }
    if (component.positionStatus == "MOVED_AND_ROTATED") {
        return ComponentUiStatus::MovedAndRotated;
    }
    if (component.positionStatus == "MOVED") {
        return ComponentUiStatus::Moved;
    }
    if (component.positionStatus == "ROTATED") {
        return ComponentUiStatus::Rotated;
    }
    return ComponentUiStatus::Unchanged;
}

std::uint32_t componentMask(const ComponentUiStatus status) {
    switch (status) {
        case ComponentUiStatus::Unchanged:
            return 0;
        case ComponentUiStatus::Moved:
            return kDifference | kMoved;
        case ComponentUiStatus::Rotated:
            return kDifference | kRotated;
        case ComponentUiStatus::MovedAndRotated:
            return kDifference | kMoved | kRotated;
        case ComponentUiStatus::GeometryChanged:
            return kDifference | kGeometryChanged;
        case ComponentUiStatus::Missing:
            return kDifference | kMissing;
        case ComponentUiStatus::Added:
            return kDifference | kAdded;
        case ComponentUiStatus::Ambiguous:
            return kDifference | kAmbiguous;
    }
    return kDifference | kAmbiguous;
}

std::uint32_t requiredMask(const ComponentFilter filter) {
    switch (filter) {
        case ComponentFilter::All:
            return 0;
        case ComponentFilter::DifferencesOnly:
            return kDifference;
        case ComponentFilter::Moved:
            return kMoved;
        case ComponentFilter::Rotated:
            return kRotated;
        case ComponentFilter::GeometryChanged:
            return kGeometryChanged;
        case ComponentFilter::Missing:
            return kMissing;
        case ComponentFilter::Added:
            return kAdded;
        case ComponentFilter::Ambiguous:
            return kAmbiguous;
    }
    return kAmbiguous;
}

QString componentName(const stepcompare::reporting::ComponentRow& component) {
    const QString a = QString::fromUtf8(component.nameA);
    const QString b = QString::fromUtf8(component.nameB);
    if (a.isEmpty()) {
        return b;
    }
    if (b.isEmpty() || a == b) {
        return a;
    }
    return QStringLiteral("%1 ↔ %2").arg(a, b);
}

std::string previewId(const char side, const std::string& id) {
    return id.empty() ? std::string{}
                      : std::string{"preview/"} + side + "/" + id;
}

QString vectorMetric(const stepcompare::reporting::Vector3& value,
                     const QString& unit = QStringLiteral("mm")) {
    return QStringLiteral("(%1; %2; %3) %4")
        .arg(fixedNumber(value.x, 4), fixedNumber(value.y, 4),
             fixedNumber(value.z, 4), unit);
}

QString featureTypeText(const std::string& type) {
    if (type == "THROUGH_HOLE") return ui("Through Hole / lỗ xuyên");
    if (type == "COUNTERBORE") return ui("Counterbore / lỗ bậc");
    if (type == "KEYWAY") return ui("Keyway / rãnh then");
    if (type == "SLOT") return ui("Slot / rãnh dài");
    if (type == "BLIND_POCKET") return ui("Blind Pocket / pocket âm");
    if (type == "FILLET") return ui("Fillet / mặt bo");
    if (type == "CHAMFER") return ui("Chamfer / mặt vát");
    return ui("Không xác định");
}

QString featureDescription(const stepcompare::reporting::FeatureRow& feature,
                           const bool sideA) {
    const auto& center = sideA ? feature.centerAAbsoluteMm
                               : feature.centerBAbsoluteMm;
    const auto& axis = sideA ? feature.axisA : feature.axisB;
    const double primary = sideA ? feature.primarySizeAMm : feature.primarySizeBMm;
    const double secondary = sideA ? feature.secondarySizeAMm
                                   : feature.secondarySizeBMm;
    const double depth = sideA ? feature.depthAMm : feature.depthBMm;
    const double radius = sideA ? feature.radiusAMm : feature.radiusBMm;
    const double angle = sideA ? feature.angleADegrees : feature.angleBDegrees;
    const auto& profile = sideA ? feature.profileA : feature.profileB;
    const bool through = sideA ? feature.throughA : feature.throughB;
    const bool exists = sideA ? !feature.idA.empty() : !feature.idB.empty();
    if (!exists) {
        return unavailable();
    }
    const bool throughStatusApplies =
        feature.type == "THROUGH_HOLE" || feature.type == "COUNTERBORE" ||
        feature.type == "KEYWAY" || feature.type == "SLOT" ||
        feature.type == "BLIND_POCKET";
    const QString throughText = throughStatusApplies
                                    ? (through ? ui("THROUGH") : ui("BLIND"))
                                    : ui("N/A");
    return QStringLiteral("Size %1 / %2 mm · sâu %3 mm · R %4 mm · góc %5°\n"
                          "Tâm %6 · trục (%7; %8; %9)\n%10 · %11")
        .arg(fixedNumber(primary, 4), fixedNumber(secondary, 4),
             fixedNumber(depth, 4), fixedNumber(radius, 4),
             fixedNumber(angle, 4), vectorMetric(center),
             fixedNumber(axis.x, 4), fixedNumber(axis.y, 4),
             fixedNumber(axis.z, 4),
             throughText,
             profile.empty() ? unavailable() : QString::fromUtf8(profile));
}

QString featureResultText(const stepcompare::reporting::FeatureRow& feature) {
    if (feature.result == "PASS" && feature.evidenceStatus == "GEOMETRY_PROVEN") {
        return ui("PASS — EVIDENCE HÌNH HỌC");
    }
    if (feature.result == "FAIL") {
        return QStringLiteral("FAIL — %1").arg(QString::fromUtf8(feature.reason));
    }
    return ui("CHECK / FEATURE_AMBIGUOUS");
}

QColor resultColor(const QString& value) {
    if (value.startsWith(QStringLiteral("PASS"))) {
        return QColor(QStringLiteral("#176b3a"));
    }
    if (value.startsWith(QStringLiteral("FAIL"))) {
        return QColor(QStringLiteral("#a12c1b"));
    }
    if (value.startsWith(QStringLiteral("CHECK"))) {
        return QColor(QStringLiteral("#8a5700"));
    }
    return QColor(QStringLiteral("#263746"));
}

}  // namespace

OverallPresentation presentOverallVerdict(
    const stepcompare::reporting::Report& report) {
    if (report.verdict.decision == "PASS") {
        return {OverallDisplayKind::Same,
                ui("✓ PASS — GIỐNG NHAU"),
                ui("Hình học và vị trí nằm trong dung sai.")};
    }
    if (report.verdict.decision == "ERROR") {
        return {OverallDisplayKind::Error,
                ui("⛔ ERROR"),
                ui("Không thể hoàn tất so sánh; xem chẩn đoán trong report.")};
    }
    if (report.verdict.decision == "CHECK") {
        return {OverallDisplayKind::Ambiguous,
                ui("⚠ CHECK / MƠ HỒ"),
                ui("Chưa đủ bằng chứng để kết luận PASS hoặc FAIL.")};
    }
    if (report.verdict.decision == "FAIL" &&
        hasReason(report, "SAME_GEOMETRY_POSITION_CHANGED")) {
        return {OverallDisplayKind::SameGeometryDifferentPosition,
                ui("✕ FAIL — CÙNG HÌNH HỌC / KHÁC VỊ TRÍ"),
                ui("Hình học giống nhau sau rigid alignment nhưng vị trí tuyệt đối khác.")};
    }
    return {OverallDisplayKind::GeometryChanged,
            ui("✕ FAIL — HÌNH HỌC THAY ĐỔI"),
            ui("Có thay đổi hình học hoặc thành phần assembly.")};
}

ComparisonParameterModel::ComparisonParameterModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void ComparisonParameterModel::setReport(
    const stepcompare::reporting::Report& report) {
    beginResetModel();
    rows_.clear();
    rows_.reserve(42);

    const auto group = [this](const char* name) {
        rows_.push_back({.groupHeader = true, .parameter = ui(name)});
    };
    const auto numeric = [this](QString parameter,
                                QString a,
                                QString b,
                                QString difference,
                                QString tolerance,
                                QString result) {
        rows_.push_back({.parameter = std::move(parameter),
                         .fileA = std::move(a),
                         .fileB = std::move(b),
                         .differenceBMinusA = std::move(difference),
                         .tolerance = std::move(tolerance),
                         .result = std::move(result),
                         .numericValues = true});
    };

    const QString mm = QStringLiteral("mm");
    const QString degrees = QStringLiteral("°");
    const QString mm2 = QStringLiteral("mm²");
    const QString mm3 = QStringLiteral("mm³");
    const QString positionTolerance = metric(report.tolerances.positionMm, 4, mm);
    const QString surfaceTolerance = metric(report.tolerances.surfaceMm, 4, mm);
    const QString angularTolerance = metric(report.tolerances.angularDegrees, 4, degrees);

    group("KÍCH THƯỚC");
    const std::array<std::pair<const char*, std::pair<double, double>>, 3> sizes{{
        {"Size X", {report.statisticsA.sizeMm.x, report.statisticsB.sizeMm.x}},
        {"Size Y", {report.statisticsA.sizeMm.y, report.statisticsB.sizeMm.y}},
        {"Size Z", {report.statisticsA.sizeMm.z, report.statisticsB.sizeMm.z}},
    }};
    for (const auto& [label, values] : sizes) {
        const double difference = values.second - values.first;
        numeric(ui(label), metric(values.first, 4, mm), metric(values.second, 4, mm),
                signedMetric(difference, 4, mm), surfaceTolerance,
                passFail(within(difference, report.tolerances.surfaceMm)));
    }

    group("VỊ TRÍ / TỌA ĐỘ");
    const std::array<std::pair<const char*, std::pair<double, double>>, 3> centers{{
        {"Center of Mass X", {report.statisticsA.centerOfMassMm.x,
                              report.statisticsB.centerOfMassMm.x}},
        {"Center of Mass Y", {report.statisticsA.centerOfMassMm.y,
                              report.statisticsB.centerOfMassMm.y}},
        {"Center of Mass Z", {report.statisticsA.centerOfMassMm.z,
                              report.statisticsB.centerOfMassMm.z}},
    }};
    for (const auto& [label, values] : centers) {
        const double difference = values.second - values.first;
        numeric(ui(label), metric(values.first, 4, mm), metric(values.second, 4, mm),
                signedMetric(difference, 4, mm), positionTolerance,
                passFail(within(difference, report.tolerances.positionMm)));
    }
    const std::array<std::pair<const char*, double>, 3> translations{{
        {"ΔX (B - A)", report.placement.translationBMinusAMm.x},
        {"ΔY (B - A)", report.placement.translationBMinusAMm.y},
        {"ΔZ (B - A)", report.placement.translationBMinusAMm.z},
    }};
    for (const auto& [label, value] : translations) {
        numeric(ui(label), unavailable(), unavailable(), signedMetric(value, 4, mm),
                positionTolerance,
                passFail(within(value, report.tolerances.positionMm)));
    }

    group("GÓC XOAY");
    const QString rotationResult = report.placement.ambiguousBySymmetry
                                       ? QStringLiteral("CHECK")
                                       : passFail(within(report.placement.rotationAngleDegrees,
                                                         report.tolerances.angularDegrees));
    numeric(ui("Rotation angle"), metric(0.0, 4, degrees),
            metric(report.placement.rotationAngleDegrees, 4, degrees),
            signedMetric(report.placement.rotationAngleDegrees, 4, degrees),
            angularTolerance, rotationResult);
    const std::array<std::pair<const char*, double>, 3> euler{{
        {"Rx", report.placement.displayEulerDegrees.x},
        {"Ry", report.placement.displayEulerDegrees.y},
        {"Rz", report.placement.displayEulerDegrees.z},
    }};
    for (const auto& [label, value] : euler) {
        numeric(ui(label), metric(0.0, 4, degrees),
                report.placement.ambiguousBySymmetry ? ui("Không có nghĩa do đối xứng")
                                                     : metric(value, 4, degrees),
                report.placement.ambiguousBySymmetry ? unavailable()
                                                     : signedMetric(value, 4, degrees),
                angularTolerance,
                report.placement.ambiguousBySymmetry
                    ? QStringLiteral("CHECK")
                    : passFail(within(value, report.tolerances.angularDegrees)));
    }

    group("HÌNH HỌC");
    const auto relativeTolerance = [&report](const double a, const double b) {
        return std::max({1.0, std::abs(a), std::abs(b)}) *
               report.tolerances.relativeProperty;
    };
    const double volumeDifference =
        report.statisticsB.volumeMm3 - report.statisticsA.volumeMm3;
    numeric(ui("Volume"), metric(report.statisticsA.volumeMm3, 3, mm3),
            metric(report.statisticsB.volumeMm3, 3, mm3),
            signedMetric(volumeDifference, 3, mm3),
            QStringLiteral("%1 %").arg(
                fixedNumber(report.tolerances.relativeProperty * 100.0, 6)),
            passFail(within(volumeDifference,
                            relativeTolerance(report.statisticsA.volumeMm3,
                                              report.statisticsB.volumeMm3))));
    const double areaDifference =
        report.statisticsB.surfaceAreaMm2 - report.statisticsA.surfaceAreaMm2;
    numeric(ui("Surface Area"), metric(report.statisticsA.surfaceAreaMm2, 3, mm2),
            metric(report.statisticsB.surfaceAreaMm2, 3, mm2),
            signedMetric(areaDifference, 3, mm2),
            QStringLiteral("%1 %").arg(
                fixedNumber(report.tolerances.relativeProperty * 100.0, 6)),
            passFail(within(areaDifference,
                            relativeTolerance(report.statisticsA.surfaceAreaMm2,
                                              report.statisticsB.surfaceAreaMm2))));

    const std::array<std::tuple<const char*, std::uint64_t, std::uint64_t>, 5> counts{{
        {"Solid", report.statisticsA.solidCount, report.statisticsB.solidCount},
        {"Shell", report.statisticsA.shellCount, report.statisticsB.shellCount},
        {"Face", report.statisticsA.faceCount, report.statisticsB.faceCount},
        {"Edge", report.statisticsA.edgeCount, report.statisticsB.edgeCount},
        {"Vertex", report.statisticsA.vertexCount, report.statisticsB.vertexCount},
    }};
    for (const auto& [label, a, b] : counts) {
        const auto difference = static_cast<std::int64_t>(b) -
                                static_cast<std::int64_t>(a);
        numeric(ui(label), integerMetric(a), integerMetric(b),
                signedIntegerMetric(difference), QStringLiteral("0"),
                passFail(difference == 0));
    }

    group("SAI LỆCH BỀ MẶT");
    const auto deviation = [&numeric, &report, &surfaceTolerance](
                               const char* label,
                               const double value) {
        numeric(ui(label), unavailable(), unavailable(),
                report.deepDeviation.available ? metric(value, 4, QStringLiteral("mm"))
                                               : unavailable(),
                surfaceTolerance,
                report.deepDeviation.available
                    ? passFail(within(value, report.tolerances.surfaceMm))
                    : QStringLiteral("CHECK"));
    };
    deviation("Max deviation", report.deepDeviation.maximumMm);
    deviation("Mean deviation", report.deepDeviation.meanMm);
    deviation("RMS deviation", report.deepDeviation.rmsMm);
    deviation("P95 deviation", report.deepDeviation.percentile95Mm);

    group("THÔNG TIN FILE");
    rows_.push_back({.parameter = ui("Đường dẫn"),
                     .fileA = QString::fromUtf8(report.inputA.pathUtf8),
                     .fileB = QString::fromUtf8(report.inputB.pathUtf8),
                     .differenceBMinusA = unavailable(),
                     .tolerance = unavailable(),
                     .result = ui("THÔNG TIN"),
                     .tooltip = QStringLiteral("A: %1\nB: %2")
                                    .arg(QString::fromUtf8(report.inputA.pathUtf8),
                                         QString::fromUtf8(report.inputB.pathUtf8))});
    rows_.push_back({.parameter = QStringLiteral("SHA-256"),
                     .fileA = shortenedHash(report.inputA.sha256),
                     .fileB = shortenedHash(report.inputB.sha256),
                     .differenceBMinusA = unavailable(),
                     .tolerance = unavailable(),
                     .result = report.inputA.sha256 == report.inputB.sha256
                                   ? ui("TRÙNG")
                                   : ui("KHÁC"),
                     .tooltip = QStringLiteral("A: %1\nB: %2")
                                    .arg(QString::fromStdString(report.inputA.sha256),
                                         QString::fromStdString(report.inputB.sha256))});
    const auto fileSizeDifference =
        static_cast<std::int64_t>(report.inputB.sizeBytes) -
        static_cast<std::int64_t>(report.inputA.sizeBytes);
    numeric(ui("Kích thước file"),
            QStringLiteral("%1 byte").arg(integerMetric(report.inputA.sizeBytes)),
            QStringLiteral("%1 byte").arg(integerMetric(report.inputB.sizeBytes)),
            QStringLiteral("%1 byte").arg(signedIntegerMetric(fileSizeDifference)),
            unavailable(), fileSizeDifference == 0 ? ui("TRÙNG") : ui("KHÁC"));

    endResetModel();
}

void ComparisonParameterModel::clearReport() {
    beginResetModel();
    rows_.clear();
    endResetModel();
}

int ComparisonParameterModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int ComparisonParameterModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ComparisonParameterModel::data(const QModelIndex& index,
                                        const int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(rows_.size()) || index.column() < 0 ||
        index.column() >= ColumnCount) {
        return {};
    }
    const auto& row = rows_[static_cast<std::size_t>(index.row())];
    if (role == Qt::DisplayRole) {
        if (row.groupHeader) {
            return index.column() == Parameter ? row.parameter : QString{};
        }
        const std::array<QString, ColumnCount> values{
            row.parameter, row.fileA, row.fileB, row.differenceBMinusA,
            row.tolerance, row.result};
        return values[static_cast<std::size_t>(index.column())];
    }
    if (role == Qt::ToolTipRole && !row.tooltip.isEmpty()) {
        return row.tooltip;
    }
    if (role == Qt::TextAlignmentRole) {
        if (row.groupHeader) {
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
        if (index.column() == Result) {
            return static_cast<int>(Qt::AlignCenter);
        }
        if (row.numericValues && index.column() >= FileA) {
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (role == Qt::BackgroundRole && row.groupHeader) {
        return QBrush(QColor(QStringLiteral("#dce8f2")));
    }
    if (role == Qt::FontRole && (row.groupHeader || index.column() == Result)) {
        QFont font;
        font.setBold(true);
        return font;
    }
    if (role == Qt::ForegroundRole && !row.groupHeader && index.column() == Result) {
        return QBrush(resultColor(row.result));
    }
    return {};
}

QVariant ComparisonParameterModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 ||
        section >= ColumnCount) {
        return {};
    }
    const std::array<QString, ColumnCount> headers{
        ui("Thông số"), ui("File A"), ui("File B"), ui("Sai lệch B - A"),
        ui("Dung sai"), ui("Kết quả")};
    return headers[static_cast<std::size_t>(section)];
}

Qt::ItemFlags ComparisonParameterModel::flags(const QModelIndex& index) const {
    if (!index.isValid() || isGroupHeader(index.row())) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool ComparisonParameterModel::isGroupHeader(const int row) const noexcept {
    return row >= 0 && row < static_cast<int>(rows_.size()) &&
           rows_[static_cast<std::size_t>(row)].groupHeader;
}

ComponentComparisonModel::ComponentComparisonModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void ComponentComparisonModel::setReport(
    const stepcompare::reporting::Report& report) {
    beginResetModel();
    rows_.clear();
    rows_.reserve(report.components.size());
    for (const auto& component : report.components) {
        const ComponentUiStatus status = componentStatus(component);
        rows_.push_back({
            .component = componentName(component),
            .status = statusText(status),
            .deltaX = signedMetric(component.translationBMinusAMm.x, 4,
                                   QStringLiteral("mm")),
            .deltaY = signedMetric(component.translationBMinusAMm.y, 4,
                                   QStringLiteral("mm")),
            .deltaZ = signedMetric(component.translationBMinusAMm.z, 4,
                                   QStringLiteral("mm")),
            .rotation = metric(component.rotationAngleDegrees, 4, QStringLiteral("°")),
            .maximumDeviation = component.deviation.available
                                    ? metric(component.deviation.maximumMm, 4,
                                             QStringLiteral("mm"))
                                    : unavailable(),
            .stableIdA = previewId('A', component.idA),
            .stableIdB = previewId('B', component.idB),
            .uiStatus = status,
            .filterMask = componentMask(status),
        });
    }
    endResetModel();
}

void ComponentComparisonModel::clearReport() {
    beginResetModel();
    rows_.clear();
    endResetModel();
}

int ComponentComparisonModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int ComponentComparisonModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ComponentComparisonModel::data(const QModelIndex& index,
                                        const int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(rows_.size()) || index.column() < 0 ||
        index.column() >= ColumnCount) {
        return {};
    }
    const auto& row = rows_[static_cast<std::size_t>(index.row())];
    if (role == Qt::DisplayRole) {
        const std::array<QString, ColumnCount> values{
            row.component, row.status, row.deltaX, row.deltaY,
            row.deltaZ, row.rotation, row.maximumDeviation};
        return values[static_cast<std::size_t>(index.column())];
    }
    if (role == FilterMaskRole) {
        return static_cast<qulonglong>(row.filterMask);
    }
    if (role == StableIdARole) {
        return QString::fromStdString(row.stableIdA);
    }
    if (role == StableIdBRole) {
        return QString::fromStdString(row.stableIdB);
    }
    if (role == UiStatusRole) {
        return static_cast<int>(row.uiStatus);
    }
    if (role == Qt::TextAlignmentRole) {
        if (index.column() >= DeltaX) {
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        if (index.column() == Status) {
            return static_cast<int>(Qt::AlignCenter);
        }
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (role == Qt::ForegroundRole && index.column() == Status) {
        return QBrush(resultColor(row.status));
    }
    if (role == Qt::FontRole && index.column() == Status) {
        QFont font;
        font.setBold(true);
        return font;
    }
    if (role == Qt::BackgroundRole && row.filterMask != 0) {
        return QBrush(QColor(QStringLiteral("#fff8ec")));
    }
    return {};
}

QVariant ComponentComparisonModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 ||
        section >= ColumnCount) {
        return {};
    }
    const std::array<QString, ColumnCount> headers{
        ui("Component"), ui("Status"), QStringLiteral("ΔX"), QStringLiteral("ΔY"),
        QStringLiteral("ΔZ"), ui("Rotation"), ui("Max deviation")};
    return headers[static_cast<std::size_t>(section)];
}

Qt::ItemFlags ComponentComparisonModel::flags(const QModelIndex& index) const {
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                           : Qt::NoItemFlags;
}

QModelIndex ComponentComparisonModel::indexForStableId(
    const std::string_view stableId) const {
    for (std::size_t row = 0; row < rows_.size(); ++row) {
        if (rows_[row].stableIdA == stableId || rows_[row].stableIdB == stableId) {
            return index(static_cast<int>(row), Component);
        }
    }
    return {};
}

std::string ComponentComparisonModel::preferredStableId(
    const QModelIndex& indexValue) const {
    if (!indexValue.isValid() || indexValue.row() < 0 ||
        indexValue.row() >= static_cast<int>(rows_.size())) {
        return {};
    }
    const auto& row = rows_[static_cast<std::size_t>(indexValue.row())];
    return row.stableIdA.empty() ? row.stableIdB : row.stableIdA;
}

ComponentFilterProxyModel::ComponentFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
}

void ComponentFilterProxyModel::setComponentFilter(const ComponentFilter filter) {
    if (filter_ == filter) {
        return;
    }
    beginFilterChange();
    filter_ = filter;
    endFilterChange(Direction::Rows);
}

ComponentFilter ComponentFilterProxyModel::componentFilter() const noexcept {
    return filter_;
}

FeatureComparisonModel::FeatureComparisonModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void FeatureComparisonModel::setReport(
    const stepcompare::reporting::Report& report) {
    beginResetModel();
    rows_.clear();
    rows_.reserve(report.features.size());
    std::size_t ordinal = 0;
    for (const auto& feature : report.features) {
        ++ordinal;
        const bool selectA = !feature.ownerComponentIdA.empty();
        const auto& absolute = feature.absoluteDifferenceBMinusAMm;
        const auto& aligned = feature.alignedDifferenceBMinusAMm;
        rows_.push_back({
            .feature = ui("Feature %1").arg(static_cast<qulonglong>(ordinal)),
            .type = featureTypeText(feature.type),
            .fileA = featureDescription(feature, true),
            .fileB = featureDescription(feature, false),
            .differenceBMinusA =
                QStringLiteral("ABS Δ %1\nALIGNED Δ %2")
                    .arg(vectorMetric(absolute), vectorMetric(aligned)),
            .tolerance = QStringLiteral("%1 mm / %2°")
                             .arg(fixedNumber(feature.positionToleranceMm, 4),
                                  fixedNumber(feature.angularToleranceDegrees, 4)),
            .result = featureResultText(feature),
            .ownerStableId = previewId(selectA ? 'A' : 'B',
                                       selectA ? feature.ownerComponentIdA
                                               : feature.ownerComponentIdB),
            .faceIndices = selectA ? feature.faceIndicesA : feature.faceIndicesB,
        });
    }
    endResetModel();
}

void FeatureComparisonModel::clearReport() {
    beginResetModel();
    rows_.clear();
    endResetModel();
}

int FeatureComparisonModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int FeatureComparisonModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant FeatureComparisonModel::data(const QModelIndex& index,
                                      const int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(rows_.size()) || index.column() < 0 ||
        index.column() >= ColumnCount) {
        return {};
    }
    const auto& row = rows_[static_cast<std::size_t>(index.row())];
    if (role == Qt::DisplayRole) {
        const std::array<QString, ColumnCount> values{
            row.feature, row.type, row.fileA, row.fileB,
            row.differenceBMinusA, row.tolerance, row.result};
        return values[static_cast<std::size_t>(index.column())];
    }
    if (role == Qt::TextAlignmentRole) {
        if (index.column() == Tolerance) {
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        if (index.column() == Result) {
            return static_cast<int>(Qt::AlignCenter);
        }
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (role == Qt::ForegroundRole && index.column() == Result) {
        return QBrush(resultColor(row.result));
    }
    if (role == Qt::FontRole && index.column() == Result) {
        QFont font;
        font.setBold(true);
        return font;
    }
    if (role == Qt::ToolTipRole) {
        return QStringLiteral("%1\n%2\n%3")
            .arg(row.fileA, row.fileB, row.differenceBMinusA);
    }
    return {};
}

QVariant FeatureComparisonModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 ||
        section >= ColumnCount) {
        return {};
    }
    const std::array<QString, ColumnCount> headers{
        ui("Feature"), ui("Type"), ui("File A"), ui("File B"),
        ui("Difference B-A"), ui("Tolerance"), ui("Result")};
    return headers[static_cast<std::size_t>(section)];
}

Qt::ItemFlags FeatureComparisonModel::flags(const QModelIndex& index) const {
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                           : Qt::NoItemFlags;
}

FeatureSelectionTarget FeatureComparisonModel::selectionTarget(
    const QModelIndex& indexValue) const {
    if (!indexValue.isValid() || indexValue.row() < 0 ||
        indexValue.row() >= static_cast<int>(rows_.size())) {
        return {};
    }
    const auto& row = rows_[static_cast<std::size_t>(indexValue.row())];
    return {.ownerStableId = row.ownerStableId, .faceIndices = row.faceIndices};
}

QModelIndex FeatureComparisonModel::indexForOwnerStableId(
    const std::string_view stableId) const {
    for (std::size_t row = 0; row < rows_.size(); ++row) {
        if (rows_[row].ownerStableId == stableId) {
            return index(static_cast<int>(row), Feature);
        }
    }
    return {};
}

bool ComponentFilterProxyModel::filterAcceptsRow(
    const int sourceRow,
    const QModelIndex& sourceParent) const {
    if (filter_ == ComponentFilter::All) {
        return true;
    }
    const auto value = sourceModel()->index(sourceRow, 0, sourceParent)
                           .data(ComponentComparisonModel::FilterMaskRole)
                           .toULongLong();
    return (value & requiredMask(filter_)) != 0;
}

stepcompare::viewer::ComponentChangeKind componentChangeKind(
    const stepcompare::reporting::ComponentRow& component) noexcept {
    using stepcompare::viewer::ComponentChangeKind;
    switch (componentStatus(component)) {
        case ComponentUiStatus::Unchanged:
            return ComponentChangeKind::Unchanged;
        case ComponentUiStatus::GeometryChanged:
            return ComponentChangeKind::GeometryChanged;
        case ComponentUiStatus::Moved:
        case ComponentUiStatus::MovedAndRotated:
            return ComponentChangeKind::Moved;
        case ComponentUiStatus::Rotated:
            return ComponentChangeKind::Rotated;
        case ComponentUiStatus::Added:
            return ComponentChangeKind::Added;
        case ComponentUiStatus::Missing:
            return ComponentChangeKind::Missing;
        case ComponentUiStatus::Ambiguous:
            return ComponentChangeKind::Ambiguous;
    }
    return ComponentChangeKind::Ambiguous;
}

}  // namespace stepcompare::gui
