#pragma once

#include "ControllerAction.h"

#include <cstdint>
#include <optional>

namespace circuitpedal::circuitstomp {

namespace prototype1 {

constexpr std::uint8_t statusTypeMask = 0xF0U;
constexpr std::uint8_t channelMask = 0x0FU;
constexpr std::uint8_t controlChangeStatus = 0xB0U;
constexpr std::uint8_t channelOne = 0U;
constexpr std::uint8_t p1Controller = 20U;
constexpr std::uint8_t relativeZero = 0U;
constexpr std::uint8_t relativeAlternateZero = 64U;

} // namespace prototype1

std::optional<ControllerAction> decodeMidi1Message(std::uint8_t status,
                                                   std::uint8_t data1,
                                                   std::uint8_t data2) noexcept;

} // namespace circuitpedal::circuitstomp
