#pragma once

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QString>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <stepcompare/reporting/report.hpp>
#include <stepcompare/viewer/selection_presenter.hpp>

namespace stepcompare::gui {

enum class OverallDisplayKind {
    Same,
    SameGeometryDifferentPosition,
    GeometryChanged,
    Ambiguous,
    Error,
};

struct OverallPresentation final {
    OverallDisplayKind kind{OverallDisplayKind::Ambiguous};
    QString title{};
    QString detail{};
};

[[nodiscard]] OverallPresentation presentOverallVerdict(
    const stepcompare::reporting::Report& report);

struct ParameterRow final {
    bool groupHeader{};
    QString parameter{};
    QString fileA{};
    QString fileB{};
    QString differenceBMinusA{};
    QString tolerance{};
    QString result{};
    QString tooltip{};
    bool numericValues{};
};

class ComparisonParameterModel final : public QAbstractTableModel {
public:
    enum Column {
        Parameter = 0,
        FileA,
        FileB,
        DifferenceBMinusA,
        Tolerance,
        Result,
        ColumnCount,
    };

    explicit ComparisonParameterModel(QObject* parent = nullptr);

    void setReport(const stepcompare::reporting::Report& report);
    void clearReport();

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(
        const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    [[nodiscard]] bool isGroupHeader(int row) const noexcept;

private:
    std::vector<ParameterRow> rows_;
};

enum class ComponentFilter {
    All,
    DifferencesOnly,
    Moved,
    Rotated,
    GeometryChanged,
    Missing,
    Added,
    Ambiguous,
};

enum class ComponentUiStatus {
    Unchanged,
    Moved,
    Rotated,
    MovedAndRotated,
    GeometryChanged,
    Missing,
    Added,
    Ambiguous,
};

struct ComponentViewRow final {
    QString component{};
    QString status{};
    QString deltaX{};
    QString deltaY{};
    QString deltaZ{};
    QString rotation{};
    QString maximumDeviation{};
    std::string stableIdA{};
    std::string stableIdB{};
    ComponentUiStatus uiStatus{ComponentUiStatus::Ambiguous};
    std::uint32_t filterMask{};
};

class ComponentComparisonModel final : public QAbstractTableModel {
public:
    enum Column {
        Component = 0,
        Status,
        DeltaX,
        DeltaY,
        DeltaZ,
        Rotation,
        MaximumDeviation,
        ColumnCount,
    };
    enum Role {
        FilterMaskRole = Qt::UserRole + 1,
        StableIdARole,
        StableIdBRole,
        UiStatusRole,
    };

    explicit ComponentComparisonModel(QObject* parent = nullptr);

    void setReport(const stepcompare::reporting::Report& report);
    void clearReport();

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(
        const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    [[nodiscard]] QModelIndex indexForStableId(std::string_view stableId) const;
    [[nodiscard]] std::string preferredStableId(const QModelIndex& index) const;

private:
    std::vector<ComponentViewRow> rows_;
};

class ComponentFilterProxyModel final : public QSortFilterProxyModel {
public:
    explicit ComponentFilterProxyModel(QObject* parent = nullptr);

    void setComponentFilter(ComponentFilter filter);
    [[nodiscard]] ComponentFilter componentFilter() const noexcept;

protected:
    [[nodiscard]] bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

private:
    ComponentFilter filter_{ComponentFilter::All};
};

struct FeatureSelectionTarget final {
    std::string ownerStableId{};
    std::vector<std::uint32_t> faceIndices{};
};

struct FeatureViewRow final {
    QString feature{};
    QString type{};
    QString fileA{};
    QString fileB{};
    QString differenceBMinusA{};
    QString tolerance{};
    QString result{};
    std::string ownerStableId{};
    std::vector<std::uint32_t> faceIndices{};
};

class FeatureComparisonModel final : public QAbstractTableModel {
public:
    enum Column {
        Feature = 0,
        Type,
        FileA,
        FileB,
        DifferenceBMinusA,
        Tolerance,
        Result,
        ColumnCount,
    };

    explicit FeatureComparisonModel(QObject* parent = nullptr);

    void setReport(const stepcompare::reporting::Report& report);
    void clearReport();

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(
        const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    [[nodiscard]] FeatureSelectionTarget selectionTarget(
        const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexForOwnerStableId(
        std::string_view stableId) const;

private:
    std::vector<FeatureViewRow> rows_;
};

[[nodiscard]] stepcompare::viewer::ComponentChangeKind componentChangeKind(
    const stepcompare::reporting::ComponentRow& component) noexcept;

}  // namespace stepcompare::gui
