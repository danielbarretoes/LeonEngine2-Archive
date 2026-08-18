#pragma once

#include "Gameplay/AActor.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"

#include <glm/glm.hpp>

namespace Leon {

    class APawn;

    /**
     * Respawnable collectible with bob/spin and overlap (or radius) give.
     * Product subclasses implement GiveTo / BuildVisual.
     */
    class APickup : public AActor {
    public:
        APickup() = default;
        APickup(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "Pickup");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

        void SetRespawnDelay(float InSeconds) { RespawnDelay = InSeconds; }
        float GetRespawnDelay() const { return RespawnDelay; }
        void SetPickupRadius(float InRadius);
        float GetPickupRadius() const { return PickupRadius; }
        bool IsPickupActive() const { return bActive; }
        void SetAnchorLocation(const glm::vec3& InLocation);
        virtual void SetPickupActive(bool bInActive);

        void SetBobEnabled(bool bEnabled) { bBobEnabled = bEnabled; }
        void SetBobAmplitude(float InAmplitude) { BobAmplitude = InAmplitude; }
        void SetBobSpeed(float InSpeed) { BobSpeed = InSpeed; }
        void SetSpinDegreesPerSecond(float InDegrees) { SpinDegreesPerSecond = InDegrees; }
        float GetSpinDegreesPerSecond() const { return SpinDegreesPerSecond; }

    protected:
        virtual bool CanBePickedUp(APawn* InPawn) const;
        virtual bool GiveTo(APawn* InPawn) = 0;
        virtual void BuildVisual() {}

        void TryCollectOverlappingPawns();
        void OnOverlapPawn(APawn* InPawn);

        float RespawnDelay = 15.0f;
        float PickupRadius = 1.2f;
        float BobPhase = 0.0f;
        float BobAmplitude = 0.12f;
        float BobSpeed = 2.4f;
        float SpinYaw = 0.0f;
        float SpinDegreesPerSecond = 120.0f;
        float SpinTiltDegrees = 22.0f;
        float RespawnRemaining = 0.0f;
        bool bActive = true;
        bool bBobEnabled = true;
        glm::vec3 HomeLocation{0.0f};
        TRef<USphereComponent> OverlapSphere;
    };

} // namespace Leon
