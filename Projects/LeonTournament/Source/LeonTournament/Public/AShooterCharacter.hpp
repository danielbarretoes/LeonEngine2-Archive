#pragma once

#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "UShooterAnimInstance.hpp"
#include "UShooterCombatComponent.hpp"
#include "AShooterWeapon.hpp"
#include "FShooterTypes.hpp"
#include "Engine/FNetBlob.hpp"

namespace Leon {

    class AShooterPlayerState;

    class AShooterCharacter : public ACharacter {
    public:
        AShooterCharacter() = default;
        AShooterCharacter(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "ShooterCharacter");

        void PostInitializeComponents() override;
        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;
        void EndPlay() override;
        void SetupPlayerInputComponent(float DeltaSeconds) override;
        void UpdateAnimInstance(UAnimInstance& InAnim) const override;

        TRef<UHealthComponent> GetHealthComponent() const { return Health; }
        TRef<UShooterCombatComponent> GetCombatComponent() const { return Combat; }
        AShooterWeapon* GetWeapon() const { return Weapon; }
        AShooterPlayerState* GetShooterPlayerState() const;

        EShooterTeam GetTeam() const;
        bool IsSprinting() const { return bSprinting; }
        void SetSprinting(bool bValue) { bSprinting = bValue; }
        bool IsAiming() const { return bAiming; }
        bool IsBotControlled() const;
        void SetBotControlled(bool bInBot);

        void GetAimRay(glm::vec3& OutOrigin, glm::vec3& OutDirection) const;
        void ApplyDamageFrom(const FDamageInfo& InInfo);
        void PulseHitConfirm(bool bKill);
        void OnServerDeath(const FDamageInfo& InInfo);
        void OnServerRespawn(const glm::vec3& InLocation);

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
        void UpdateSprintState();
        void ApplyLookRotation();
        void FlushPendingNetInput(float DeltaSeconds);
        bool IsForwardPressed() const;

        TRef<UHealthComponent> Health;
        TRef<UShooterCombatComponent> Combat;
        TRef<UShooterAnimInstance> ShooterAnim;
        AShooterWeapon* Weapon = nullptr;
        uint8_t PendingNetBits = 0;
        bool bHasPendingNetInput = false;
        bool bSprinting = false;
        bool bAiming = true;
        bool bBot = false;
        bool bDeadFrozen = false;
        bool bReloadWasDown = false;
        bool bDeathBound = false;
        mutable uint8_t PendingHitConfirm = 0;
        mutable uint8_t PendingDamageFlash = 0;
    };

} // namespace Leon
