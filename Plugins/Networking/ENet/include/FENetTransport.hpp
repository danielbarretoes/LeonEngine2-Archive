#pragma once

#include "Engine/INetTransport.hpp"

namespace Leon {

    /**
     * ENet host/peer transport. Implementation lives in Plugins/Networking/ENet.
     */
    class FENetTransport : public INetTransport {
    public:
        FENetTransport();
        ~FENetTransport() override;

        bool Listen(uint16_t InPort) override;
        bool Connect(const std::string& InHost, uint16_t InPort) override;
        void Close() override;
        void Poll() override;

        bool Send(int32_t InConnectionId, const uint8_t* InData, size_t InSize, bool bReliable) override;
        bool SendToAll(const uint8_t* InData, size_t InSize, bool bReliable) override;

        int32_t ConsumeAcceptedConnection() override;
        std::vector<FIncomingNetPacket> TakeIncoming() override;
        int32_t GetConnectionCount() const override;
        bool IsServer() const override { return bListening; }
        bool IsOpen() const override { return Host != nullptr; }

    private:
        struct FPeerSlot {
            void* Peer = nullptr;
            int32_t ConnectionId = -1;
        };

        void* Host = nullptr;
        std::vector<FPeerSlot> Peers;
        std::vector<int32_t> AcceptedQueue;
        std::vector<FIncomingNetPacket> Incoming;
        bool bListening = false;
        int32_t NextConnectionId = 1;
    };

} // namespace Leon
