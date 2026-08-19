#pragma once

#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "Gameplay/UInventoryComponent.hpp"
#include "Gameplay/UFootstepComponent.hpp"
#include "ULeonTournamentAnimInstance.hpp"
#include "ULeonTournamentCombatComponent.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "FLeonTournamentTypes.hpp"
#include "Engine/FNetBlob.hpp"

#include <vector>

namespace Leon {

    class ALeonTournamentPlayerState;
    class USkeletalMesh;

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
        TRef<UInventoryComponent> GetInventoryComponent() const { return Inventory; }
        TRef<UFootstepComponent> GetFootstepComponent() const { return Footsteps; }
        TRef<ULeonTournamentCombatComponent> GetCombatComponent() const { return Combat; }
        ALeonTournamentWeapon* GetWeapon() const { return Weapon; }
        ALeonTournamentPlayerState* GetPlayerState() const;

        ELeonTournamentTeam GetTeam() const;
        bool IsBotControlled() const;
        void SetBotControlled(bool bInBot);
        bool IsDeadFrozen() const { return bDeadFrozen; }

        void GetAimRay(glm::vec3& OutOrigin, glm::vec3& OutDirection) const;
        /** Grip / muzzle for VFX. Hitscan uses GetAimRay (camera), not this. */
        glm::vec3 GetMuzzleSocketLocation() const;
        /** Impulse used by rocket splash / jump pads (sets Falling when upward). */
        void ApplyLaunchVelocity(const glm::vec3& InVelocity);
        void TryDodge();
        void ApplyDamageFrom(const FDamageInfo& InInfo);
        bool IsAimingDownSights() const { return bAimingDownSights; }
        void PulseHitConfirm(bool bKill);
        void OnServerDeath(const FDamageInfo& InInfo);
        void OnServerRespawn(const glm::vec3& InLocation);

        virtual bool ShouldSpawnWeapon() const;

        /** Grant default rifle if missing and select it. Safe to call before or after BeginPlay. */
        void EnsureWeapon();

        /** UT inventory: grant weapon (or refill mag) and optionally auto-switch. Returns true if pickup consumed. */
        bool GiveWeapon(ELeonTournamentWeaponId InId, bool bAutoSwitch = true);
        bool HasWeapon(ELeonTournamentWeaponId InId) const;
        bool SelectWeapon(ELeonTournamentWeaponId InId);
        void CycleWeapon(int InDirection);
        ELeonTournamentWeaponId GetActiveWeaponId() const;
        ALeonTournamentWeapon* GetInventoryWeapon(ELeonTournamentWeaponId InId) const;

        void BotMoveToward(const glm::vec3& InWorldTarget, float DeltaSeconds, float InSpeedScale = 1.0f);
        void BotLookAt(const glm::vec3& InWorldPoint);
        void BotSetFireHeld(bool bHeld);
        void BotRequestReload();

        void ApplyCharacterSkin(ELeonTournamentCharacterSkin InSkin);
        ELeonTournamentCharacterSkin GetCharacterSkin() const { return CharacterSkin; }

        void SetMenuShowcase(bool bEnabled);
        bool IsMenuShowcase() const { return bMenuShowcase; }

        /** Lowest bind-pose Y of foot-weighted vertices (soles), else foot bones, else AABB. */
        static float BindPoseFeetY(const USkeletalMesh& InMesh);
        /** Bind-pose feet Y after skinning with InPalette (tests / one-shot); do not call per tick. */
        static float SkinnedFeetY(const USkeletalMesh& InMesh, const std::vector<glm::mat4>& InPalette);

        void SerializeReplication(std::vector<uint8_t>& OutBytes) const override;
        void DeserializeReplication(const uint8_t* InData, size_t InSize) override;
        void SerializeControlInput(std::vector<uint8_t>& OutBytes) const override;
        void ApplyControlInput(const uint8_t* InData, size_t InSize) override;
        bool HandleServerRPC(uint16_t InFunctionId, const uint8_t* InData, size_t InSize) override;

    protected:
        bool ShouldApplyControlYawToActor() const override { return !bDeadFrozen; }
        bool CanApplyControlMove() const override {
            return !bDeadFrozen && !bMenuShowcase && ACharacter::CanApplyControlMove();
        }

    private:
        void ClearInventoryKeepRifle();
        ALeonTournamentWeapon* SpawnWeaponActor(ELeonTournamentWeaponId InId);
        void ApplyLookRotation();
        void UpdatePresentationVisibility();
        void UpdateTeamOutline();
        void HandleWeaponSwitchInput();
        void HandleCameraToggleInput();
        void UpdateAimDownSights(float DeltaSeconds);
        void PlantMeshFeetOnCapsule();

        TRef<UHealthComponent> Health;
        TRef<UInventoryComponent> Inventory;
        TRef<ULeonTournamentCombatComponent> Combat;
        TRef<ULeonTournamentAnimInstance> AnimInst;
        TRef<UFootstepComponent> Footsteps;
        ALeonTournamentWeapon* Weapon = nullptr;
        bool bBot = false;
        bool bMenuShowcase = false;
        bool bDeadFrozen = false;
        bool bDeathForcedThirdPerson = false;
        bool bDeathCamArmOverride = false;
        float DeathCamArmLengthRestore = 3.4f;
        bool bReloadWasDown = false;
        bool bDeathBound = false;
        bool bKey1WasDown = false;
        bool bKey2WasDown = false;
        bool bKey3WasDown = false;
        bool bKey4WasDown = false;
        bool bKey5WasDown = false;
        bool bKey6WasDown = false;
        bool bKeyQWasDown = false;
        bool bKeyEWasDown = false;
        bool bPadDLeftWasDown = false;
        bool bPadDRightWasDown = false;
        bool bDodgeWasDown = false;
        bool bCameraToggleWasDown = false;
        bool bAimingDownSights = false;
        float DodgeCooldownRemaining = 0.0f;
        ELeonTournamentCharacterSkin CharacterSkin = ELeonTournamentCharacterSkin::YBot;
        /** Bind-pose sole Y in mesh space; filled on skin apply (vertex scan is too heavy per tick). */
        float CachedBindPoseFeetY = 0.0f;
        bool bCachedBindPoseFeetY = false;
        mutable uint8_t PendingHitConfirm = 0;
        mutable uint8_t PendingDamageFlash = 0;
    };

} // namespace Leon
