#pragma once

#include "Engine/UNetDriver.hpp"
#include "Engine/INetTransport.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Leon {

    /**
     * IP listen/join driver. Transport is injected (ENet). Same snapshot protocol as UNetDriver.
     */
    class UIpNetDriver : public UNetDriver {
    public:
        static constexpr uint16_t DefaultPort = 7777;

        UIpNetDriver();
        ~UIpNetDriver() override;

        void SetTransport(std::unique_ptr<INetTransport> InTransport);
        INetTransport* GetTransport() const { return Transport.get(); }

        bool Listen(uint16_t InPort = DefaultPort);
        bool Connect(const std::string& InHost, uint16_t InPort = DefaultPort);
        void Close();
        bool IsOpen() const;

        void Tick(float InDeltaSeconds) override;

    private:
        void AcceptJoin(int32_t InConnectionId);
        UNetConnection* ConnectionForId(int32_t InConnectionId);

        std::unique_ptr<INetTransport> Transport;
        std::unordered_map<int32_t, size_t> ConnectionIndexById;
        uint16_t Port = DefaultPort;
        bool bListening = false;
    };

} // namespace Leon
