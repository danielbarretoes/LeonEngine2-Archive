#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Leon {

    struct FIncomingNetPacket {
        int32_t ConnectionId = -1;
        std::vector<uint8_t> Bytes;
        bool bReliable = false;
    };

    /**
     * Byte-oriented transport. Gameplay and UNetDriver never include ENet headers.
     */
    class INetTransport {
    public:
        virtual ~INetTransport() = default;

        virtual bool Listen(uint16_t InPort) = 0;
        virtual bool Connect(const std::string& InHost, uint16_t InPort) = 0;
        virtual void Close() = 0;
        virtual void Poll() = 0;

        virtual bool Send(int32_t InConnectionId, const uint8_t* InData, size_t InSize, bool bReliable) = 0;
        virtual bool SendToAll(const uint8_t* InData, size_t InSize, bool bReliable) = 0;

        virtual int32_t ConsumeAcceptedConnection() = 0;
        virtual std::vector<FIncomingNetPacket> TakeIncoming() = 0;
        virtual int32_t GetConnectionCount() const = 0;
        virtual bool IsServer() const = 0;
        virtual bool IsOpen() const = 0;

        uint32_t PacketsSent = 0;
        uint32_t PacketsReceived = 0;
        uint32_t BytesSent = 0;
        uint32_t BytesReceived = 0;
        float PingMs = 0.0f;

        /** Per-peer RTT when the transport tracks multiple connections (ENet server). */
        virtual float GetPeerPingMs(int32_t InConnectionId) const {
            (void)InConnectionId;
            return PingMs;
        }
    };

} // namespace Leon
