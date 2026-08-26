#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <stepcompare/viewer/viewer_state.hpp>

namespace stepcompare::viewer {

enum class PreviewLoadPhase {
    Idle,
    Queued,
    Importing,
    PreparingScene,
    Completed,
    Failed,
    Cancelled,
};

struct PreviewLoadStatus final {
    PreviewLoadPhase phase{PreviewLoadPhase::Idle};
    ModelSide side{ModelSide::A};
    std::u8string sourcePathUtf8;
    std::string messageUtf8{"Ready"};
    int percent{};
    bool cancellable{};
    bool cancelRequested{};
    std::uint64_t generation{};
};

class PreviewLoadStateModel final {
public:
    [[nodiscard]] std::uint64_t begin(ModelSide side, std::u8string sourcePathUtf8);
    void markImporting(std::uint64_t generation);
    void updateProgress(std::uint64_t generation,
                        int percent,
                        std::string messageUtf8);
    void markPreparingScene(std::uint64_t generation);
    void complete(std::uint64_t generation, std::string messageUtf8);
    void fail(std::uint64_t generation, std::string messageUtf8);
    [[nodiscard]] bool requestCancel() noexcept;
    void cancel(std::uint64_t generation) noexcept;

    [[nodiscard]] const PreviewLoadStatus& status() const noexcept;
    [[nodiscard]] bool accepts(std::uint64_t generation) const noexcept;

private:
    [[nodiscard]] bool active() const noexcept;

    PreviewLoadStatus status_;
    std::uint64_t nextGeneration_{};
};

[[nodiscard]] std::string_view toString(PreviewLoadPhase phase) noexcept;

}  // namespace stepcompare::viewer
