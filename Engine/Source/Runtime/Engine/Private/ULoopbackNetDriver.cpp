#include "Engine/ULoopbackNetDriver.hpp"

namespace Leon {

    void ULoopbackNetDriver::Pair(ULoopbackNetDriver& InServer, ULoopbackNetDriver& InClient) {
        InServer.Peer = &InClient;
        InClient.Peer = &InServer;
        if (InServer.GetConnections().empty())
            InServer.AddConnection();
        if (InClient.GetConnections().empty())
            InClient.AddConnection();
    }

    void ULoopbackNetDriver::Tick(float InDeltaSeconds) {
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
        if (!local->OutgoingInput.empty()) {
            remote->IncomingInput = local->OutgoingInput;
            local->OutgoingInput.clear();
        }
    }

} // namespace Leon
