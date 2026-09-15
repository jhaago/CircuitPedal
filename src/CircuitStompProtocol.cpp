#include "CircuitStompProtocol.h"

namespace circuitpedal::circuitstomp {

std::optional<ControllerAction> decodeMidi1Message(std::uint8_t status,
                                                   std::uint8_t data1,
                                                   std::uint8_t data2) noexcept
{
    if ((status & prototype1::statusTypeMask) != prototype1::controlChangeStatus
        || (status & prototype1::channelMask) != prototype1::channelOne
        || data1 > 127U
        || data2 > 127U
        || data1 != prototype1::p1Controller
        || data2 == prototype1::relativeZero
        || data2 == prototype1::relativeAlternateZero)
    {
        return std::nullopt;
    }

    const int steps = data2 < prototype1::relativeAlternateZero
        ? static_cast<int>(data2)
        : static_cast<int>(data2) - 128;
    return ControllerAction { ParameterDelta { 0U, steps } };
}

} // namespace circuitpedal::circuitstomp
