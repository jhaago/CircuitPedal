#pragma once

#include "ControllerAction.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace circuitpedal {

class MacMidiInput {
public:
    using ActionCallback = std::function<void(const ControllerAction&)>;
    using DiagnosticCallback = std::function<void(const std::string&)>;

    MacMidiInput();
    ~MacMidiInput();

    MacMidiInput(const MacMidiInput&) = delete;
    MacMidiInput& operator=(const MacMidiInput&) = delete;

    bool start(ActionCallback actionCallback,
               DiagnosticCallback diagnosticCallback,
               std::string& error);
    void stop() noexcept;
    bool running() const noexcept;
    std::size_t connectedSourceCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace circuitpedal
