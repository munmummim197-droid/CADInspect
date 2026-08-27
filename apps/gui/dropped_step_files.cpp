#include "dropped_step_files.hpp"

#include <QFileInfo>
#include <QObject>

namespace stepcompare::gui {

bool isSupportedDroppedStepPath(const QString& path) {
    const QString suffix = QFileInfo(path).suffix();
    return suffix.compare(QStringLiteral("step"), Qt::CaseInsensitive) == 0 ||
           suffix.compare(QStringLiteral("stp"), Qt::CaseInsensitive) == 0;
}

DroppedStepOpenPlan planDroppedStepFiles(const QStringList& paths,
                                         const bool hasInputA,
                                         const bool hasInputB,
                                         const bool operationBusy) {
    if (paths.isEmpty() || paths.size() > 2) {
        return {.rejectionReason = QObject::tr(
                    "Chỉ hỗ trợ kéo-thả một file STEP hoặc một cặp A/B.")};
    }
    for (const auto& path : paths) {
        if (!isSupportedDroppedStepPath(path)) {
            return {.rejectionReason = QObject::tr(
                        "Chỉ hỗ trợ file .step và .stp.")};
        }
    }

    if (paths.size() == 2) {
        return {.target = operationBusy || hasInputA || hasInputB
                              ? DroppedStepOpenTarget::NewWindowPair
                              : DroppedStepOpenTarget::CurrentPair,
                .paths = paths};
    }
    if (operationBusy || (hasInputA && hasInputB)) {
        return {.target = DroppedStepOpenTarget::NewWindowA, .paths = paths};
    }
    return {.target = !hasInputA ? DroppedStepOpenTarget::CurrentA
                                 : DroppedStepOpenTarget::CurrentB,
            .paths = paths};
}

}  // namespace stepcompare::gui
