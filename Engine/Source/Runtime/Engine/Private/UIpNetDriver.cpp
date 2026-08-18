#include "Engine/UIpNetDriver.hpp"
#include "Engine/FNetBlob.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Core/FFrameProfiler.hpp"

#include <cstring>

namespace Leon {

    UIpNetDriver::FCreateNetTransport UIpNetDriver::TransportFactory;

    void UIpNetDriver::SetTransportFactory(FCreateNetTransport InFactory) { TransportFactory = std::move(InFactory); }

    std::unique_ptr<INetTransport> UIpNetDriver::CreateTransport() {
        if (!TransportFactory)
            return nullptr;
        return TransportFactory();
    }

    UIpNetDriver::UIpNetDriver() = default;

    UIpNetDriver::~UIpNetDriver() { Close(); }

    void UIpNetDriver::SetTransport(std::unique_ptr<INetTransport> InTransport) {
        Close();
        Transport = std::move(InTransport);
    }

    bool UIpNetDriver::IsOpen() const { return Transport && Transport->IsOpen(); }

    bool UIpNetDriver::Listen(uint16_t InPort) {
        if (!Transport)
            return false;
        Close();
        Port = InPort;
        if (!Transport->Listen(InPort))
            return false;
        bListening = true;
        return true;
    }

    bool UIpNetDriver::Connect(const std::string& InHost, uint16_t InPort) {
        if (!Transport)
            return false;
        Close();
        Port = InPort;
        bListening = false;
        if (!Transport->Connect(InHost, InPort))
            return false;
        if (GetConnections().empty())
            AddConnection();
        ConnectionIndexById[1] = 0;
        return true;
    }

    void UIpNetDriver::Close() {
        if (Transport)
            Transport->Close();
        ConnectionIndexById.clear();
        Connections.clear();
        LocalPlayerId = -1;
        bListening = false;
    }

    UNetConnection* UIpNetDriver::ConnectionForId(int32_t InConnectionId) {
        auto it = ConnectionIndexById.find(InConnectionId);
        if (it == ConnectionIndexById.end() || it->second >= Connections.size())
            return nullptr;
        return Connections[it->second].get();
    }

    void UIpNetDriver::AcceptJoin(int32_t InConnectionId) {
        if (!World)
            return;
        AGameModeBase* gm = World->GetGameMode();
        APlayerController* pc = gm ? gm->Login("LAN_Player") : nullptr;
        UNetConnection* conn = AddConnection();
        if (conn && pc && pc->GetPlayerState())
            conn->BoundPlayerId = pc->GetPlayerState()->GetPlayerId();
        ConnectionIndexById[InConnectionId] = Connections.size() - 1;

        std::vector<uint8_t> welcome;
        const char magic[] = "LEONWELC";
        welcome.insert(welcome.end(), magic, magic + 8);
        const int32_t playerId = conn ? conn->BoundPlayerId : -1;
        FNetBlob::WriteI32(welcome, playerId);
        if (Transport)
            Transport->Send(InConnectionId, welcome.data(), welcome.size(), true);
    }

    void UIpNetDriver::Tick(float InDeltaSeconds) {
        if (!Transport)
            return;

        Transport->Poll();

        for (;;) {
            int32_t id = Transport->ConsumeAcceptedConnection();
            if (id < 0)
                break;
            if (bListening)
                AcceptJoin(id);
        }

        auto packets = Transport->TakeIncoming();
        for (auto& packet : packets) {
            if (!bListening && packet.Bytes.size() >= 12 &&
                std::memcmp(packet.Bytes.data(), "LEONWELC", 8) == 0) {
                size_t offset = 8;
                int32_t id = -1;
                if (FNetBlob::ReadI32(packet.Bytes, offset, id))
                    SetLocalPlayerId(id);
                continue;
            }
            UNetConnection* conn = ConnectionForId(packet.ConnectionId);
            if (!conn && !bListening && !Connections.empty())
                conn = Connections.front().get();
            if (!conn)
                continue;
            if (UNetDriver::IsRPCBatch(packet.Bytes))
                conn->IncomingRPC = std::move(packet.Bytes);
            else if (bListening)
                conn->IncomingInput = std::move(packet.Bytes);
            else
                conn->Incoming = std::move(packet.Bytes);
        }

        UNetDriver::Tick(InDeltaSeconds);

        if (bListening) {
            for (auto& conn : Connections) {
                if (conn && !conn->Outgoing.empty() && Transport) {
                    Transport->SendToAll(conn->Outgoing.data(), conn->Outgoing.size(), false);
                    conn->Outgoing.clear();
                    break;
                }
            }
            for (auto& conn : Connections) {
                if (conn)
                    conn->Outgoing.clear();
            }
            for (auto& conn : Connections) {
                if (conn && !conn->OutgoingRPC.empty() && Transport) {
                    Transport->SendToAll(conn->OutgoingRPC.data(), conn->OutgoingRPC.size(), true);
                    conn->OutgoingRPC.clear();
                }
            }
        } else if (!Connections.empty() && Connections.front()) {
            auto& front = Connections.front();
            if (!front->OutgoingInput.empty()) {
                Transport->SendToAll(front->OutgoingInput.data(), front->OutgoingInput.size(), false);
                front->OutgoingInput.clear();
            }
            if (!front->OutgoingRPC.empty()) {
                Transport->SendToAll(front->OutgoingRPC.data(), front->OutgoingRPC.size(), true);
                front->OutgoingRPC.clear();
            }
        }

        auto& timing = FFrameProfiler::Working();
        timing.PacketsSent = Transport->PacketsSent;
        timing.PacketsReceived = Transport->PacketsReceived;
        timing.PingMs = Transport->PingMs;
        timing.BytesPerSec = Transport->BytesSent + Transport->BytesReceived;
    }

} // namespace Leon
