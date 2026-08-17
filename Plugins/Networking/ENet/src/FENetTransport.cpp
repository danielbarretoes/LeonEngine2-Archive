#include "FENetTransport.hpp"

#include <enet/enet.h>

#include <cstring>

namespace Leon {

    namespace {
        bool EnsureENet() {
            static bool bInit = false;
            static bool bOk = false;
            if (!bInit) {
                bInit = true;
                bOk = enet_initialize() == 0;
            }
            return bOk;
        }

        constexpr uint8_t kChannelUnreliable = 0;
        constexpr uint8_t kChannelReliable = 1;
        constexpr size_t kChannelCount = 2;
    } // namespace

    FENetTransport::FENetTransport() = default;

    FENetTransport::~FENetTransport() { Close(); }

    bool FENetTransport::Listen(uint16_t InPort) {
        Close();
        if (!EnsureENet())
            return false;
        ENetAddress address{};
        address.host = ENET_HOST_ANY;
        address.port = InPort;
        Host = enet_host_create(&address, 32, kChannelCount, 0, 0);
        bListening = Host != nullptr;
        return bListening;
    }

    bool FENetTransport::Connect(const std::string& InHost, uint16_t InPort) {
        Close();
        if (!EnsureENet())
            return false;
        Host = enet_host_create(nullptr, 1, kChannelCount, 0, 0);
        if (!Host)
            return false;
        ENetAddress address{};
        if (enet_address_set_host(&address, InHost.c_str()) != 0) {
            Close();
            return false;
        }
        address.port = InPort;
        ENetPeer* peer = enet_host_connect(static_cast<ENetHost*>(Host), &address, kChannelCount, 0);
        if (!peer) {
            Close();
            return false;
        }
        FPeerSlot slot;
        slot.Peer = peer;
        slot.ConnectionId = NextConnectionId++;
        Peers.push_back(slot);
        bListening = false;
        return true;
    }

    void FENetTransport::Close() {
        if (Host) {
            enet_host_destroy(static_cast<ENetHost*>(Host));
            Host = nullptr;
        }
        Peers.clear();
        AcceptedQueue.clear();
        Incoming.clear();
        bListening = false;
    }

    void FENetTransport::Poll() {
        if (!Host)
            return;
        ENetEvent event{};
        while (enet_host_service(static_cast<ENetHost*>(Host), &event, 0) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                int32_t id = -1;
                for (auto& slot : Peers) {
                    if (slot.Peer == event.peer) {
                        id = slot.ConnectionId;
                        break;
                    }
                }
                if (id < 0) {
                    FPeerSlot slot;
                    slot.Peer = event.peer;
                    slot.ConnectionId = NextConnectionId++;
                    Peers.push_back(slot);
                    id = slot.ConnectionId;
                    AcceptedQueue.push_back(id);
                }
                event.peer->data = reinterpret_cast<void*>(static_cast<intptr_t>(id));
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                int32_t id = static_cast<int32_t>(reinterpret_cast<intptr_t>(event.peer->data));
                FIncomingNetPacket packet;
                packet.ConnectionId = id;
                packet.bReliable = event.channelID == kChannelReliable;
                packet.Bytes.assign(event.packet->data, event.packet->data + event.packet->dataLength);
                Incoming.push_back(std::move(packet));
                ++PacketsReceived;
                BytesReceived += static_cast<uint32_t>(event.packet->dataLength);
                enet_packet_destroy(event.packet);
                PingMs = static_cast<float>(event.peer->roundTripTime);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                for (auto it = Peers.begin(); it != Peers.end(); ++it) {
                    if (it->Peer == event.peer) {
                        Peers.erase(it);
                        break;
                    }
                }
                break;
            }
            default:
                break;
            }
        }
    }

    bool FENetTransport::Send(int32_t InConnectionId, const uint8_t* InData, size_t InSize, bool bReliable) {
        if (!Host || !InData || InSize == 0)
            return false;
        for (auto& slot : Peers) {
            if (slot.ConnectionId != InConnectionId)
                continue;
            auto* peer = static_cast<ENetPeer*>(slot.Peer);
            if (!peer)
                return false;
            ENetPacket* packet =
                enet_packet_create(InData, InSize, bReliable ? ENET_PACKET_FLAG_RELIABLE : 0);
            if (!packet)
                return false;
            if (enet_peer_send(peer, bReliable ? kChannelReliable : kChannelUnreliable, packet) != 0)
                return false;
            ++PacketsSent;
            BytesSent += static_cast<uint32_t>(InSize);
            return true;
        }
        return false;
    }

    bool FENetTransport::SendToAll(const uint8_t* InData, size_t InSize, bool bReliable) {
        bool bAny = false;
        for (auto& slot : Peers)
            bAny = Send(slot.ConnectionId, InData, InSize, bReliable) || bAny;
        return bAny;
    }

    int32_t FENetTransport::ConsumeAcceptedConnection() {
        if (AcceptedQueue.empty())
            return -1;
        int32_t id = AcceptedQueue.front();
        AcceptedQueue.erase(AcceptedQueue.begin());
        return id;
    }

    std::vector<FIncomingNetPacket> FENetTransport::TakeIncoming() {
        std::vector<FIncomingNetPacket> out;
        out.swap(Incoming);
        return out;
    }

    int32_t FENetTransport::GetConnectionCount() const { return static_cast<int32_t>(Peers.size()); }

} // namespace Leon
