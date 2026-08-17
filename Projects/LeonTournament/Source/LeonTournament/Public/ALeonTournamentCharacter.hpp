#pragma once

#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "ULeonTournamentAnimInstance.hpp"
#include "ULeonTournamentCombatComponent.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "FLeonTournamentTypes.hpp"
#include "Engine/FNetBlob.hpp"

namespace Leon {

    class ALeonTournamentPlayerState;

    class ALeonTournamentCharacter : public ACharacter {
    public:
        ALeonTournamentCharacter() = default;
        ALeonTournamentCharacter(entt::entity InHandle, UWorld* InWorld,
                                 const std::string& InName = "LeonTournamentCharacter");

        void PostInitializeComponents() override;
        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;
        void EndPlay() override;
        void SetupPlayerInputComponent(float DeltaSeconds) override;
        void UpdateAnimInstance(UAnimInstance& InAnim) const override;

        TRef<UHealthComponent> GetHealthComponent() const { return Health; }
        TRef<ULeonTournamentCombatComponent> GetCombatComponent() const { return Combat; }
        ALeonTournamentWeapon* GetWeapon() const { return Weapon; }
        ALeonTournamentPlayerState* GetPlayerState() const;

        ELeonTournamentTeam GetTeam() const;
        bool IsBotControlled() const;
        void SetBotControlled(bool bInBot);
        bool IsDeadFrozen() const { return bDeadFrozen; }

        void GetAimRay(glm::vec3& OutOrigin, glm::vec3& OutDirection) const;
        void ApplyDamageFrom(const FDamageInfo& InInfo);
        void PulseHitConfirm(bool bKill);
        void OnServerDeath(const FDamageInfo& InInfo);
        void OnServerRespawn(const glm::vec3& InLocation);

        virtual bool ShouldSpawnWeapon() const { return true; }

        void BotMoveToward(const glm::vec3& InWorldTarget, float DeltaSeconds, float InSpeedScale = 1.0f);
        void BotLookAt(const glm::vec3& InWorldPoint);
        void BotSetFireHeld(bool bHeld);
        void BotRequestReload();

        void SerializeReplication(std::vector<uint8_t>& OutBytes) const override;
        void DeserializeReplication(const uint8_t* InData, size_t InSize) override;
        void SerializeControlInput(std::vector<uint8_t>& OutBytes) const override;
        void ApplyControlInput(const uint8_t* InData, size_t InSize) override;

    private:
        void EnsureWeapon();
        void ApplyLookRotation();
        void FlushPendingNetInput(float DeltaSeconds);
        void UpdatePresentationVisibility();
        void BeginDeathRagdoll();
        void StopDeathRagdoll();

        TRef<UHealthComponent> Health;
        TRef<ULeonTournamentCombatComponent> Combat;
        TRef<ULeonTournamentAnimInstance> AnimInst;
        ALeonTournamentWeapon* Weapon = nullptr;
        uint8_t PendingNetBits = 0;
        bool bHasPendingNetInput = false;
        bool bBot = false;
        bool bDeadFrozen = false;
        bool bReloadWasDown = false;
        bool bDeathBound = false;
        mutable uint8_t PendingHitConfirm = 0;
        mutable uint8_t PendingDamageFlash = 0;
    };

} // namespace Leon
