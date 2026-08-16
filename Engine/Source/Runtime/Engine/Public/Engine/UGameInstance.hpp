#pragma once

#include "Gameplay/UObject.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/ENetTypes.hpp"

namespace Leon {

    /**
     * @brief Unreal Engine aligned GameInstance surviving across map loads.
     */
    class UGameInstance : public UObject {
    public:
        UGameInstance() = default;
        explicit UGameInstance(const std::string& InName) : UObject(InName) {}
        ~UGameInstance() override = default;

        virtual void Init() {}
        virtual void Shutdown() {}

        void SetWorld(const TRef<UWorld>& InWorld) { World = InWorld; }
        TRef<UWorld> GetWorld() const { return World; }

        ENetMode GetNetMode() const { return NetMode; }
        void SetNetMode(ENetMode InMode) { NetMode = InMode; }

        const std::string& GetTravelURL() const { return TravelURL; }
        void SetTravelURL(const std::string& InURL) { TravelURL = InURL; }

    protected:
        TRef<UWorld> World;
        ENetMode NetMode = ENetMode::Standalone;
        std::string TravelURL;
    };

} // namespace Leon
