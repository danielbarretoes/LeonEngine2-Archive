#pragma once

#include "Engine/UNetDriver.hpp"

namespace Leon {

    /**
     * @brief In-process paired drivers for tests (no sockets).
     */
    class ULoopbackNetDriver : public UNetDriver {
    public:
        static void Pair(ULoopbackNetDriver& InServer, ULoopbackNetDriver& InClient);
        void Tick(float InDeltaSeconds) override;

    private:
        ULoopbackNetDriver* Peer = nullptr;
    };

} // namespace Leon
