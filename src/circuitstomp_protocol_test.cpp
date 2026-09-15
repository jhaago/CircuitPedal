#include "CircuitStompProtocol.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <variant>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expectParameterDelta(const std::optional<circuitpedal::ControllerAction>& action,
                          std::size_t expectedIndex,
                          int expectedSteps,
                          const std::string& message)
{
    if (!action.has_value() || !std::holds_alternative<circuitpedal::ParameterDelta>(*action))
    {
        expect(false, message + " (not a parameter delta)");
        return;
    }
    const auto delta = std::get<circuitpedal::ParameterDelta>(*action);
    expect(delta.index == expectedIndex && delta.steps == expectedSteps, message);
}

void testP1RelativeMessages()
{
    using circuitpedal::circuitstomp::decodeMidi1Message;

    expectParameterDelta(decodeMidi1Message(0xB0, 20, 1),
                         0U, 1, "B0 14 01 decodes as P1 +1");
    expectParameterDelta(decodeMidi1Message(0xB0, 20, 127),
                         0U, -1, "B0 14 7F decodes as P1 -1");
    expectParameterDelta(decodeMidi1Message(0xB0, 20, 63),
                         0U, 63, "relative value 63 decodes as +63");
    expectParameterDelta(decodeMidi1Message(0xB0, 20, 65),
                         0U, -63, "relative value 65 decodes as -63");
}

void testUnsupportedMessagesAreIgnored()
{
    using circuitpedal::circuitstomp::decodeMidi1Message;

    expect(!decodeMidi1Message(0xB0, 20, 0).has_value(), "relative zero is a no-op");
    expect(!decodeMidi1Message(0xB0, 20, 64).has_value(), "relative 64 is a no-op");
    expect(!decodeMidi1Message(0xB1, 20, 1).has_value(), "other MIDI channel is ignored");
    expect(!decodeMidi1Message(0x90, 20, 1).has_value(), "note-on is ignored");
    expect(!decodeMidi1Message(0xB0, 21, 1).has_value(), "unassigned CC is ignored");
    expect(!decodeMidi1Message(0xB0, 20, 128).has_value(), "invalid value byte is ignored");
    expect(!decodeMidi1Message(0xB0, 128, 1).has_value(), "invalid controller byte is ignored");
}

} // namespace

int main()
{
    testP1RelativeMessages();
    testUnsupportedMessagesAreIgnored();

    if (failures != 0)
    {
        std::cerr << failures << " CircuitStomp protocol test(s) failed\n";
        return 1;
    }
    std::cout << "CircuitStomp protocol tests passed\n";
    return 0;
}
