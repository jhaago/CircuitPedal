#pragma once

#include "ControllerAction.h"
#include "ControllerTarget.h"

namespace circuitpedal {

struct ControllerRouteResult {
    bool handled = false;
    bool stateChanged = false;
};

class ControllerRouter {
public:
    explicit ControllerRouter(ControllerTarget& target) noexcept;

    ControllerRouteResult route(const ControllerAction& action) noexcept;
    ControllerHostState hostState() const;

private:
    ControllerTarget& target_;
};

} // namespace circuitpedal
