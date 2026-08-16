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
    };

    /**
     * @brief Minimal listen-server snapshot driver (no RPCs / relevancy / prediction).
     * Replicates GameState elapsed time, PlayerState name/id/score, and possessed pawn transforms.
     * NetGUID = Actor GUID.
     */
    class UNetDriver {
    public:
        virtual ~UNetDriver() = default;

        void SetWorld(UWorld* InWorld) { World = InWorld; }
        UWorld* GetWorld() const { return World; }

        virtual void Tick(float InDeltaSeconds);

        const std::vector<TRef<UNetConnection>>& GetConnections() const { return Connections; }
        UNetConnection* AddConnection();

    protected:
        void BuildSnapshot(std::vector<uint8_t>& OutBytes) const;
        void ApplySnapshot(const std::vector<uint8_t>& InBytes);

        UWorld* World = nullptr;
        std::vector<TRef<UNetConnection>> Connections;
    };

} // namespace Leon
