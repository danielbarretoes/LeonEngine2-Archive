#pragma once

#include "Core/Base.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Engine/ENetTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    class UWorld;
    class UNetDriver;

    /** Max payload bytes per RPC entry (reject larger before queueing). */
    constexpr size_t kMaxNetRPCPayloadBytes = 1024;

    /**
     * @brief One peer on a UNetDriver. Incoming bytes are filled by the driver before Tick.
     */
    class UNetConnection {
    public:
        std::vector<uint8_t> Incoming;
        std::vector<uint8_t> Outgoing;
        std::vector<uint8_t> IncomingInput;
        std::vector<uint8_t> OutgoingInput;
        /** Framed RPC batch (magic RPC1 + entries). Separate from snapshots / control input. */
        std::vector<uint8_t> IncomingRPC;
        std::vector<uint8_t> OutgoingRPC;
        /** PlayerId of the remote pawn this connection drives. -1 = unbound. */
        int32_t BoundPlayerId = -1;
        /** Last measured RTT for this peer (ms). Updated by UIpNetDriver on receive. */
        float PingMs = 0.0f;
    };

    /**
     * @brief Minimal listen-server snapshot driver (+ framed Server/Client/Multicast RPCs).
     * Replicates GameState, PlayerState, pawns, and other bReplicates actors (relevancy lite).
     * NetGUID = Actor GUID. AutonomousProxy keeps local movement; server pose is corrected softly.
     */
    class UNetDriver {
    public:
        virtual ~UNetDriver() = default;

        void SetWorld(UWorld* InWorld) { World = InWorld; }
        UWorld* GetWorld() const { return World; }

        /** RTT (ms) for the connection bound to InPlayerId, or 0 if unknown (host / offline). */
        float GetPingMsForPlayer(int32_t InPlayerId) const;

        virtual void Tick(float InDeltaSeconds);
        virtual void ConsumeIncomingInput();
        virtual void ConsumeIncomingRPCs();

        /** Append one RPC entry to a connection OutgoingRPC batch (creates magic header if empty). */
        static bool AppendOutgoingRPC(UNetConnection& InConn, const FUUID& InActorGuid, ENetRPCKind InKind,
                                      uint16_t InFunctionId, const std::vector<uint8_t>& InPayload);

        /** True if buffer starts with the RPC1 batch magic. */
        static bool IsRPCBatch(const std::vector<uint8_t>& InBytes);

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
