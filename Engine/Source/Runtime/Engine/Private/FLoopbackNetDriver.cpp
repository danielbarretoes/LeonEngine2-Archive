#include "Engine/FLoopbackNetDriver.hpp"

namespace Leon {

    void FLoopbackNetDriver::Pair(FLoopbackNetDriver& InServer, FLoopbackNetDriver& InClient) {
        InServer.Peer = &InClient;
        InClient.Peer = &InServer;
        if (InServer.GetConnections().empty())
            InServer.AddConnection();
        if (InClient.GetConnections().empty())
            InClient.AddConnection();
    }

    void FLoopbackNetDriver::Tick(float InDeltaSeconds) {
        UNetDriver::Tick(InDeltaSeconds);
        if (!Peer || Connections.empty() || Peer->Connections.empty())
            return;

        auto& local = Connections.front();
        auto& remote = Peer->Connections.front();
        if (!local || !remote)
            return;

        if (!local->Outgoing.empty()) {
            remote->Incoming = local->Outgoing;
            local->Outgoing.clear();
        }
    }

} // namespace Leon
