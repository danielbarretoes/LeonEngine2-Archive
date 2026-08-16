#pragma once

#include <cstdint>

namespace Leon {

    enum class ENetMode : uint8_t {
        Standalone = 0,
        ListenServer = 1,
        Client = 2
    };

    enum class ENetRole : uint8_t {
        None = 0,
        SimulatedProxy = 1,
        AutonomousProxy = 2,
        Authority = 3
    };

} // namespace Leon
