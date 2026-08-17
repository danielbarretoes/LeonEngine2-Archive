#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentPlayerState.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentPlayerController.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "Core/FInput.hpp"
#include "Core/FInputSettings.hpp"
#include "Engine/Components.hpp"
#include "Engine/UEngine.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Physics/FSimplePhysicsScene.hpp"
#include "Renderer/FRenderingMath.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>

namespace Leon {

    ALeonTournamentCharacter::ALeonTournamentCharacter(entt::entity InHandle, UWorld* InWorld,
                                                       const std::string& InName)
        : ACharacter(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentCharacter");
    }

    void ALeonTournamentCharacter::PostInitializeComponents() {
        ACharacter::PostInitializeComponents();

        if (!Health)
            Health = AddActorComponent<UHealthComponent>("Health");
        if (!Combat)
            Combat = AddActorComponent<ULeonTournamentCombatComponent>("Combat");

        if (GetMesh()) {
            AnimInst = MakeRef<ULeonTournamentAnimInstance>("LeonTournamentAnim");
            GetMesh()->SetAnimInstance(AnimInst);
            ELeonTournamentCharacterSkin skin = ELeonTournamentCharacterSkin::YBot;
            if (auto* ps = GetPlayerState())
                skin = ps->GetCharacterSkin();
            else if (UEngine::HasInstance()) {
                if (auto* gi = dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get()))
                    skin = gi->GetSelectedCharacterSkin();
            }
            ApplyCharacterSkin(skin);
        }
        UpdatePresentationVisibility();
    }

    void ALeonTournamentCharacter::ApplyCharacterSkin(ELeonTournamentCharacterSkin InSkin) {
        CharacterSkin = InSkin;
        if (!GetMesh())
            return;
        GetMesh()->SetMeshAssetPath(LeonTournamentCharacterMeshPath(InSkin));
        if (AnimInst)
            GetMesh()->SetAnimInstance(AnimInst);

        // Normalize bind-pose height so every skin stands at the same design height.
        float uniformScale = 1.0f;
        float feetY = 0.0f;
        const float targetH = LeonTournamentCharacterTargetHeightMeters(InSkin);
        if (auto skm = GetMesh()->GetSkeletalMesh()) {
            const float meshH = skm->GetBoundsMax().y - skm->GetBoundsMin().y;
            feetY = skm->GetBoundsMin().y;
            if (targetH > 1e-3f && meshH > 1e-3f)
                uniformScale = targetH / meshH;
        }
        GetMesh()->SetRelativeScale3D(glm::vec3(uniformScale));
        // Capsule bottom is -halfHeight; compensate bind-pose foot offset after scale.
        GetMesh()->SetRelativeLocation(glm::vec3(0.0f, -GetCapsuleHalfHeight() - feetY * uniformScale, 0.0f));
    }

    void ALeonTournamentCharacter::BeginPlay() {
        ACharacter::BeginPlay();
        if (ShouldSpawnWeapon())
            EnsureWeapon();
        if (Health && !bDeathBound) {
            Health->OnDeath.push_back([this](const FDamageInfo& info) { OnServerDeath(info); });
            bDeathBound = true;
        }
        UpdatePresentationVisibility();
    }

    void ALeonTournamentCharacter::EndPlay() {
        for (auto*& slot : Inventory) {
            if (slot && World) {
                slot->SetOwnerCharacter(nullptr);
                World->DestroyActor(slot);
            }
            slot = nullptr;
        }
        Weapon = nullptr;
        ACharacter::EndPlay();
    }

    ALeonTournamentWeapon* ALeonTournamentCharacter::SpawnWeaponActor(ELeonTournamentWeaponId InId) {
        if (!World)
            return nullptr;
        ALeonTournamentWeapon* weap = nullptr;
        switch (InId) {
        case ELeonTournamentWeaponId::Shotgun:
            weap = World->SpawnActor<ALeonTournamentShotgun>("Shotgun");
            break;
        case ELeonTournamentWeaponId::Rocket:
            weap = World->SpawnActor<ALeonTournamentRocketLauncher>("RocketLauncher");
            break;
        case ELeonTournamentWeaponId::Laser:
            weap = World->SpawnActor<ALeonTournamentLaserRifle>("LaserRifle");
            break;
        case ELeonTournamentWeaponId::Grenade:
            weap = World->SpawnActor<ALeonTournamentGrenadeLauncher>("GrenadeLauncher");
            break;
        case ELeonTournamentWeaponId::Flamethrower:
            weap = World->SpawnActor<ALeonTournamentFlamethrower>("Flamethrower");
            break;
        case ELeonTournamentWeaponId::Rifle:
        default:
            weap = World->SpawnActor<ALeonTournamentRifle>("Rifle");
            break;
        }
        if (!weap)
            return nullptr;
        weap->SetOwnerCharacter(this);
        weap->AttachVisual();
        weap->SetVisualHidden(true);
        weap->SetFireHeld(false);
        return weap;
    }

    void ALeonTournamentCharacter::EnsureWeapon() {
        if (!World)
            return;
        if (!Inventory[static_cast<size_t>(ELeonTournamentWeaponId::Rifle)]) {
            Inventory[static_cast<size_t>(ELeonTournamentWeaponId::Rifle)] =
                SpawnWeaponActor(ELeonTournamentWeaponId::Rifle);
        }
        SelectWeapon(ELeonTournamentWeaponId::Rifle);
    }

    void ALeonTournamentCharacter::ClearInventoryKeepRifle() {
        for (size_t i = 0; i < Inventory.size(); ++i) {
            if (i == static_cast<size_t>(ELeonTournamentWeaponId::Rifle))
                continue;
            if (Inventory[i] && World) {
                Inventory[i]->SetOwnerCharacter(nullptr);
                World->DestroyActor(Inventory[i]);
            }
            Inventory[i] = nullptr;
        }
        if (auto* rifle = Inventory[static_cast<size_t>(ELeonTournamentWeaponId::Rifle)])
            rifle->ResetMagazine();
        SelectWeapon(ELeonTournamentWeaponId::Rifle);
    }

    bool ALeonTournamentCharacter::HasWeapon(ELeonTournamentWeaponId InId) const {
        const size_t idx = static_cast<size_t>(InId);
        return idx < Inventory.size() && Inventory[idx] != nullptr;
    }

    ALeonTournamentWeapon* ALeonTournamentCharacter::GetInventoryWeapon(ELeonTournamentWeaponId InId) const {
        const size_t idx = static_cast<size_t>(InId);
        return idx < Inventory.size() ? Inventory[idx] : nullptr;
    }

    bool ALeonTournamentCharacter::SelectWeapon(ELeonTournamentWeaponId InId) {
        const size_t idx = static_cast<size_t>(InId);
        if (idx >= Inventory.size() || !Inventory[idx])
            return false;
        if (Weapon && Weapon != Inventory[idx]) {
            Weapon->SetFireHeld(false);
            Weapon->CancelReload();
            Weapon->SetVisualHidden(true);
        }
        ActiveWeaponId = InId;
        Weapon = Inventory[idx];
        Weapon->SetVisualHidden(false);
        if (Combat)
            Combat->SetWeapon(Weapon);
        UpdatePresentationVisibility();
        return true;
    }

    void ALeonTournamentCharacter::CycleWeapon(int InDirection) {
        if (InDirection == 0)
            return;
        const int count = static_cast<int>(ELeonTournamentWeaponId::Count);
        int cur = static_cast<int>(ActiveWeaponId);
        for (int step = 0; step < count; ++step) {
            cur = (cur + (InDirection > 0 ? 1 : -1) + count) % count;
            if (SelectWeapon(static_cast<ELeonTournamentWeaponId>(cur)))
                return;
        }
    }

    bool ALeonTournamentCharacter::GiveWeapon(ELeonTournamentWeaponId InId, bool bAutoSwitch) {
        const size_t idx = static_cast<size_t>(InId);
        if (idx >= Inventory.size() || !World)
            return false;
        if (Inventory[idx]) {
            Inventory[idx]->ResetMagazine();
            if (bAutoSwitch)
                SelectWeapon(InId);
            return true;
        }
        Inventory[idx] = SpawnWeaponActor(InId);
        if (!Inventory[idx])
            return false;
        if (bAutoSwitch)
            SelectWeapon(InId);
        return true;
    }

    void ALeonTournamentCharacter::HandleWeaponSwitchInput() {
        const bool k1 = FInput::IsKeyPressed(Key::D1);
        const bool k2 = FInput::IsKeyPressed(Key::D2);
        const bool k3 = FInput::IsKeyPressed(Key::D3);
        const bool k4 = FInput::IsKeyPressed(Key::D4);
        const bool k5 = FInput::IsKeyPressed(Key::D5);
        const bool k6 = FInput::IsKeyPressed(Key::D6);
        const bool kQ = FInput::IsKeyPressed(Key::Q);
        const bool kE = FInput::IsKeyPressed(Key::E);
        if (k1 && !bKey1WasDown)
            SelectWeapon(ELeonTournamentWeaponId::Rifle);
        if (k2 && !bKey2WasDown)
            SelectWeapon(ELeonTournamentWeaponId::Shotgun);
        if (k3 && !bKey3WasDown)
            SelectWeapon(ELeonTournamentWeaponId::Rocket);
        if (k4 && !bKey4WasDown)
            SelectWeapon(ELeonTournamentWeaponId::Laser);
        if (k5 && !bKey5WasDown)
            SelectWeapon(ELeonTournamentWeaponId::Grenade);
        if (k6 && !bKey6WasDown)
            SelectWeapon(ELeonTournamentWeaponId::Flamethrower);
        if (kQ && !bKeyQWasDown)
            CycleWeapon(-1);
        if (kE && !bKeyEWasDown)
            CycleWeapon(1);
        bKey1WasDown = k1;
        bKey2WasDown = k2;
        bKey3WasDown = k3;
        bKey4WasDown = k4;
        bKey5WasDown = k5;
        bKey6WasDown = k6;
        bKeyQWasDown = kQ;
        bKeyEWasDown = kE;

        const FInputSettings& input = FInputSettings::Get();
        if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId)) {
            const bool dL = FInput::IsGamepadButtonPressed(GamepadButton::DPadLeft, input.GamepadId);
            const bool dR = FInput::IsGamepadButtonPressed(GamepadButton::DPadRight, input.GamepadId);
            if (dL && !bPadDLeftWasDown)
                CycleWeapon(-1);
            if (dR && !bPadDRightWasDown)
                CycleWeapon(1);
            bPadDLeftWasDown = dL;
            bPadDRightWasDown = dR;
        }
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
        // Direction follows look / crosshair. In third person the shot starts at capsule centre
        // (mid-body) so traces are not cast from the spring-arm camera behind the pawn.
        OutDirection = GetControlLookDirection();
        if (IsThirdPerson())
            OutOrigin = GetActorLocation();
        else
            OutOrigin = GetPawnViewLocation();
    }

    glm::vec3 ALeonTournamentCharacter::GetMuzzleSocketLocation() const {
        if (IsThirdPerson())
            return GetActorLocation();
        const glm::vec3 look = GetControlLookDirection();
        glm::vec3 right, up;
        StableViewBasis(look, right, up);
        (void)up;
        const float yFromActor = LeonTournamentFireHeightFromGround - GetCapsuleHalfHeight();
        return GetActorLocation() + glm::vec3(0.0f, yFromActor, 0.0f) + right * LeonTournamentFireRightOffset +
               look * LeonTournamentFireForwardOffset;
    }

    void ALeonTournamentCharacter::ApplyLaunchVelocity(const glm::vec3& InVelocity) {
        if (bDeadFrozen) {
            if (auto cap = GetCapsuleComponent(); cap && cap->IsSimulatingPhysics())
                cap->AddImpulse(InVelocity * 55.0f);
            return;
        }
        if (auto move = GetCharacterMovement()) {
            glm::vec3 v = move->GetVelocity() + InVelocity;
            if (InVelocity.y > 0.0f)
                v.y = std::max(v.y, InVelocity.y);
            move->SetVelocity(v);
            if (v.y > 0.5f || glm::length(glm::vec3(v.x, 0.0f, v.z)) > 0.5f)
                move->SetMovementMode(EMovementMode::Falling);
        }
    }

    void ALeonTournamentCharacter::TryDodge() {
        if (bDeadFrozen || DodgeCooldownRemaining > 0.0f)
            return;
        auto move = GetCharacterMovement();
        if (!move || move->GetMovementMode() == EMovementMode::None)
            return;

        glm::vec3 dir(move->GetVelocity().x, 0.0f, move->GetVelocity().z);
        if (glm::length(dir) < 0.4f)
            dir = GetControlPlanarForward();
        else
            dir = glm::normalize(dir);

        constexpr float kDodgeSpeed = 14.0f;
        constexpr float kDodgeZ = 4.5f;
        glm::vec3 v = move->GetVelocity();
        v.x = dir.x * kDodgeSpeed;
        v.z = dir.z * kDodgeSpeed;
        v.y = std::max(v.y, kDodgeZ);
        move->SetVelocity(v);
        move->SetMovementMode(EMovementMode::Falling);
        DodgeCooldownRemaining = 0.75f;
    }

    void ALeonTournamentCharacter::ApplyDamageFrom(const FDamageInfo& InInfo) {
        if (!IsNetworkAuthority())
            return;
        if (glm::length(InInfo.Impulse) > 0.1f)
            PendingDeathImpulse = InInfo.Impulse;
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
        // FP body hide is UpdateMeshVisibility (!third-person + local). Never hide corpses.
        SetMeshHiddenInGame(false);
        for (auto* slot : Inventory) {
            if (!slot)
                continue;
            slot->SetVisualHidden(bDeadFrozen || slot != Weapon);
        }
        UpdateTeamOutline();
    }

    void ALeonTournamentCharacter::UpdateTeamOutline() {
        if (!HasComponent<FSkinnedMeshRenderState>())
            return;
        auto& skel = GetComponent<FSkinnedMeshRenderState>();
        // Local first-person body is hidden; no silhouette on self.
        if (IsLocallyControlled() && !IsThirdPerson()) {
            skel.bDrawOutline = false;
            return;
        }
        if (!skel.bVisible) {
            skel.bDrawOutline = false;
            return;
        }

        ELeonTournamentTeam localTeam = ELeonTournamentTeam::None;
        if (World) {
            if (auto* pc = World->GetFirstPlayerController()) {
                if (auto* localPawn = pc->GetPawn<ALeonTournamentCharacter>())
                    localTeam = localPawn->GetTeam();
            }
        }

        const ELeonTournamentTeam myTeam = GetTeam();
        const bool bAlly = localTeam != ELeonTournamentTeam::None && myTeam != ELeonTournamentTeam::None &&
                           myTeam == localTeam;
        skel.bDrawOutline = true;
        skel.OutlineColor = bAlly ? glm::vec3(0.12f, 0.95f, 0.32f) : glm::vec3(1.0f, 0.14f, 0.1f);
        skel.OutlineWidth = 0.016f;
    }

    void ALeonTournamentCharacter::UpdateFootstepAudio(float DeltaSeconds) {
        FootstepCooldown = std::max(0.0f, FootstepCooldown - DeltaSeconds);
        if (bDeadFrozen || IsFalling())
            return;
        const float speed = GetAnimRepState().Speed;
        if (speed < 1.15f || FootstepCooldown > 0.0f)
            return;
        const float maxSpeed = std::max(1.0f, GetMoveSpeed());
        FootstepCooldown = std::clamp(0.42f * (maxSpeed / speed), 0.26f, 0.52f);
        UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_Footstep", GetActorLocation(), 0.45f, 1200.0f);
    }

    void ALeonTournamentCharacter::BeginDeathRagdoll() {
        auto cap = GetCapsuleComponent();
        if (!cap)
            return;
        // Sync kinematic pose into the body before enabling dynamics.
        cap->SyncPhysicsTransform();
        cap->SetMass(70.0f);
        cap->SetLinearDamping(1.15f);
        if (auto* body = dynamic_cast<FSimplePhysicsBody*>(cap->GetPhysicsBody())) {
            body->Info.Mass = 70.0f;
            body->Info.LinearDamping = 1.15f;
            body->Info.bEnableGravity = true;
        }
        cap->SetSimulatePhysics(true);
        glm::vec3 impulse = PendingDeathImpulse;
        if (glm::length(impulse) < 1.0f) {
            impulse = GetControlLookDirection() * -80.0f;
            impulse.y = 140.0f;
        } else {
            impulse *= 70.0f;
            impulse.y = std::max(impulse.y, 160.0f);
        }
        cap->AddImpulse(impulse);
    }

    void ALeonTournamentCharacter::StopDeathRagdoll() {
        if (auto cap = GetCapsuleComponent()) {
            cap->SetSimulatePhysics(false);
            cap->SetLinearDamping(0.01f);
            if (auto* body = cap->GetPhysicsBody())
                body->SetLinearVelocity(glm::vec3(0.0f));
        }
    }

    void ALeonTournamentCharacter::OnServerDeath(const FDamageInfo& InInfo) {
        bDeadFrozen = true;
        // Flow: death presentation
        // 1. Force third-person so local players see the corpse / ragdoll
        // 2. Stop movement, play death anim, enable capsule ragdoll
        // 3. Authority notifies GameMode (respawn timer, score)
        bDeathForcedThirdPerson = false;
        if (IsLocallyControlled() && !IsThirdPerson()) {
            bDeathForcedThirdPerson = true;
            SetThirdPerson(true);
        }
        if (auto move = GetCharacterMovement()) {
            move->StopMovementImmediately();
            move->SetMovementMode(EMovementMode::None);
        }
        if (Combat)
            Combat->SetFireHeld(false);
        if (Weapon)
            Weapon->SetFireHeld(false);
        if (AnimInst)
            AnimInst->PlayDeathMontage();
        BeginDeathRagdoll();
        UpdatePresentationVisibility();
        if (!IsNetworkAuthority())
            return;
        if (auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World ? World->GetGameMode() : nullptr))
            gm->NotifyDeath(*this, InInfo);
    }

    void ALeonTournamentCharacter::OnServerRespawn(const glm::vec3& InLocation) {
        bDeadFrozen = false;
        DodgeCooldownRemaining = 0.0f;
        PendingDeathImpulse = {0.0f, 4.0f, 0.0f};
        bAimingDownSights = false;
        StopDeathRagdoll();
        if (bDeathForcedThirdPerson)
            bDeathForcedThirdPerson = false;
        if (auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World ? World->GetGameMode() : nullptr))
            gm->ApplyCameraPreference(this);
        if (AnimInst)
            AnimInst->ClearOverrideSequence();
        SetControlPitch(0.0f);
        SetActorLocation(InLocation);
        SnapToFloorPublic();
        if (Health)
            Health->ResetHealth();
        ClearInventoryKeepRifle();
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

        const FInputSettings& input = FInputSettings::Get();
        bool bDodge = FInput::IsKeyPressed(Key::LeftAlt) || FInput::IsKeyPressed(Key::C);
        if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
            FInput::IsGamepadButtonPressed(GamepadButton::B, input.GamepadId))
            bDodge = true;
        if (bDodge && !bDodgeWasDown)
            TryDodge();
        bDodgeWasDown = bDodge;

        bool bFire = FInput::IsMouseButtonPressed(Mouse::ButtonLeft);
        if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
            FInput::GetGamepadTrigger(GamepadAxis::RightTrigger, input.GamepadId, input.GamepadTriggerThreshold) >
                0.0f)
            bFire = true;

        bool bReload = FInput::IsKeyPressed(Key::R);
        if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
            FInput::IsGamepadButtonPressed(GamepadButton::X, input.GamepadId))
            bReload = true;

        if (Combat) {
            Combat->SetFireHeld(bFire);
            if (bReload && !bReloadWasDown)
                Combat->RequestReload();
            bReloadWasDown = bReload;
        }
        HandleWeaponSwitchInput();
        HandleCameraToggleInput();
    }

    void ALeonTournamentCharacter::HandleCameraToggleInput() {
        bool bDown = FInput::IsKeyPressed(Key::V);
        const FInputSettings& input = FInputSettings::Get();
        if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
            FInput::IsGamepadButtonPressed(GamepadButton::RightThumb, input.GamepadId))
            bDown = true;
        const bool bEdge = bDown && !bCameraToggleWasDown;
        bCameraToggleWasDown = bDown;
        if (!bEdge)
            return;
        auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World ? World->GetGameMode() : nullptr);
        const bool next = gm ? !gm->PrefersThirdPerson() : !IsThirdPerson();
        if (gm)
            gm->SetPreferThirdPerson(next);
        SetThirdPerson(next);
    }

    void ALeonTournamentCharacter::UpdateAimDownSights(float DeltaSeconds) {
        bool ads = false;
        if (!bDeadFrozen && !IsBotControlled() && Weapon && Weapon->CanAimDownSights()) {
            ads = FInput::IsMouseButtonPressed(Mouse::ButtonRight);
            const FInputSettings& input = FInputSettings::Get();
            if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
                FInput::GetGamepadTrigger(GamepadAxis::LeftTrigger, input.GamepadId, input.GamepadTriggerThreshold) >
                    0.0f)
                ads = true;
            if (APlayerController* pc = dynamic_cast<APlayerController*>(GetController())) {
                if (!pc->IsGameInputAllowed())
                    ads = false;
            }
        }
        bAimingDownSights = ads;
        const float hipFov = 95.0f;
        const float scopedFov = (Weapon && Weapon->CanAimDownSights()) ? Weapon->GetConfig().ScopeFOV : hipFov;
        const float target = ads ? scopedFov : hipFov;
        if (HasComponent<UCameraComponent>()) {
            auto& camComp = GetComponent<UCameraComponent>();
            const float cur = camComp.Camera.GetFOV();
            const float alpha = 1.0f - std::exp(-14.0f * std::max(DeltaSeconds, 0.0f));
            camComp.Camera.SetFOV(cur + (target - cur) * alpha);
        }
    }

    void ALeonTournamentCharacter::Tick(float DeltaSeconds) {
        if (GetLocalRole() == ENetRole::Authority && !IsLocallyControlled())
            FlushPendingNetInput(DeltaSeconds);
        ACharacter::Tick(DeltaSeconds);
        UpdateAimDownSights(DeltaSeconds);
        DodgeCooldownRemaining = std::max(0.0f, DodgeCooldownRemaining - DeltaSeconds);
        if (bDeadFrozen) {
            if (auto cap = GetCapsuleComponent(); cap && cap->IsSimulatingPhysics()) {
                glm::vec3 loc = GetActorLocation();
                const float minY = GetFloorZ() + GetCapsuleHalfHeight();
                bool bTouchedFloor = false;
                if (loc.y < minY) {
                    loc.y = minY;
                    bTouchedFloor = true;
                }
                loc.x = std::clamp(loc.x, -35.0f, 35.0f);
                loc.z = std::clamp(loc.z, -35.0f, 35.0f);
                SetActorLocation(loc);
                if (auto* body = cap->GetPhysicsBody()) {
                    glm::vec3 bodyLoc;
                    glm::quat rot;
                    body->GetTransform(bodyLoc, rot);
                    bodyLoc = loc + cap->GetRelativeLocation();
                    body->SetTransform(bodyLoc, rot);
                    glm::vec3 vel = body->GetLinearVelocity();
                    if (bTouchedFloor) {
                        vel.y = std::max(0.0f, vel.y);
                        vel.x *= 0.35f;
                        vel.z *= 0.35f;
                    }
                    const float speed = glm::length(vel);
                    if (speed > 42.0f)
                        vel *= 42.0f / speed;
                    body->SetLinearVelocity(vel);
                }
            }
        }
        UpdatePresentationVisibility();
        UpdateFootstepAudio(DeltaSeconds);
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
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(ActiveWeaponId));
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
        uint8_t reload = 0, weaponId = 0, dead = 0, hitConfirm = 0, damageFlash = 0;
        if (!FNetBlob::ReadF32(bytes, offset, hp) || !FNetBlob::ReadI32(bytes, offset, ammo) ||
            !FNetBlob::ReadI32(bytes, offset, mag) || !FNetBlob::ReadU8(bytes, offset, reload) ||
            !FNetBlob::ReadU8(bytes, offset, weaponId) || !FNetBlob::ReadU8(bytes, offset, dead) ||
            !FNetBlob::ReadU8(bytes, offset, hitConfirm) || !FNetBlob::ReadU8(bytes, offset, damageFlash))
            return;
        if (Health)
            Health->SetHealth(hp);
        if (weaponId < static_cast<uint8_t>(ELeonTournamentWeaponId::Count)) {
            const auto id = static_cast<ELeonTournamentWeaponId>(weaponId);
            if (!HasWeapon(id))
                GiveWeapon(id, false);
            SelectWeapon(id);
        }
        if (Weapon)
            Weapon->ApplyReplicatedState(ammo, reload != 0);
        bDeadFrozen = dead != 0;
        (void)mag;
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
