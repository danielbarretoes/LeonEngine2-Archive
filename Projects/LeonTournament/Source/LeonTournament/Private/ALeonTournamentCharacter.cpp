#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "Core/FInput.hpp"
#include "Engine/Components.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    ALeonTournamentCharacter::ALeonTournamentCharacter(entt::entity InHandle, UWorld* InWorld,
                                                       const std::string& InName)
        : ACharacter(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentCharacter");
    }

    void ALeonTournamentCharacter::PostInitializeComponents() {
        ACharacter::PostInitializeComponents();
        SetThirdPerson(false);

        if (!Health)
            Health = AddActorComponent<UHealthComponent>("Health");
        if (!Combat)
            Combat = AddActorComponent<ULeonTournamentCombatComponent>("Combat");

        if (GetMesh()) {
            AnimInst = MakeRef<ULeonTournamentAnimInstance>("LeonTournamentAnim");
            GetMesh()->SetAnimInstance(AnimInst);
            GetMesh()->SetMeshAssetPath("/Game/SkeletalMeshes/YBot.lskeletalmesh");
        }
        UpdatePresentationVisibility();
    }

    void ALeonTournamentCharacter::BeginPlay() {
        ACharacter::BeginPlay();
        EnsureWeapon();
        if (Health && !bDeathBound) {
            Health->OnDeath.push_back([this](const FDamageInfo& info) { OnServerDeath(info); });
            bDeathBound = true;
        }
        UpdatePresentationVisibility();
    }

    void ALeonTournamentCharacter::EndPlay() {
        if (Weapon && World) {
            Weapon->SetOwnerCharacter(nullptr);
            World->DestroyActor(Weapon);
            Weapon = nullptr;
        }
        ACharacter::EndPlay();
    }

    void ALeonTournamentCharacter::EnsureWeapon() {
        if (Weapon || !World)
            return;
        Weapon = World->SpawnActor<ALeonTournamentRifle>("Rifle");
        if (!Weapon)
            return;
        Weapon->SetOwnerCharacter(this);
        Weapon->AttachVisual();
        if (Combat)
            Combat->SetWeapon(Weapon);
    }

    ALeonTournamentPlayerState* ALeonTournamentCharacter::GetPlayerState() const {
        return dynamic_cast<ALeonTournamentPlayerState*>(APawn::GetPlayerState());
    }

    ELeonTournamentTeam ALeonTournamentCharacter::GetTeam() const {
        if (auto* ps = GetPlayerState())
            return ps->GetTeam();
        return ELeonTournamentTeam::None;
    }

    bool ALeonTournamentCharacter::IsBotControlled() const {
        if (auto* ps = GetPlayerState())
            return ps->IsBot();
        return bBot;
    }

    void ALeonTournamentCharacter::SetBotControlled(bool bInBot) {
        bBot = bInBot;
        if (auto* ps = GetPlayerState())
            ps->SetIsBot(bInBot);
    }

    void ALeonTournamentCharacter::GetAimRay(glm::vec3& OutOrigin, glm::vec3& OutDirection) const {
        // Eye origin lives on the actor; yaw/pitch live on control rotation.
        // Camera.GetForwardDirection() is only valid after UpdateCameraFromView.
        OutOrigin = GetActorLocation();
        OutDirection = GetControlLookDirection();
    }

    void ALeonTournamentCharacter::ApplyDamageFrom(const FDamageInfo& InInfo) {
        if (!IsNetworkAuthority())
            return;
        if (Health)
            Health->ApplyDamage(InInfo);
        PendingDamageFlash = 1;
        if (IsLocallyControlled()) {
            if (auto* pc = dynamic_cast<ALeonTournamentPlayerController*>(GetController()))
                pc->NotifyTookDamage();
        }
    }

    void ALeonTournamentCharacter::PulseHitConfirm(bool bKill) {
        PendingHitConfirm = bKill ? 2 : 1;
        auto* pc = dynamic_cast<ALeonTournamentPlayerController*>(GetController());
        if (!pc)
            return;
        const bool bLocalHud = IsLocallyControlled() || (World && pc == World->GetFirstPlayerController());
        if (bLocalHud)
            pc->NotifyConfirmedHit(bKill);
    }

    void ALeonTournamentCharacter::UpdatePresentationVisibility() {
        SetMeshHiddenInGame(bDeadFrozen);
        if (Weapon)
            Weapon->SetVisualHidden(bDeadFrozen);
    }

    void ALeonTournamentCharacter::OnServerDeath(const FDamageInfo& InInfo) {
        bDeadFrozen = true;
        if (auto move = GetCharacterMovement()) {
            move->StopMovementImmediately();
            move->SetMovementMode(EMovementMode::None);
        }
        if (Combat)
            Combat->SetFireHeld(false);
        if (Weapon)
            Weapon->SetFireHeld(false);
        UpdatePresentationVisibility();
        if (!IsNetworkAuthority())
            return;
        if (auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World ? World->GetGameMode() : nullptr))
            gm->NotifyDeath(*this, InInfo);
    }

    void ALeonTournamentCharacter::OnServerRespawn(const glm::vec3& InLocation) {
        bDeadFrozen = false;
        SetControlPitch(0.0f);
        SetActorLocation(InLocation);
        SnapToFloorPublic();
        if (Health)
            Health->ResetHealth();
        if (Weapon)
            Weapon->ResetMagazine();
        ResetMovementForRespawn();
        if (AnimInst)
            AnimInst->ResetPoseState();
        UpdatePresentationVisibility();
    }

    void ALeonTournamentCharacter::ApplyLookRotation() {
        ApplyYawOnlyActorRotation();
    }

    void ALeonTournamentCharacter::SetupPlayerInputComponent(float DeltaSeconds) {
        if (bDeadFrozen)
            return;
        if (IsBotControlled())
            return;
        if (APlayerController* pc = dynamic_cast<APlayerController*>(GetController())) {
            if (!pc->IsGameInputAllowed())
                return;
        }

        ApplyLookInput(DeltaSeconds, false);
        ApplyMoveInput(DeltaSeconds, 1.0f);

        const bool bFire = FInput::IsMouseButtonPressed(Mouse::ButtonLeft);
        if (Combat) {
            Combat->SetFireHeld(bFire);
            const bool bReload = FInput::IsKeyPressed(Key::R);
            if (bReload && !bReloadWasDown)
                Combat->RequestReload();
            bReloadWasDown = bReload;
        }
    }

    void ALeonTournamentCharacter::Tick(float DeltaSeconds) {
        if (GetLocalRole() == ENetRole::Authority && !IsLocallyControlled())
            FlushPendingNetInput(DeltaSeconds);
        ACharacter::Tick(DeltaSeconds);
        UpdatePresentationVisibility();
    }

    void ALeonTournamentCharacter::UpdateAnimInstance(UAnimInstance& InAnim) const {
        ACharacter::UpdateAnimInstance(InAnim);
        InAnim.SetBool("bIsDead", Health && Health->IsDead());
        InAnim.SetBool("IsFiring", Weapon && Weapon->IsFiring());
        InAnim.SetBool("IsReloading", Weapon && Weapon->IsReloading());
        InAnim.SetFloat("AimPitch", GetControlPitch());
        InAnim.SetFloat("AimYaw", 0.0f);
        InAnim.SetBool("bIsFalling", IsFalling());
    }

    void ALeonTournamentCharacter::BotMoveToward(const glm::vec3& InWorldTarget, float DeltaSeconds,
                                                 float InSpeedScale) {
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

    void ALeonTournamentCharacter::BotLookAt(const glm::vec3& InWorldPoint) {
        glm::vec3 delta = InWorldPoint - GetActorLocation();
        if (glm::length(delta) < 1e-4f)
            return;
        glm::vec3 n = glm::normalize(delta);
        SetControlYaw(glm::degrees(std::atan2(n.z, n.x)));
        SetControlPitch(glm::degrees(std::asin(std::clamp(n.y, -1.0f, 1.0f))));
        ApplyLookRotation();
    }

    void ALeonTournamentCharacter::BotSetFireHeld(bool bHeld) {
        if (Combat)
            Combat->SetFireHeld(bHeld && !bDeadFrozen);
    }

    void ALeonTournamentCharacter::BotRequestReload() {
        if (Combat)
            Combat->RequestReload();
    }

    void ALeonTournamentCharacter::SerializeReplication(std::vector<uint8_t>& OutBytes) const {
        FNetBlob::WriteF32(OutBytes, Health ? Health->GetHealth() : 0.0f);
        FNetBlob::WriteI32(OutBytes, Weapon ? Weapon->GetCurrentAmmo() : 0);
        FNetBlob::WriteI32(OutBytes, Weapon ? Weapon->GetMagazineSize() : 0);
        FNetBlob::WriteU8(OutBytes, Weapon && Weapon->IsReloading() ? 1 : 0);
        FNetBlob::WriteU8(OutBytes, 0);
        FNetBlob::WriteU8(OutBytes, Health && Health->IsDead() ? 1 : 0);
        FNetBlob::WriteU8(OutBytes, PendingHitConfirm);
        FNetBlob::WriteU8(OutBytes, PendingDamageFlash);
        PendingHitConfirm = 0;
        PendingDamageFlash = 0;
    }

    void ALeonTournamentCharacter::DeserializeReplication(const uint8_t* InData, size_t InSize) {
        std::vector<uint8_t> bytes(InData, InData + InSize);
        size_t offset = 0;
        float hp = 0;
        int32_t ammo = 0, mag = 0;
        uint8_t reload = 0, unusedSprint = 0, dead = 0, hitConfirm = 0, damageFlash = 0;
        if (!FNetBlob::ReadF32(bytes, offset, hp) || !FNetBlob::ReadI32(bytes, offset, ammo) ||
            !FNetBlob::ReadI32(bytes, offset, mag) || !FNetBlob::ReadU8(bytes, offset, reload) ||
            !FNetBlob::ReadU8(bytes, offset, unusedSprint) || !FNetBlob::ReadU8(bytes, offset, dead) ||
            !FNetBlob::ReadU8(bytes, offset, hitConfirm) || !FNetBlob::ReadU8(bytes, offset, damageFlash))
            return;
        if (Health)
            Health->SetHealth(hp);
        if (Weapon)
            Weapon->ApplyReplicatedState(ammo, reload != 0);
        bDeadFrozen = dead != 0;
        (void)mag;
        (void)unusedSprint;
        UpdatePresentationVisibility();
        if (auto* pc = dynamic_cast<ALeonTournamentPlayerController*>(GetController())) {
            if (hitConfirm)
                pc->NotifyConfirmedHit(hitConfirm == 2);
            if (damageFlash)
                pc->NotifyTookDamage();
        }
    }

    void ALeonTournamentCharacter::SerializeControlInput(std::vector<uint8_t>& OutBytes) const {
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
        if (FInput::IsMouseButtonPressed(Mouse::ButtonLeft))
            bits |= 64;
        if (FInput::IsKeyPressed(Key::R))
            bits |= 128;
        FNetBlob::WriteU8(OutBytes, bits);
    }

    void ALeonTournamentCharacter::ApplyControlInput(const uint8_t* InData, size_t InSize) {
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
        if (Combat) {
            Combat->SetFireHeld((bits & 64) != 0);
            if (bits & 128)
                Combat->RequestReload();
        }
        ApplyLookRotation();
    }

    void ALeonTournamentCharacter::FlushPendingNetInput(float DeltaSeconds) {
        if (!bHasPendingNetInput)
            return;
        bHasPendingNetInput = false;
        if (bDeadFrozen)
            return;
        const uint8_t bits = PendingNetBits;
        glm::vec3 forward = GetControlPlanarForward();
        glm::vec3 right(-forward.z, 0.0f, forward.x);
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
                move->AddInputVector(glm::normalize(wish) * GetMoveSpeed());
        }
        if (bits & 16)
            Jump();
        ApplyLookRotation();
        (void)DeltaSeconds;
    }

} // namespace Leon
