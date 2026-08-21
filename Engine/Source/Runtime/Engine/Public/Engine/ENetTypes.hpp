#pragma once

#include <cstdint>

namespace Leon {

    enum class ENetMode : uint8_t {
        Standalone = 0,
        ListenServer = 1,
        Client = 2,
        /** Authority without a local player (joins only). */
        DedicatedServer = 3
    };

    enum class ENetRole : uint8_t { None = 0, SimulatedProxy = 1, AutonomousProxy = 2, Authority = 3 };

    /** Direction of a framed RPC batch entry (see UNetDriver IncomingRPC / OutgoingRPC). */
    enum class ENetRPCKind : uint8_t { Server = 0, Client = 1, Multicast = 2 };

} // namespace Leon
