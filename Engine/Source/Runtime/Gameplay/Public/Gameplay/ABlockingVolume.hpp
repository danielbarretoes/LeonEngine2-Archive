#pragma once

#include "Gameplay/AActor.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"

namespace Leon {

    /**
     * Invisible collision volume used to block movement or traces.
     */
    class ABlockingVolume : public AActor {
    public:
        ABlockingVolume() = default;
        ABlockingVolume(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "BlockingVolume");

        void PostInitializeComponents() override;
        void Tick(float DeltaSeconds) override;

        TRef<UBoxComponent> GetBoxComponent() const { return Box; }

    private:
        TRef<UBoxComponent> Box;
    };

} // namespace Leon
