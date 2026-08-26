#pragma once

#include "stepcompare/import/step_import_port.hpp"

namespace stepcompare::import {

class OcctStepImporter final : public StepImportPort {
public:
    [[nodiscard]] StepImportResult importStep(
        const StepImportRequest& request) noexcept override;
};

}  // namespace stepcompare::import
