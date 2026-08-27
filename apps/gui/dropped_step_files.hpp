#pragma once

#include <QString>
#include <QStringList>

namespace stepcompare::gui {

enum class DroppedStepOpenTarget {
    Reject,
    CurrentA,
    CurrentB,
    CurrentPair,
    NewWindowA,
    NewWindowPair,
};

struct DroppedStepOpenPlan final {
    DroppedStepOpenTarget target{DroppedStepOpenTarget::Reject};
    QStringList paths{};
    QString rejectionReason{};

    [[nodiscard]] bool accepted() const noexcept {
        return target != DroppedStepOpenTarget::Reject;
    }
};

[[nodiscard]] bool isSupportedDroppedStepPath(const QString& path);

[[nodiscard]] DroppedStepOpenPlan planDroppedStepFiles(
    const QStringList& paths,
    bool hasInputA,
    bool hasInputB,
    bool operationBusy);

}  // namespace stepcompare::gui
