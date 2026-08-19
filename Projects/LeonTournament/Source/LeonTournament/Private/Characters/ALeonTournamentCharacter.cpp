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
#include "Gameplay/FControlInput.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/USkeleton.hpp"
#include "Assets/FAnimRuntime.hpp"
#include "Renderer/FRenderingMath.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace Leon {

    namespace {
        constexpr uint16_t kServerRpcReload = 1;

        bool IsFootPlantBone(const std::string& InName) {
            const std::string n = ToLowerCopy(StripBoneNamespace(InName));
            if (n.find("footstep") != std::string::npos)
                return false;
            return n.find("toe") != std::string::npos || n.find("foot") != std::string::npos ||
                   n.find("ankle") != std::string::npos || n.find("ball") != std::string::npos;
        }

        float FootBoneMinY(const USkeletalMesh& InMesh) {
            auto skeleton = InMesh.GetSkeleton();
            if (!skeleton || skeleton->GetNumBones() == 0)
                return 1.0e9f;
            FPose rest;
            FAnimRuntime::RestPose(*skeleton, rest);
            std::vector<glm::mat4> component;
            FAnimRuntime::LocalToComponent(*skeleton, rest, component);
            float feetY = 1.0e9f;
            const auto& bones = skeleton->GetBones();
            for (size_t i = 0; i < bones.size() && i < component.size(); ++i) {
                if (!IsFootPlantBone(bones[i].Name))
                    continue;
                feetY = std::min(feetY, component[i][3].y);
            }
            return feetY;
        }

        float FootWeightedVertexMinY(const USkeletalMesh& InMesh, const std::vector<glm::mat4>* InPalette) {
            auto skeleton = InMesh.GetSkeleton();
            const auto& verts = InMesh.GetVertices();
            if (!skeleton || verts.empty())
                return 1.0e9f;
            const auto& bones = skeleton->GetBones();
            std::vector<char> isFoot(bones.size(), 0);
            for (size_t i = 0; i < bones.size(); ++i)
                isFoot[i] = IsFootPlantBone(bones[i].Name) ? 1 : 0;

            constexpr float kMinFootWeight = 0.35f;
            float minY = 1.0e9f;
            bool bFound = false;
            const bool bSkin = InPalette && InPalette->size() == bones.size();
            for (const auto& v : verts) {
                float footW = 0.0f;
                glm::vec4 skinned(0.0f);
                for (int k = 0; k < 4; ++k) {
                    const int idx = v.BoneIndices[k];
                    const float w = v.BoneWeights[k];
                    if (w <= 1e-4f || idx < 0 || idx >= static_cast<int>(isFoot.size()))
                        continue;
                    if (isFoot[static_cast<size_t>(idx)])
                        footW += w;
                    if (bSkin)
                        skinned += w * ((*InPalette)[static_cast<size_t>(idx)] * glm::vec4(v.Position, 1.0f));
                }
                if (footW < kMinFootWeight)
                    continue;
                const float y = bSkin ? skinned.y : v.Position.y;
                minY = std::min(minY, y);
                bFound = true;
            }
            return bFound ? minY : 1.0e9f;
        }

        float AabbMinY(const USkeletalMesh& InMesh) {
            const glm::vec3 extent = InMesh.GetBoundsMax() - InMesh.GetBoundsMin();
            if (glm::length(extent) <= 1e-4f)
                return 1.0e9f;
            return InMesh.GetBoundsMin().y;
        }
    } // namespace

    ALeonTournamentCharacter::ALeonTournamentCharacter(entt::entity InHandle, UWorld* InWorld,
                                                       const std::string& InName)
        : ACharacter(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentCharacter");
    }

    void ALeonTournamentCharacter::PostInitializeComponents() {
        ACharacter::PostInitializeComponents();
        if (GetName() == kLeonTournamentMenuShowcaseActorName)
            SetMenuShowcase(true);

        if (!Health)
            Health = AddActorComponent<UHealthComponent>("Health");
        if (!Inventory) {
            Inventory = AddActorComponent<UInventoryComponent>("Inventory");
            Inventory->SetSlotCount(static_cast<uint32_t>(ELeonTournamentWeaponId::Count));
        }
        if (!Combat)
            Combat = AddActorComponent<ULeonTournamentCombatComponent>("Combat");
        if (!Footsteps) {
            Footsteps = AddActorComponent<UFootstepComponent>("Footsteps");
            Footsteps->SetSoundPath("/Game/Audio/SFX_Footstep");
        }

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

    float ALeonTournamentCharacter::BindPoseFeetY(const USkeletalMesh& InMesh) {
        const float vertY = FootWeightedVertexMinY(InMesh, nullptr);
        if (vertY < 1.0e8f)
            return vertY;

        const float aabbMin = AabbMinY(InMesh);
        const bool bHasBounds = aabbMin < 1.0e8f;
        const float boneY = FootBoneMinY(InMesh);
        if (boneY >= 1.0e8f)
            return bHasBounds ? aabbMin : 0.0f;
        if (!bHasBounds)
            return boneY;
        // Loose AABB (cape, hair) hangs below the soles — plant on bones.
        constexpr float kLooseBoundsSlack = 0.08f;
        if (aabbMin < boneY - kLooseBoundsSlack)
            return boneY;
        return aabbMin;
    }

    float ALeonTournamentCharacter::SkinnedFeetY(const USkeletalMesh& InMesh, const std::vector<glm::mat4>& InPalette) {
        const float vertY = FootWeightedVertexMinY(InMesh, &InPalette);
        if (vertY < 1.0e8f)
            return vertY;
        return BindPoseFeetY(InMesh);
    }

    void ALeonTournamentCharacter::PlantMeshFeetOnCapsule() {
        if (!GetMesh())
            return;
        if (!bCachedBindPoseFeetY) {
            if (auto skm = GetMesh()->GetSkeletalMesh()) {
                CachedBindPoseFeetY = BindPoseFeetY(*skm);
                bCachedBindPoseFeetY = true;
            } else {
                CachedBindPoseFeetY = 0.0f;
            }
        }
        const float scale = GetMesh()->GetRelativeScale3D().y;
        GetMesh()->SetRelativeLocation(glm::vec3(0.0f, -GetCapsuleHalfHeight() - CachedBindPoseFeetY * scale, 0.0f));
    }

    void ALeonTournamentCharacter::SetMenuShowcase(bool bEnabled) {
        bMenuShowcase = bEnabled;
        if (!bEnabled)
            return;
        if (HasComponent<FCameraComponent>())
            GetComponent<FCameraComponent>().bPrimary = false;
        if (auto move = GetCharacterMovement()) {
            move->StopMovementImmediately();
            move->SetMovementMode(EMovementMode::None);
        }
        SetThirdPerson(true);
        UpdatePresentationVisibility();
    }

    bool ALeonTournamentCharacter::ShouldSpawnWeapon() const {
        return !bMenuShowcase;
    }

    void ALeonTournamentCharacter::ApplyCharacterSkin(ELeonTournamentCharacterSkin InSkin) {
        CharacterSkin = LeonTournamentClampCharacterSkin(InSkin);
        if (!GetMesh())
            return;
        GetMesh()->SetMeshAssetPath(LeonTournamentCharacterMeshPath(InSkin));
        if (AnimInst)
            GetMesh()->SetAnimInstance(AnimInst);

        // Normalize bind-pose height so every skin stands at the same design height.
        float uniformScale = 1.0f;
        const float targetH = LeonTournamentCharacterTargetHeightMeters(InSkin);
        auto skm = GetMesh()->GetSkeletalMesh();
        if (skm) {
            const float meshH = skm->GetBoundsMax().y - skm->GetBoundsMin().y;
            if (targetH > 1e-3f && meshH > 1e-3f)
                uniformScale = targetH / meshH;
        }
        GetMesh()->SetRelativeScale3D(glm::vec3(uniformScale));
        CachedBindPoseFeetY = skm ? BindPoseFeetY(*skm) : 0.0f;
        bCachedBindPoseFeetY = true;
        PlantMeshFeetOnCapsule();
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
        if (!HasWeapon(ELeonTournamentWeaponId::Rifle))
            GiveWeapon(ELeonTournamentWeaponId::Rifle, false);
        SelectWeapon(ELeonTournamentWeaponId::Rifle);
    }

    void ALeonTournamentCharacter::ClearInventoryKeepRifle() {
        if (!Inventory)
            return;
        const uint32_t rifle = static_cast<uint32_t>(ELeonTournamentWeaponId::Rifle);
        for (uint32_t i = 0; i < Inventory->GetSlotCount(); ++i) {
            if (i == rifle)
                continue;
            if (auto* weap = dynamic_cast<ALeonTournamentWeapon*>(Inventory->GetItem(i)))
                weap->SetOwnerCharacter(nullptr);
            Inventory->RemoveItem(i, true);
        }
        if (auto* keep = GetInventoryWeapon(ELeonTournamentWeaponId::Rifle))
            keep->ResetMagazine();
        SelectWeapon(ELeonTournamentWeaponId::Rifle);
    }

    bool ALeonTournamentCharacter::HasWeapon(ELeonTournamentWeaponId InId) const {
        return Inventory && Inventory->HasItem(static_cast<uint32_t>(InId));
    }

    ALeonTournamentWeapon* ALeonTournamentCharacter::GetInventoryWeapon(ELeonTournamentWeaponId InId) const {
        if (!Inventory)
            return nullptr;
        return dynamic_cast<ALeonTournamentWeapon*>(Inventory->GetItem(static_cast<uint32_t>(InId)));
    }

    ELeonTournamentWeaponId ALeonTournamentCharacter::GetActiveWeaponId() const {
        if (!Inventory)
            return ELeonTournamentWeaponId::Rifle;
        const uint32_t slot = Inventory->GetActiveSlot();
        if (slot >= static_cast<uint32_t>(ELeonTournamentWeaponId::Count))
            return ELeonTournamentWeaponId::Rifle;
        return static_cast<ELeonTournamentWeaponId>(slot);
    }

    bool ALeonTournamentCharacter::SelectWeapon(ELeonTournamentWeaponId InId) {
        auto* next = GetInventoryWeapon(InId);
        if (!next || !Inventory)
            return false;
        if (Weapon && Weapon != next) {
            Weapon->SetFireHeld(false);
            Weapon->CancelReload();
            Weapon->SetVisualHidden(true);
        }
        Inventory->SetActiveSlot(static_cast<uint32_t>(InId));
        Weapon = next;
        Weapon->SetVisualHidden(false);
        if (Combat)
            Combat->SetWeapon(Weapon);
        UpdatePresentationVisibility();
        return true;
    }

    void ALeonTournamentCharacter::CycleWeapon(int InDirection) {
        if (!Inventory || InDirection == 0)
            return;
        Inventory->Cycle(InDirection);
        SelectWeapon(GetActiveWeaponId());
    }

    bool ALeonTournamentCharacter::GiveWeapon(ELeonTournamentWeaponId InId, bool bAutoSwitch) {
        if (!Inventory || !World)
            return false;
        const uint32_t slot = static_cast<uint32_t>(InId);
        if (auto* existing = GetInventoryWeapon(InId)) {
            existing->ResetMagazine();
            if (bAutoSwitch)
                SelectWeapon(InId);
            return true;
        }
        ALeonTournamentWeapon* spawned = SpawnWeaponActor(InId);
        if (!spawned || !Inventory->GiveItem(slot, spawned)) {
            if (spawned)
                World->DestroyActor(spawned);
            return false;
        }
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
        // Same ray as the rendered camera / HUD crosshair (spring arm in third person).
        GetViewPoint(OutOrigin, OutDirection);
    }

    glm::vec3 ALeonTournamentCharacter::GetMuzzleSocketLocation() const {
        if (auto mesh = GetMesh()) {
            glm::vec3 loc{0.0f};
            if (mesh->FindSocket("muzzle") && mesh->GetSocketLocation("muzzle", loc))
                return loc;
        }
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
        LaunchCharacter(InVelocity);
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
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_Dodge", 0.55f);
    }

    void ALeonTournamentCharacter::ApplyDamageFrom(const FDamageInfo& InInfo) {
        if (!IsNetworkAuthority())
            return;
        if (bSpawnProtected)
            return;
        if (auto* attacker = dynamic_cast<ALeonTournamentCharacter*>(InInfo.Instigator)) {
            glm::vec3 delta = attacker->GetActorLocation() - GetActorLocation();
            delta.y = 0.0f;
            if (glm::length(delta) > 1e-4f) {
                delta = glm::normalize(delta);
                LastDamageYawDeg = glm::degrees(std::atan2(delta.z, delta.x));
                DamageIndicatorRemaining = 0.35f;
            }
        }
        if (Health)
            Health->ApplyDamage(InInfo);
        PendingDamageFlash = 1;
        if (IsLocallyControlled()) {
            if (auto* pc = dynamic_cast<ALeonTournamentPlayerController*>(GetController()))
                pc->NotifyTookDamage(LastDamageYawDeg);
        }
    }

    void ALeonTournamentCharacter::PulseHitConfirm(bool bKill, bool bHeadshot) {
        if (bHeadshot)
            PendingHitConfirm = bKill ? 4 : 3;
        else
            PendingHitConfirm = bKill ? 2 : 1;
        auto* pc = dynamic_cast<ALeonTournamentPlayerController*>(GetController());
        if (!pc)
            return;
        const bool bLocalHud = IsLocallyControlled() || (World && pc == World->GetFirstPlayerController());
        if (bLocalHud)
            pc->NotifyConfirmedHit(bKill, bHeadshot);
    }

    void ALeonTournamentCharacter::UpdatePresentationVisibility() {
        // FP body hide is UpdateMeshVisibility (!third-person + local). Never hide corpses.
        SetMeshHiddenInGame(false);
        if (Inventory) {
            for (AActor* actor : Inventory->GetItems()) {
                auto* slot = dynamic_cast<ALeonTournamentWeapon*>(actor);
                if (!slot)
                    continue;
                slot->SetVisualHidden(bDeadFrozen || slot != Weapon);
            }
        }
        UpdateTeamOutline();
    }

    void ALeonTournamentCharacter::UpdateTeamOutline() {
        if (!HasComponent<FSkinnedMeshRenderState>())
            return;
        auto& skel = GetComponent<FSkinnedMeshRenderState>();
        if (bMenuShowcase) {
            skel.bDrawOutline = false;
            return;
        }
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
        const bool bAlly =
            localTeam != ELeonTournamentTeam::None && myTeam != ELeonTournamentTeam::None && myTeam == localTeam;
        skel.bDrawOutline = true;
        if (bSpawnProtected) {
            const float alpha = SpawnProtectionRemaining > 0.0f
                                    ? std::clamp(SpawnProtectionRemaining / 2.5f, 0.25f, 1.0f)
                                    : 1.0f;
            skel.OutlineColor = bAlly ? glm::vec3(0.10f * alpha, 0.95f * alpha, 0.28f * alpha)
                                      : glm::vec3(1.0f * alpha, 0.55f * alpha, 0.08f * alpha);
            skel.OutlineWidth = 0.048f;
            return;
        }
        skel.OutlineColor = bAlly ? glm::vec3(0.10f, 0.95f, 0.28f) : glm::vec3(1.0f, 0.02f, 0.02f);
        skel.OutlineWidth = 0.038f;
    }

    void ALeonTournamentCharacter::BeginSpawnProtection(float InSeconds) {
        bSpawnProtected = InSeconds > 0.0f;
        SpawnProtectionRemaining = std::max(0.0f, InSeconds);
        UpdateTeamOutline();
    }

    void ALeonTournamentCharacter::TickSpawnProtection(float InDeltaSeconds) {
        if (!bSpawnProtected)
            return;
        SpawnProtectionRemaining = std::max(0.0f, SpawnProtectionRemaining - InDeltaSeconds);
        if (SpawnProtectionRemaining <= 0.0f)
            bSpawnProtected = false;
    }

    void ALeonTournamentCharacter::OnServerDeath(const FDamageInfo& InInfo) {
        bDeadFrozen = true;
        // Flow: death presentation
        // 1. Always third-person + free look orbit (control yaw does not spin the corpse)
        // 2. Pull spring arm out for a readable spectator view
        // 3. Stop movement; play death montage
        // 4. Authority notifies GameMode for scoring / respawn
        bDeathForcedThirdPerson = !IsThirdPerson();
        SetThirdPerson(true);
        if (auto arm = GetSpringArm()) {
            if (!bDeathCamArmOverride) {
                DeathCamArmLengthRestore = arm->TargetArmLength;
                bDeathCamArmOverride = true;
            }
            arm->TargetArmLength = 5.6f;
            arm->SocketOffset = {0.15f, 0.55f, 0.0f};
        }
        if (auto move = GetCharacterMovement()) {
            move->StopMovementImmediately();
            move->SetMovementMode(EMovementMode::None);
        }
        if (Combat)
            Combat->SetFireHeld(false);
        if (Weapon)
            Weapon->SetFireHeld(false);
        if (auto mesh = GetMesh())
            mesh->SetComponentTickEnabled(true);
        if (AnimInst)
            AnimInst->PlayDeathMontage();
        UpdatePresentationVisibility();
        if (IsLocallyControlled()) {
            if (auto* pc = dynamic_cast<ALeonTournamentPlayerController*>(GetController()))
                pc->NotifyLocalDeath();
        }
        if (!IsNetworkAuthority())
            return;
        if (auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World ? World->GetGameMode() : nullptr))
            gm->NotifyDeath(*this, InInfo);
    }

    void ALeonTournamentCharacter::OnServerRespawn(const glm::vec3& InLocation) {
        bDeadFrozen = false;
        bSpawnProtected = false;
        SpawnProtectionRemaining = 0.0f;
        DodgeCooldownRemaining = 0.0f;
        bAimingDownSights = false;
        if (AnimInst)
            AnimInst->ClearOverrideSequence();
        PlantMeshFeetOnCapsule();
        if (bDeathCamArmOverride) {
            if (auto arm = GetSpringArm()) {
                arm->TargetArmLength = DeathCamArmLengthRestore;
                arm->SocketOffset = {0.45f, 0.25f, 0.0f};
            }
            bDeathCamArmOverride = false;
        }
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
        if (APlayerController* pc = dynamic_cast<APlayerController*>(GetController())) {
            if (!pc->IsGameInputAllowed())
                return;
        }
        // Free death camera: orbit look only (actor yaw frozen via ShouldApplyControlYawToActor).
        if (bDeadFrozen) {
            if (!IsBotControlled())
                ApplyLookInput(DeltaSeconds, false);
            return;
        }
        if (IsBotControlled())
            return;

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
            FInput::GetGamepadTrigger(GamepadAxis::RightTrigger, input.GamepadId, input.GamepadTriggerThreshold) > 0.0f)
            bFire = true;

        bool bReload = FInput::IsKeyPressed(Key::R);
        if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
            FInput::IsGamepadButtonPressed(GamepadButton::X, input.GamepadId))
            bReload = true;

        if (Combat) {
            Combat->SetFireHeld(bFire);
            if (bReload && !bReloadWasDown) {
                if (IsNetworkAuthority())
                    Combat->RequestReload();
                else
                    CallServerRPC(kServerRpcReload);
            }
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
        if (HasComponent<FCameraComponent>()) {
            auto& camComp = GetComponent<FCameraComponent>();
            const float cur = camComp.Camera.GetFOV();
            const float alpha = 1.0f - std::exp(-14.0f * std::max(DeltaSeconds, 0.0f));
            camComp.Camera.SetFOV(cur + (target - cur) * alpha);
        }
    }

    void ALeonTournamentCharacter::Tick(float DeltaSeconds) {
        ACharacter::Tick(DeltaSeconds);
        UpdateAimDownSights(DeltaSeconds);
        DodgeCooldownRemaining = std::max(0.0f, DodgeCooldownRemaining - DeltaSeconds);
        TickSpawnProtection(DeltaSeconds);
        DamageIndicatorRemaining = std::max(0.0f, DamageIndicatorRemaining - DeltaSeconds);
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
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(GetActiveWeaponId()));
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
                pc->NotifyConfirmedHit(hitConfirm == 2 || hitConfirm == 4, hitConfirm >= 3);
            if (damageFlash)
                pc->NotifyTookDamage();
        }
    }

    void ALeonTournamentCharacter::SerializeControlInput(std::vector<uint8_t>& OutBytes) const {
        FControlInput input = BuildLocalControlInput();
        bool bFire = FInput::IsMouseButtonPressed(Mouse::ButtonLeft);
        const FInputSettings& settings = FInputSettings::Get();
        if (settings.bEnableGamepad && FInput::IsGamepadConnected(settings.GamepadId) &&
            FInput::GetGamepadTrigger(GamepadAxis::RightTrigger, settings.GamepadId, settings.GamepadTriggerThreshold) >
                0.0f)
            bFire = true;
        input.SetAction(FControlInput::CustomBit0, bFire);
        input.Serialize(OutBytes);
    }

    void ALeonTournamentCharacter::ApplyControlInput(const uint8_t* InData, size_t InSize) {
        FControlInput input;
        if (!input.Deserialize(InData, InSize))
            return;
        ApplyControlSchema(input);
        ApplyLookRotation();
        if (bDeadFrozen)
            return;
        if (Combat)
            Combat->SetFireHeld(input.HasAction(FControlInput::CustomBit0));
    }

    bool ALeonTournamentCharacter::HandleServerRPC(uint16_t InFunctionId, const uint8_t* InData, size_t InSize) {
        (void)InData;
        (void)InSize;
        if (bDeadFrozen)
            return true;
        if (InFunctionId == kServerRpcReload) {
            if (Combat)
                Combat->RequestReload();
            return true;
        }
        return false;
    }

} // namespace Leon
