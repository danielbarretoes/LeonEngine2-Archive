#pragma once

#include "Core/Base.hpp"
#include "Assets/FAssetTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    class UWorld;
    class UNetDriver;

    /**
     * @brief One peer on a UNetDriver. Incoming bytes are filled by the driver before Tick.
     */
    class UNetConnection {
    public:
        std::vector<uint8_t> Incoming;
        std::vector<uint8_t> Outgoing;
        std::vector<uint8_t> IncomingInput;
        std::vector<uint8_t> OutgoingInput;
        /** PlayerId of the remote pawn this connection drives. -1 = unbound. */
        int32_t BoundPlayerId = -1;
    };

    /**
     * @brief Minimal listen-server snapshot driver (no RPCs / relevancy / prediction).
     * Replicates GameState, PlayerState, pawn transforms, and optional subclass blobs.
     * NetGUID = Actor GUID.
     */
    class UNetDriver {
    public:
        virtual ~UNetDriver() = default;

        void SetWorld(UWorld* InWorld) { World = InWorld; }
        UWorld* GetWorld() const { return World; }

        virtual void Tick(float InDeltaSeconds);
        virtual void ConsumeIncomingInput();

        const std::vector<TRef<UNetConnection>>& GetConnections() const { return Connections; }
        UNetConnection* AddConnection();

        int32_t GetLocalPlayerId() const { return LocalPlayerId; }
        void SetLocalPlayerId(int32_t InId) { LocalPlayerId = InId; }

    protected:
        void BuildSnapshot(std::vector<uint8_t>& OutBytes) const;
        void ApplySnapshot(const std::vector<uint8_t>& InBytes);

        UWorld* World = nullptr;
        std::vector<TRef<UNetConnection>> Connections;
        int32_t LocalPlayerId = -1;
    };

} // namespace Leon
