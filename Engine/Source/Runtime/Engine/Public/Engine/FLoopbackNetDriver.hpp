#pragma once

#include "Engine/UNetDriver.hpp"

namespace Leon {

    /**
     * @brief In-process paired drivers for tests (no sockets).
     */
    class FLoopbackNetDriver : public UNetDriver {
    public:
        static void Pair(FLoopbackNetDriver& InServer, FLoopbackNetDriver& InClient);
        void Tick(float InDeltaSeconds) override;

    private:
        FLoopbackNetDriver* Peer = nullptr;
    };

} // namespace Leon
