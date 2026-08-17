#include "AShooterCharacter.hpp"
#include "AShooterPlayerState.hpp"
#include "AShooterGameMode.hpp"
#include "AShooterPlayerController.hpp"
#include "Core/FInput.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    AShooterCharacter::AShooterCharacter(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : ACharacter(InHandle, InWorld, InName) {
        SetClass("AShooterCharacter");
    }

    void AShooterCharacter::PostInitializeComponents() {
        ACharacter::PostInitializeComponents();
        SetThirdPerson(true);

        if (!Health)
            Health = AddActorComponent<UHealthComponent>("Health");
        if (!Combat)
            Combat = AddActorComponent<UShooterCombatComponent>("Combat");

        if (GetMesh()) {
            ShooterAnim = MakeRef<UShooterAnimInstance>("ShooterAnim");
            GetMesh()->SetAnimInstance(ShooterAnim);
            GetMesh()->SetMeshAssetPath("/Game/SkeletalMeshes/YBot.lskeletalmesh");
        }
    }

    void AShooterCharacter::BeginPlay() {
        ACharacter::BeginPlay();
        EnsureWeapon();
        if (Health && !bDeathBound) {
            Health->OnDeath.push_back([this](const FDamageInfo& info) { OnServerDeath(info); });
            bDeathBound = true;
        }
    }

    void AShooterCharacter::EndPlay() {
        if (Weapon && World) {
            Weapon->SetOwnerCharacter(nullptr);
            World->DestroyActor(Weapon);
            Weapon = nullptr;
        }
        ACharacter::EndPlay();
    }

    void AShooterCharacter::EnsureWeapon() {
        if (Weapon || !World)
            return;
        Weapon = World->SpawnActor<AShooterRifle>("Rifle");
        if (!Weapon)
            return;
        Weapon->SetOwnerCharacter(this);
        Weapon->AttachVisual();
        if (Combat)
            Combat->SetWeapon(Weapon);
    }

    AShooterPlayerState* AShooterCharacter::GetShooterPlayerState() const {
        if (auto* pc = GetController())
            return dynamic_cast<AShooterPlayerState*>(pc->GetPlayerState());
        return nullptr;
    }

    EShooterTeam AShooterCharacter::GetTeam() const {
        if (auto* ps = GetShooterPlayerState())
            return ps->GetTeam();
        return EShooterTeam::None;
    }

    bool AShooterCharacter::IsBotControlled() const {
        if (auto* ps = GetShooterPlayerState())
            return ps->IsBot();
        return bBot;
    }

    void AShooterCharacter::SetBotControlled(bool bInBot) {
        bBot = bInBot;
        if (auto* ps = GetShooterPlayerState())
            ps->SetIsBot(bInBot);
    }

    void AShooterCharacter::GetAimRay(glm::vec3& OutOrigin, glm::vec3& OutDirection) const {
        OutDirection = GetControlLookDirection();
        if (HasComponent<UCameraComponent>() && IsLocallyControlled()) {
            OutOrigin = GetComponent<UCameraComponent>().Camera.GetPosition();
            OutDirection = GetComponent<UCameraComponent>().Camera.GetForwardDirection();
        } else {
            OutOrigin = GetActorLocation();
        }
    }

    void AShooterCharacter::ApplyDamageFrom(const FDamageInfo& InInfo) {
        if (Health)
            Health->ApplyDamage(InInfo);
        PendingDamageFlash = 1;
        if (IsLocallyControlled()) {
            if (auto* pc = dynamic_cast<AShooterPlayerController*>(GetController()))
                pc->NotifyTookDamage();
        }
    }

    void AShooterCharacter::PulseHitConfirm(bool bKill) {
        PendingHitConfirm = bKill ? 2 : 1;
        auto* pc = dynamic_cast<AShooterPlayerController*>(GetController());
        if (!pc)
            return;
        const bool bLocalHud = IsLocallyControlled() || (World && pc == World->GetFirstPlayerController());
        if (bLocalHud)
            pc->NotifyConfirmedHit(bKill);
    }

    void AShooterCharacter::OnServerDeath(const FDamageInfo& InInfo) {
        bDeadFrozen = true;
        bSprinting = false;
        if (Combat)
            Combat->SetFireHeld(false);
        if (Weapon)
            Weapon->SetFireHeld(false);
        if (auto* gm = dynamic_cast<AShooterGameMode*>(World ? World->GetGameMode() : nullptr))
            gm->NotifyDeath(*this, InInfo);
    }

    void AShooterCharacter::OnServerRespawn(const glm::vec3& InLocation) {
        bDeadFrozen = false;
        SetActorLocation(InLocation);
        if (Health)
            Health->ResetHealth();
        if (Weapon)
            Weapon->ResetMagazine();
        bSprinting = false;
    }

    bool AShooterCharacter::IsForwardPressed() const {
        return FInput::IsKeyPressed(FInputSettings::Get().MoveForwardKey);
    }

    void AShooterCharacter::UpdateSprintState() {
        const bool bWantSprint = !IsBotControlled() && FInput::IsKeyPressed(FInputSettings::Get().SprintKey);
        const bool bReload = Weapon && Weapon->IsReloading();
        const bool bFire =
            Weapon && (Weapon->IsFiring() || (Combat && FInput::IsMouseButtonPressed(Mouse::ButtonLeft)));
        bSprinting = bWantSprint && IsForwardPressed() && !bReload && !bFire && !bDeadFrozen && !IsFalling();
        if (bSprinting && Combat)
            Combat->SetFireHeld(false);
    }

    void AShooterCharacter::ApplyLookRotation() {
        ApplyYawOnlyActorRotation();
    }

    void AShooterCharacter::SetupPlayerInputComponent(float DeltaSeconds) {
        if (bDeadFrozen)
            return;
        if (IsBotControlled())
            return;
        if (APlayerController* pc = dynamic_cast<APlayerController*>(GetController())) {
            if (!pc->IsGameInputAllowed())
                return;
        }

        ApplyLookInput(DeltaSeconds, false);
        UpdateSprintState();
        const float scale = bSprinting ? GetSprintMultiplier() : 1.0f;
        ApplyMoveInput(DeltaSeconds, scale);

        const bool bFire = FInput::IsMouseButtonPressed(Mouse::ButtonLeft);
        if (Combat) {
            Combat->SetFireHeld(bFire && !bSprinting);
            const bool bReload = FInput::IsKeyPressed(Key::R);
            if (bReload && !bReloadWasDown)
                Combat->RequestReload();
            bReloadWasDown = bReload;
        }
    }

    void AShooterCharacter::Tick(float DeltaSeconds) {
        if (GetLocalRole() == ENetRole::Authority && !IsLocallyControlled())
            FlushPendingNetInput(DeltaSeconds);
        if (bDeadFrozen) {
            if (GetLocalRole() != ENetRole::SimulatedProxy)
                ACharacter::Tick(DeltaSeconds);
            return;
        }
        ACharacter::Tick(DeltaSeconds);
    }

    void AShooterCharacter::UpdateAnimInstance(UAnimInstance& InAnim) const {
        ACharacter::UpdateAnimInstance(InAnim);
        InAnim.SetBool("bIsDead", Health && Health->IsDead());
        InAnim.SetBool("IsSprinting", bSprinting);
        InAnim.SetBool("IsAiming", bAiming && !bSprinting);
        InAnim.SetBool("IsFiring", Weapon && Weapon->IsFiring());
        InAnim.SetBool("IsReloading", Weapon && Weapon->IsReloading());
        InAnim.SetFloat("AimPitch", GetControlPitch());
        InAnim.SetFloat("AimYaw", 0.0f);
        InAnim.SetBool("bIsFalling", IsFalling());
    }

    void AShooterCharacter::BotMoveToward(const glm::vec3& InWorldTarget, float DeltaSeconds, float InSpeedScale) {
        if (bDeadFrozen)
            return;
        glm::vec3 delta = InWorldTarget - GetActorLocation();
        delta.y = 0.0f;
        if (glm::length(delta) < 0.15f)
            return;
        if (auto move = GetCharacterMovement()) {
            glm::vec3 vel = glm::normalize(delta) * GetMoveSpeed() * InSpeedScale;
            move->RequestDirectMove(vel, true);
        }
        (void)DeltaSeconds;
    }

    void AShooterCharacter::BotLookAt(const glm::vec3& InWorldPoint) {
        glm::vec3 delta = InWorldPoint - GetActorLocation();
        if (glm::length(delta) < 1e-4f)
            return;
        glm::vec3 n = glm::normalize(delta);
        SetControlYaw(glm::degrees(std::atan2(n.z, n.x)));
        SetControlPitch(glm::degrees(std::asin(std::clamp(n.y, -1.0f, 1.0f))));
        ApplyLookRotation();
    }

    void AShooterCharacter::BotSetFireHeld(bool bHeld) {
        if (Combat)
            Combat->SetFireHeld(bHeld && !bSprinting && !bDeadFrozen);
    }

    void AShooterCharacter::BotRequestReload() {
        if (Combat)
            Combat->RequestReload();
    }

    void AShooterCharacter::SerializeReplication(std::vector<uint8_t>& OutBytes) const {
        FNetBlob::WriteF32(OutBytes, Health ? Health->GetHealth() : 0.0f);
        FNetBlob::WriteI32(OutBytes, Weapon ? Weapon->GetCurrentAmmo() : 0);
        FNetBlob::WriteI32(OutBytes, Weapon ? Weapon->GetMagazineSize() : 0);
        FNetBlob::WriteU8(OutBytes, Weapon && Weapon->IsReloading() ? 1 : 0);
        FNetBlob::WriteU8(OutBytes, bSprinting ? 1 : 0);
        FNetBlob::WriteU8(OutBytes, Health && Health->IsDead() ? 1 : 0);
        FNetBlob::WriteU8(OutBytes, PendingHitConfirm);
        FNetBlob::WriteU8(OutBytes, PendingDamageFlash);
        PendingHitConfirm = 0;
        PendingDamageFlash = 0;
    }

    void AShooterCharacter::DeserializeReplication(const uint8_t* InData, size_t InSize) {
        std::vector<uint8_t> bytes(InData, InData + InSize);
        size_t offset = 0;
        float hp = 0;
        int32_t ammo = 0, mag = 0;
        uint8_t reload = 0, sprint = 0, dead = 0, hitConfirm = 0, damageFlash = 0;
        if (!FNetBlob::ReadF32(bytes, offset, hp) || !FNetBlob::ReadI32(bytes, offset, ammo) ||
            !FNetBlob::ReadI32(bytes, offset, mag) || !FNetBlob::ReadU8(bytes, offset, reload) ||
            !FNetBlob::ReadU8(bytes, offset, sprint) || !FNetBlob::ReadU8(bytes, offset, dead) ||
            !FNetBlob::ReadU8(bytes, offset, hitConfirm) || !FNetBlob::ReadU8(bytes, offset, damageFlash))
            return;
        if (Health)
            Health->SetHealth(hp);
        if (Weapon)
            Weapon->ApplyReplicatedState(ammo, reload != 0);
        bSprinting = sprint != 0;
        bDeadFrozen = dead != 0;
        (void)mag;
        if (auto* pc = dynamic_cast<AShooterPlayerController*>(GetController())) {
            if (hitConfirm)
                pc->NotifyConfirmedHit(hitConfirm == 2);
            if (damageFlash)
                pc->NotifyTookDamage();
        }
    }

    void AShooterCharacter::SerializeControlInput(std::vector<uint8_t>& OutBytes) const {
        FNetBlob::WriteF32(OutBytes, GetControlYaw());
        FNetBlob::WriteF32(OutBytes, GetControlPitch());
        uint8_t bits = 0;
        const auto& in = FInputSettings::Get();
        if (FInput::IsKeyPressed(in.MoveForwardKey))
            bits |= 1;
        if (FInput::IsKeyPressed(in.MoveBackwardKey))
            bits |= 2;
        if (FInput::IsKeyPressed(in.MoveLeftKey))
            bits |= 4;
        if (FInput::IsKeyPressed(in.MoveRightKey))
            bits |= 8;
        if (FInput::IsKeyPressed(in.JumpKey))
            bits |= 16;
        if (FInput::IsKeyPressed(in.SprintKey))
            bits |= 32;
        if (FInput::IsMouseButtonPressed(Mouse::ButtonLeft))
            bits |= 64;
        if (FInput::IsKeyPressed(Key::R))
            bits |= 128;
        FNetBlob::WriteU8(OutBytes, bits);
    }

    void AShooterCharacter::ApplyControlInput(const uint8_t* InData, size_t InSize) {
        std::vector<uint8_t> bytes(InData, InData + InSize);
        size_t offset = 0;
        float yaw = 0, pitch = 0;
        uint8_t bits = 0;
        if (!FNetBlob::ReadF32(bytes, offset, yaw) || !FNetBlob::ReadF32(bytes, offset, pitch) ||
            !FNetBlob::ReadU8(bytes, offset, bits))
            return;
        SetControlYaw(yaw);
        SetControlPitch(pitch);
        PendingNetBits = bits;
        bHasPendingNetInput = true;
        if (bDeadFrozen)
            return;
        bSprinting = (bits & 32) != 0 && (bits & 1) != 0 && (bits & 64) == 0;
        if (Combat) {
            Combat->SetFireHeld((bits & 64) != 0 && !bSprinting);
            if (bits & 128)
                Combat->RequestReload();
        }
        ApplyLookRotation();
    }

    void AShooterCharacter::FlushPendingNetInput(float DeltaSeconds) {
        if (!bHasPendingNetInput)
            return;
        bHasPendingNetInput = false;
        if (bDeadFrozen)
            return;
        const uint8_t bits = PendingNetBits;
        bSprinting = (bits & 32) != 0 && (bits & 1) != 0 && (bits & 64) == 0;
        glm::vec3 forward = GetControlPlanarForward();
        glm::vec3 right(-forward.z, 0.0f, forward.x);
        const float scale = bSprinting ? GetSprintMultiplier() : 1.0f;
        glm::vec3 wish(0.0f);
        if (bits & 1)
            wish += forward;
        if (bits & 2)
            wish -= forward;
        if (bits & 4)
            wish -= right;
        if (bits & 8)
            wish += right;
        if (auto move = GetCharacterMovement()) {
            if (glm::length(wish) > 1e-4f)
                move->AddInputVector(glm::normalize(wish) * GetMoveSpeed() * scale);
        }
        if (bits & 16)
            Jump();
        ApplyLookRotation();
    }

} // namespace Leon
