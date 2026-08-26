#include <stepcompare/viewer/preview_load_state.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void unicodeAndProgressContract() {
    using namespace stepcompare::viewer;
    PreviewLoadStateModel state;
    const auto generation = state.begin(ModelSide::A, u8"D:/Dự án/Bản vẽ/Chi tiết 01.step");
    expect(state.status().phase == PreviewLoadPhase::Queued, "load must begin queued");
    expect(state.status().sourcePathUtf8 == u8"D:/Dự án/Bản vẽ/Chi tiết 01.step",
           "UTF-8 source path must survive state round trip");
    expect(state.status().cancellable, "queued load must be cancellable");

    state.markImporting(generation);
    state.updateProgress(generation, 42, "Reading STEP");
    expect(state.status().phase == PreviewLoadPhase::Importing,
           "active load must report importing phase");
    expect(state.status().percent == 42, "progress percentage must update");
    state.markPreparingScene(generation);
    state.complete(generation, "Preview ready");
    expect(state.status().phase == PreviewLoadPhase::Completed,
           "successful job must complete");
    expect(state.status().percent == 100 && !state.status().cancellable,
           "completed job must be terminal at 100 percent");
}

void cancellationAndStaleGenerationContract() {
    using namespace stepcompare::viewer;
    PreviewLoadStateModel state;
    const auto oldGeneration = state.begin(ModelSide::A, u8"A.step");
    const auto activeGeneration = state.begin(ModelSide::B, u8"B.step");
    state.fail(oldGeneration, "stale failure");
    expect(state.status().generation == activeGeneration &&
               state.status().phase == PreviewLoadPhase::Queued,
           "stale job result must not replace active job state");

    expect(state.requestCancel(), "active job must accept cancellation request");
    expect(state.status().cancelRequested && state.status().cancellable,
           "cancel request must remain visible until worker acknowledges it");
    state.cancel(activeGeneration);
    expect(state.status().phase == PreviewLoadPhase::Cancelled,
           "worker acknowledgement must make cancellation terminal");
    expect(!state.requestCancel(), "terminal job must reject another cancel request");
}

void invalidTransitionsFailClosed() {
    using namespace stepcompare::viewer;
    PreviewLoadStateModel state;
    bool emptyRejected = false;
    try {
        static_cast<void>(state.begin(ModelSide::A, {}));
    } catch (const std::invalid_argument&) {
        emptyRejected = true;
    }
    expect(emptyRejected, "empty preview path must be rejected");

    const auto generation = state.begin(ModelSide::A, u8"part.step");
    state.updateProgress(generation, 101, "invalid");
    expect(state.status().percent == 0, "out-of-range progress must be ignored");
    state.updateProgress(generation, 20, "valid");
    state.updateProgress(generation, 10, "regression");
    expect(state.status().percent == 20, "progress must never regress");
}

}  // namespace

int main() {
    unicodeAndProgressContract();
    cancellationAndStaleGenerationContract();
    invalidTransitionsFailClosed();
    if (failures != 0) {
        std::cerr << failures << " preview state assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All headless preview load state tests passed\n";
    return EXIT_SUCCESS;
}
