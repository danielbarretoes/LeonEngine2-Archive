#pragma once

#include "Gameplay/AActor.hpp"
#include "FLeonTournamentTypes.hpp"

#include <unordered_map>

namespace Leon {

    class ALeonTournamentCharacter;

    class ALeonTournamentPickup : public AActor {
    public:
        ALeonTournamentPickup() = default;
        ALeonTournamentPickup(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "LeonTournamentPickup");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

        void SetRespawnDelay(float InSeconds) { RespawnDelay = InSeconds; }
        float GetRespawnDelay() const { return RespawnDelay; }
        float GetPickupRadius() const { return PickupRadius; }
        void SetPickupRadius(float InRadius) { PickupRadius = InRadius; }
        bool IsPickupActive() const { return bActive; }
        /** SpawnActor runs BeginPlay before callers can place the actor — set both transform + respawn anchor. */
        void SetAnchorLocation(const glm::vec3& InLocation);

    protected:
        virtual bool TryGiveTo(ALeonTournamentCharacter& InCharacter) = 0;
        virtual void BuildVisual() = 0;
        void SetPickupActive(bool bInActive);

        float RespawnDelay = 15.0f;
        float PickupRadius = 1.2f;
        float BobPhase = 0.0f;
        float RespawnRemaining = 0.0f;
        bool bActive = true;
        glm::vec3 HomeLocation{0.0f};
        glm::vec3 VisualColor{0.9f, 0.9f, 0.2f};
    };

    class ALeonTournamentWeaponPickup : public ALeonTournamentPickup {
    public:
        ALeonTournamentWeaponPickup() = default;
        ALeonTournamentWeaponPickup(entt::entity InHandle, UWorld* InWorld,
                                    const std::string& InName = "LeonTournamentWeaponPickup");

        void SetWeaponId(ELeonTournamentWeaponId InId);
        ELeonTournamentWeaponId GetWeaponId() const { return WeaponId; }

    protected:
        bool TryGiveTo(ALeonTournamentCharacter& InCharacter) override;
        void BuildVisual() override;

        ELeonTournamentWeaponId WeaponId = ELeonTournamentWeaponId::Shotgun;
    };

    class ALeonTournamentHealthPickup : public ALeonTournamentPickup {
    public:
        ALeonTournamentHealthPickup() = default;
        ALeonTournamentHealthPickup(entt::entity InHandle, UWorld* InWorld,
                                    const std::string& InName = "LeonTournamentHealthPickup");

    protected:
        bool TryGiveTo(ALeonTournamentCharacter& InCharacter) override;
        void BuildVisual() override;
    };

    /** Boost pad — launches pawns upward / along PadVelocity when walked over. */
    class ALeonTournamentJumpPad : public AActor {
    public:
        ALeonTournamentJumpPad() = default;
        ALeonTournamentJumpPad(entt::entity InHandle, UWorld* InWorld,
                               const std::string& InName = "LeonTournamentJumpPad");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

        void SetPadVelocity(const glm::vec3& InVelocity) { PadVelocity = InVelocity; }
        void SetTriggerRadius(float InRadius) { TriggerRadius = InRadius; }

    private:
        void BuildVisual();

        glm::vec3 PadVelocity{0.0f, 16.0f, 0.0f};
        float TriggerRadius = 1.4f;
        float Cooldown = 0.35f;
        std::unordered_map<ALeonTournamentCharacter*, float> RecentTriggers;
    };

} // namespace Leon
