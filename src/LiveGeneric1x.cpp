#include "GenericCircuitProcessor.h"

namespace circuitpedal {
namespace {

// The physically validated macOS live path is currently 1x. Four-times
// processing remains available to non-live validation/engineering code, but is
// not selected for realtime playing until its performance regression is solved.
struct SelectLiveGenericOneX {
    SelectLiveGenericOneX() noexcept
    {
        setGenericProcessingMode(GenericProcessingMode::OneX);
    }
};

const SelectLiveGenericOneX selectLiveGenericOneX;

} // namespace
} // namespace circuitpedal
