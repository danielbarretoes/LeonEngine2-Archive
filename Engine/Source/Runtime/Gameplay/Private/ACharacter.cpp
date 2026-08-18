#include "Gameplay/ACharacter.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/AController.hpp"
#include "Gameplay/APhysicsVolume.hpp"
#include "Gameplay/UAnimInstance.hpp"
#include "Gameplay/FGameplayDebugger.hpp"
#include "Renderer/FRenderingMath.hpp"
#include "Renderer/FDebugRenderer.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"
#include "Core/FWorldUnits.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Leon {

    ACharacter::ACharacter(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : APawn(InHandle, InWorld, InName) {
        SetClass("ACharacter");
    }

    void ACharacter::GetCapsuleAABB(glm::vec3& OutMin, glm::vec3& OutMax) const {
        // Actor location is CapsuleComponent (RootComponent) center — Unreal convention.
        const glm::vec3 pos = GetActorLocation();
        const float r = CapsuleRadius;
        const float halfH = GetCapsuleHalfHeight();
        OutMin = glm::vec3(pos.x - r, pos.y - halfH, pos.z - r);
        OutMax = glm::vec3(pos.x + r, pos.y + halfH, pos.z + r);
    }

    glm::vec3 ACharacter::GetPawnViewLocation() const {
        const float halfH = GetCapsuleHalfHeight();
        const float eye = bIsCrouched ? CrouchedEyeHeight : EyeHeight;
        return GetActorLocation() + glm::vec3(0.0f, eye - halfH, 0.0f);
    }

    void ACharacter::GetViewPoint(glm::vec3& OutLocation, glm::vec3& OutForward) const {
        // Must match UpdateCameraFromView — third-person spring arm is behind the pawn; aiming from
        // the eye alone diverges badly when looking down (crosshair vs shot end).
        OutForward = GetControlLookDirection();
        glm::vec3 right, up;
        StableViewBasis(OutForward, right, up);
        OutLocation = GetPawnViewLocation();
        if (bThirdPerson && SpringArm) {
            SpringArm->bDoCollisionTest = true;
            const_cast<USpringArmComponent&>(*SpringArm).UpdateDesiredArmLocation(OutLocation, OutForward, right, up);
            OutLocation = SpringArm->GetTargetLocation();
        }
    }

    glm::vec3 ACharacter::MoveBlocked(const glm::vec3& InWorldDelta) {
        if (!World || glm::dot(InWorldDelta, InWorldDelta) < 1e-12f)
            return glm::vec3(0.0f);

        // Flow: planar move against WorldStatic
        // 1. Sweep capsule radius along remaining delta (ignore walkable floors/ceilings)
        // 2. Stop before impact (skin width); slide leftover along the wall
        // 3. If already overlapping (Distance≈0), push out along the normal then slide
        constexpr float kSkin = 0.025f;
        glm::vec3 remaining = InWorldDelta;
        remaining.y = 0.0f;
        glm::vec3 applied(0.0f);
        glm::vec3 pos = GetActorLocation();
        const float bottomOffset = -(GetCapsuleHalfHeight() - CapsuleRadius);

        auto sweepCapsule = [&](const glm::vec3& InStart, const glm::vec3& InEnd, std::vector<FHitResult>& OutHits) {
            OutHits.clear();
            const glm::vec3 offsets[2] = {{0.0f, 0.0f, 0.0f}, {0.0f, bottomOffset, 0.0f}};
            for (const glm::vec3& off : offsets) {
                std::vector<FHitResult> slice;
                World->SweepMultiByChannel(InStart + off, InEnd + off, CapsuleRadius, ECollisionChannel::WorldStatic,
                                           this, slice);
                OutHits.insert(OutHits.end(), slice.begin(), slice.end());
            }
            std::sort(OutHits.begin(), OutHits.end(),
                      [](const FHitResult& a, const FHitResult& b) { return a.Distance < b.Distance; });
            return static_cast<int32_t>(OutHits.size());
        };

        for (int iter = 0; iter < 3; ++iter) {
            const float remainLen = glm::length(remaining);
            if (remainLen < 1e-7f)
                break;

            std::vector<FHitResult> hits;
            const glm::vec3 end = pos + remaining;
            if (sweepCapsule(pos, end, hits) <= 0) {
                applied += remaining;
                pos += remaining;
                remaining = glm::vec3(0.0f);
                break;
            }

            bool bHitWall = false;
            for (const auto& candidate : hits) {
                if (!candidate.bBlockingHit)
                    continue;
                // Horizontal move: walkable floors/ceilings must not consume the step.
                if (std::abs(candidate.Normal.y) > 0.7f)
                    continue;

                glm::vec3 n = candidate.Normal;
                n.y = 0.0f;
                if (glm::dot(n, n) < 1e-8f)
                    continue;
                n = glm::normalize(n);

                if (candidate.Distance <= kSkin) {
                    // Depenetrate so the next frame can slide instead of locking in place.
                    const glm::vec3 push = n * kSkin;
                    applied += push;
                    pos += push;
                } else {
                    const float safe = std::clamp((candidate.Distance - kSkin) / remainLen, 0.0f, 1.0f);
                    const glm::vec3 step = remaining * safe;
                    applied += step;
                    pos += step;
                    remaining -= step;
                }

                const float into = glm::dot(remaining, n);
                if (into < 0.0f)
                    remaining -= n * into;
                bHitWall = true;
                break;
            }

            if (!bHitWall) {
                applied += remaining;
                pos += remaining;
                remaining = glm::vec3(0.0f);
                break;
            }
        }

        auto& transform = GetTransform();
        transform.Translation.x = pos.x;
        transform.Translation.z = pos.z;
        return applied;
    }

    void ACharacter::SnapToFloor() {
        auto& transform = GetTransform();
        float standY = FloorZ;
        const float halfH = GetCapsuleHalfHeight();
        const float feetY = transform.Translation.y - halfH;
        if (World) {
            glm::vec3 probe = transform.Translation;
            probe.y = feetY + 0.05f;
            // Thin foot pad — tall walls must not register as supporting floors.
            glm::vec3 half(CapsuleRadius * 0.9f, 0.12f, CapsuleRadius * 0.9f);
            std::vector<FHitResult> hits;
            if (World->OverlapMultiByChannel(probe, half, ECollisionChannel::WorldStatic, this, hits) > 0) {
                for (const auto& floorHit : hits) {
                    if (!floorHit.Actor || floorHit.Normal.y < 0.7f)
                        continue;

                    float topY = FloorZ;
                    if (floorHit.Actor->HasComponent<FBoxCollisionComponent>()) {
                        const auto& box = floorHit.Actor->GetComponent<FBoxCollisionComponent>();
                        const glm::vec3 scale = floorHit.Actor->GetActorScale();
                        topY = floorHit.Actor->GetActorLocation().y + box.LocalMax.y * scale.y;
                    } else if (auto boxComp = floorHit.Actor->FindActorComponent<UBoxComponent>()) {
                        const glm::vec3 scale = floorHit.Actor->GetActorScale();
                        topY = boxComp->GetComponentLocation().y + boxComp->GetBoxExtent().y * scale.y;
                    } else if (floorHit.Actor->HasComponent<FStaticMeshComponent>() &&
                               floorHit.Actor->GetComponent<FStaticMeshComponent>().StaticMesh) {
                        const auto& sm = *floorHit.Actor->GetComponent<FStaticMeshComponent>().StaticMesh;
                        const glm::vec3 s = floorHit.Actor->GetActorScale();
                        topY = floorHit.Actor->GetActorLocation().y + sm.GetBoundsMax().y * s.y;
                    } else if (floorHit.Actor->HasComponent<FMeshComponent>()) {
                        const auto& mesh = floorHit.Actor->GetComponent<FMeshComponent>();
                        float top = mesh.MeshSize * 0.5f * floorHit.Actor->GetActorScale().y;
                        if (mesh.MeshType == "Plane")
                            top = 0.05f;
                        topY = floorHit.Actor->GetActorLocation().y + top;
                    } else {
                        continue;
                    }

                    // Only surfaces within a small step of the feet (ignore wall tops while standing beside them).
                    if (topY > feetY + 0.45f || topY < feetY - 0.35f)
                        continue;
                    standY = std::max(standY, topY);
                }
            }
        }
        transform.Translation.y = standY + halfH;
        if (CharacterMovement && CharacterMovement->IsMovingOnGround()) {
            auto v = CharacterMovement->GetVelocity();
            v.y = 0.0f;
            CharacterMovement->SetVelocity(v);
        }
    }

    void ACharacter::PostInitializeComponents() {
        const float halfHeight = GetCapsuleHalfHeight();
        // Flow: Unreal ACharacter defaults
        // 1. CapsuleComponent = RootComponent (collision + movement origin)
        // 2. Mesh SetupAttachment(Capsule) — visual only, offset to feet
        // 3. CharacterMovement drives the capsule/actor transform
        if (!CapsuleComponent) {
            CapsuleComponent = AddActorComponent<UCapsuleComponent>("CapsuleComponent");
            CapsuleComponent->SetCapsuleSize(CapsuleRadius, halfHeight);
            CapsuleComponent->SetCollisionObjectType(ECollisionChannel::Pawn);
            CapsuleComponent->SetCollisionProfileName("Pawn");
        }
        SetRootComponent(CapsuleComponent.get());

        if (!CharacterMovement)
            CharacterMovement = AddActorComponent<UCharacterMovementComponent>("CharacterMovement");
        if (CharacterMovement)
            CharacterMovement->SetFloorZ(FloorZ);

        if (!HasComponent<FCameraComponent>()) {
            FPerspectiveCamera camera(95.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            AddComponent<FCameraComponent>(camera);
        }
        if (!SpringArm)
            SpringArm = AddActorComponent<USpringArmComponent>("SpringArm");
        if (!Mesh) {
            Mesh = AddActorComponent<USkeletalMeshComponent>("CharacterMesh");
            Mesh->SetupAttachment(CapsuleComponent.get());
            Mesh->SetRelativeLocation(glm::vec3(0.0f, -halfHeight, 0.0f));
        }
        if (!HasComponent<FSkinnedMeshRenderState>())
            AddComponent<FSkinnedMeshRenderState>();
        auto& t = GetTransform();
        t.Translation.y = FloorZ + halfHeight;
        LastLocation = t.Translation;
        bHasLastLocation = true;
    }

    void ACharacter::Tick(float DeltaSeconds) {
        if (GetLocalRole() == ENetRole::SimulatedProxy)
            return;
        FlushPendingControlInput(DeltaSeconds);
        if (IsLocallyControlled())
            SetupPlayerInputComponent(DeltaSeconds);
        if (ShouldApplyControlYawToActor())
            ApplyYawOnlyActorRotation();
        if (CharacterMovement)
            CharacterMovement->PerformMovement(DeltaSeconds);
        UpdatePhysicsVolume();
        UpdateAnimFromMovement(DeltaSeconds);
        UpdateCameraFromView();
        UpdateMeshVisibility();
        DrawCharacterDebug();
    }

    float ACharacter::GetControlYaw() const {
        if (AController* c = GetController())
            return c->GetControlRotation().y;
        return Yaw;
    }

    float ACharacter::GetControlPitch() const {
        if (AController* c = GetController())
            return c->GetControlRotation().x;
        return Pitch;
    }

    void ACharacter::SetControlYaw(float InYaw) {
        Yaw = InYaw;
        if (AController* c = GetController())
            c->SetControlRotation({GetControlPitch(), InYaw, 0.0f});
    }

    void ACharacter::SetControlPitch(float InPitch) {
        Pitch = std::clamp(InPitch, -89.0f, 89.0f);
        if (AController* c = GetController())
            c->SetControlRotation({Pitch, GetControlYaw(), 0.0f});
    }

    void ACharacter::SetControlRotation(const glm::vec3& InRotation) {
        Pitch = std::clamp(InRotation.x, -89.0f, 89.0f);
        Yaw = InRotation.y;
        if (AController* c = GetController())
            c->SetControlRotation({Pitch, Yaw, 0.0f});
    }

    glm::vec3 ACharacter::GetControlPlanarForward() const {
        return FWorldUnits::PlanarForwardFromYaw(GetControlYaw());
    }

    glm::vec3 ACharacter::GetControlLookDirection() const {
        return FWorldUnits::LookDirection(GetControlPitch(), GetControlYaw());
    }

    void ACharacter::ApplyYawOnlyActorRotation() {
        // Control yaw uses the camera look convention (yaw -90 => engine -Z).
        // Actor euler {0, Y, 0} is a pure Y rotation of local -Z. Identity faces -Z,
        // so actor yaw = -controlYaw - 90. Pitch and roll stay 0 so look-up cannot tilt the body.
        GetTransform().Rotation = glm::vec3(0.0f, -GetControlYaw() - 90.0f, 0.0f);
    }

    void ACharacter::ApplyLookInput(float DeltaSeconds, bool bRequireHeldButton) {
        const FInputSettings& input = FInputSettings::Get();
        bool bRotated = false;

        const bool bLookHeld = !bRequireHeldButton || FInput::IsMouseButtonPressed(Mouse::ButtonRight);
        if (input.bEnableMouseLook && bLookHeld) {
            auto [mx, my] = FInput::GetMousePosition();
            if (bFirstMouse) {
                LastMousePos = {mx, my};
                bFirstMouse = false;
            }
            Yaw += (mx - LastMousePos.x) * LookSensitivity;
            Pitch += (LastMousePos.y - my) * LookSensitivity;
            LastMousePos = {mx, my};
            bRotated = true;
        } else {
            bFirstMouse = true;
        }

        // Xbox / gamepad right stick — always active in game (no hold required).
        if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) && DeltaSeconds > 0.0f) {
            auto [rx, ry] = FInput::GetGamepadRightStick(input.GamepadId, input.GamepadDeadzone);
            if (std::abs(rx) > 1e-4f || std::abs(ry) > 1e-4f) {
                Yaw += rx * input.GamepadLookSpeed * DeltaSeconds;
                Pitch += -ry * input.GamepadLookSpeed * DeltaSeconds;
                bRotated = true;
            }
        }

        if (bRotated) {
            Pitch = std::clamp(Pitch, -89.0f, 89.0f);
            SetControlRotation({Pitch, Yaw, 0.0f});
        }
    }

    void ACharacter::ApplyMoveInput(float DeltaSeconds, float InSpeedScale) {
        const FInputSettings& input = FInputSettings::Get();
        glm::vec3 forward = GetControlPlanarForward();
        glm::vec3 right(-forward.z, 0.0f, forward.x);

        float speedScale = std::max(InSpeedScale, 0.0f);
        if (FInput::IsKeyPressed(input.SprintKey))
            speedScale = std::max(speedScale, SprintMultiplier);
        if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
            FInput::IsGamepadButtonPressed(GamepadButton::LeftBumper, input.GamepadId))
            speedScale = std::max(speedScale, SprintMultiplier);

        const float speed = GetMoveSpeed() * speedScale;
        glm::vec3 wish(0.0f);
        if (FInput::IsKeyPressed(input.MoveForwardKey))
            wish += forward;
        if (FInput::IsKeyPressed(input.MoveBackwardKey))
            wish -= forward;
        if (FInput::IsKeyPressed(input.MoveLeftKey))
            wish -= right;
        if (FInput::IsKeyPressed(input.MoveRightKey))
            wish += right;

        if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId)) {
            auto [lx, ly] = FInput::GetGamepadLeftStick(input.GamepadId, input.GamepadDeadzone);
            // GLFW stick Y: up is negative → forward.
            wish += forward * (-ly) + right * lx;
            if (FInput::IsGamepadButtonPressed(GamepadButton::DPadUp, input.GamepadId))
                wish += forward;
            if (FInput::IsGamepadButtonPressed(GamepadButton::DPadDown, input.GamepadId))
                wish -= forward;
            if (FInput::IsGamepadButtonPressed(GamepadButton::DPadLeft, input.GamepadId))
                wish -= right;
            if (FInput::IsGamepadButtonPressed(GamepadButton::DPadRight, input.GamepadId))
                wish += right;
        }

        if (glm::length(wish) > 1e-4f && CharacterMovement)
            CharacterMovement->AddInputVector(glm::normalize(wish) * speed);
        (void)DeltaSeconds;

        bool bJump = FInput::IsKeyPressed(input.JumpKey);
        if (input.bEnableGamepad && FInput::IsGamepadConnected(input.GamepadId) &&
            FInput::IsGamepadButtonPressed(GamepadButton::A, input.GamepadId))
            bJump = true;
        // Rising edge only — holding Space must not consume double-jump on the same press.
        if (bJump && !bJumpWasDown)
            Jump();
        if (!bJump && bJumpWasDown)
            StopJumping();
        bJumpWasDown = bJump;
    }

    float ACharacter::ComputeLocomotionDirection(const glm::vec3& InPlanarMove, const glm::vec3& InPlanarForward) {
        glm::vec3 move = InPlanarMove;
        move.y = 0.0f;
        glm::vec3 fwd = InPlanarForward;
        fwd.y = 0.0f;
        if (glm::length(move) < 1e-5f || glm::length(fwd) < 1e-5f)
            return 0.0f;
        move = glm::normalize(move);
        fwd = glm::normalize(fwd);
        glm::vec3 right(-fwd.z, 0.0f, fwd.x);
        return glm::degrees(std::atan2(glm::dot(move, right), glm::dot(move, fwd)));
    }

    void ACharacter::ResetMovementForRespawn() {
        bMeshHiddenInGame = false;
        if (CharacterMovement)
            CharacterMovement->ResetForRespawn();
        ApplyYawOnlyActorRotation();
        LastLocation = GetActorLocation();
        LastMoveDir = GetControlPlanarForward();
        bHasLastLocation = true;
        AnimRepState.Speed = 0.0f;
        AnimRepState.Direction = 0.0f;
        AnimRepState.SetFlag(FAnimRepState::FlagInAir, false);
        StopRagdoll();
        if (bIsCrouched)
            UnCrouch();
        UpdateMeshVisibility();
    }

    void ACharacter::SetThirdPerson(bool bEnabled) {
        bThirdPerson = bEnabled;
        UpdateMeshVisibility();
    }

    void ACharacter::SetMeshHiddenInGame(bool bHidden) {
        bMeshHiddenInGame = bHidden;
        UpdateMeshVisibility();
    }

    void ACharacter::UpdateMeshVisibility() {
        if (!Mesh)
            return;
        const bool bHideBody = bMeshHiddenInGame || (!bThirdPerson && IsLocallyControlled());
        Mesh->SetHiddenInGame(bHideBody);
    }

    void ACharacter::AddMovementInput(const glm::vec3& InWorldDirection, float InScale) {
        if (!CharacterMovement || glm::length(InWorldDirection) < 1e-5f)
            return;
        CharacterMovement->AddInputVector(glm::normalize(InWorldDirection) * InScale);
    }

    void ACharacter::SetupPlayerInputComponent(float DeltaSeconds) {
        if (APlayerController* pc = dynamic_cast<APlayerController*>(GetController())) {
            if (!pc->IsGameInputAllowed()) {
                bFirstMouse = true;
                return;
            }
        }

        const FInputSettings& input = FInputSettings::Get();
        ApplyLookInput(DeltaSeconds, true);
        float speedScale = 1.0f;
        if (FInput::IsKeyPressed(input.SprintKey))
            speedScale = SprintMultiplier;
        ApplyMoveInput(DeltaSeconds, speedScale);
    }

    FControlInput ACharacter::BuildLocalControlInput() const {
        FControlInput input = FControlInput::SampleFromHardware();
        input.LookYaw = GetControlYaw();
        input.LookPitch = GetControlPitch();
        return input;
    }

    void ACharacter::SerializeControlInput(std::vector<uint8_t>& OutBytes) const {
        BuildLocalControlInput().Serialize(OutBytes);
    }

    void ACharacter::ApplyControlInput(const uint8_t* InData, size_t InSize) {
        FControlInput input;
        if (!input.Deserialize(InData, InSize))
            return;
        ApplyControlSchema(input);
    }

    void ACharacter::ApplyControlSchema(const FControlInput& InInput) {
        SetControlYaw(InInput.LookYaw);
        SetControlPitch(InInput.LookPitch);
        PendingControlInput = InInput;
        bHasPendingControlInput = true;
    }

    void ACharacter::FlushPendingControlInput(float DeltaSeconds) {
        if (!bHasPendingControlInput)
            return;
        bHasPendingControlInput = false;
        (void)DeltaSeconds;
        if (!CanApplyControlMove())
            return;

        glm::vec3 forward = GetControlPlanarForward();
        glm::vec3 right(-forward.z, 0.0f, forward.x);
        glm::vec3 wish = forward * PendingControlInput.MoveY + right * PendingControlInput.MoveX;
        float speed = GetMoveSpeed();
        if (bIsCrouched && CharacterMovement)
            speed = CharacterMovement->GetMaxWalkSpeedCrouched();
        else if (PendingControlInput.HasAction(FControlInput::SprintBit))
            speed *= SprintMultiplier;
        if (glm::length(wish) > 1e-4f && CharacterMovement)
            CharacterMovement->AddInputVector(glm::normalize(wish) * speed);

        const bool bJump = PendingControlInput.HasAction(FControlInput::JumpBit);
        if (bJump && !bJumpWasDown)
            Jump();
        if (!bJump && bJumpWasDown)
            StopJumping();
        bJumpWasDown = bJump;

        const bool bCrouchHeld = PendingControlInput.HasAction(FControlInput::CrouchBit);
        if (bCrouchHeld)
            Crouch();
        else
            UnCrouch();
    }

    void ACharacter::UpdateCameraFromView() {
        glm::vec3 camPos, look;
        GetViewPoint(camPos, look);
        (void)look;
        if (HasComponent<FCameraComponent>()) {
            auto& camComp = GetComponent<FCameraComponent>();
            camComp.Camera.SetPosition(camPos);
            camComp.Camera.SetRotation(GetControlPitch(), GetControlYaw());
        }
    }

    void ACharacter::UpdateAnimFromMovement(float DeltaSeconds) {
        (void)DeltaSeconds;
        glm::vec3 vel = CharacterMovement ? CharacterMovement->GetVelocity() : glm::vec3(0.0f);
        glm::vec3 planar(vel.x, 0.0f, vel.z);
        float speed = glm::length(planar);
        if (speed > 1e-3f)
            LastMoveDir = planar / speed;

        glm::vec3 forward = GetControlPlanarForward();
        float direction = ComputeLocomotionDirection(LastMoveDir, forward);

        AnimRepState.Speed = speed * 100.0f;
        AnimRepState.Direction = direction;
        AnimRepState.AimPitch = GetControlPitch();
        AnimRepState.SetFlag(FAnimRepState::FlagInAir, IsFalling());
        AnimRepState.SetFlag(FAnimRepState::FlagCrouched, bIsCrouched);
    }

    void ACharacter::Jump() {
        if (!CanJump())
            return;
        if (CharacterMovement)
            CharacterMovement->Jump();
    }

    void ACharacter::StopJumping() {
        if (CharacterMovement)
            CharacterMovement->StopJumping();
    }

    bool ACharacter::CanJump() const {
        return CharacterMovement && CharacterMovement->CanJump() && !bIsCrouched && !bIsRagdoll;
    }

    void ACharacter::Crouch() {
        if (bIsCrouched || bIsRagdoll)
            return;
        const float oldHalf = GetCapsuleHalfHeight();
        bIsCrouched = true;
        const float newHalf = GetCapsuleHalfHeight();
        auto& transform = GetTransform();
        transform.Translation.y += (newHalf - oldHalf);
        if (CapsuleComponent)
            CapsuleComponent->SetCapsuleSize(CapsuleRadius, newHalf);
    }

    void ACharacter::UnCrouch() {
        if (!bIsCrouched || bIsRagdoll)
            return;
        const float crouchedHalf = GetCapsuleHalfHeight();
        const float standingHalf = (EyeHeight + 0.1f) * 0.5f;
        glm::vec3 standingCenter = GetActorLocation();
        standingCenter.y += (standingHalf - crouchedHalf);
        if (World &&
            World->OverlapAnyTestByChannel(standingCenter, glm::vec3(CapsuleRadius, standingHalf, CapsuleRadius),
                                           ECollisionChannel::WorldStatic, this))
            return;
        bIsCrouched = false;
        auto& transform = GetTransform();
        transform.Translation.y += (standingHalf - crouchedHalf);
        if (CapsuleComponent)
            CapsuleComponent->SetCapsuleSize(CapsuleRadius, standingHalf);
    }

    void ACharacter::EnableRagdoll(const glm::vec3& InImpulse) {
        if (bIsRagdoll)
            return;
        if (CharacterMovement)
            CharacterMovement->StopMovementImmediately();
        if (Mesh && Mesh->TryEnableRagdoll(InImpulse)) {
            bIsRagdoll = true;
            bMeshRagdoll = true;
            // Capsule stays kinematic so CharacterMovement stays off; mesh bones drive the corpse.
            if (CapsuleComponent)
                CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            return;
        }
        if (CapsuleComponent) {
            // Recreate as a Dynamic body so Mass / gravity / damping from EnableRagdoll stick
            // (CreateRigidBody copies Info once; mutating component fields alone was a no-op).
            CapsuleComponent->SetMass(70.0f);
            CapsuleComponent->SetLinearDamping(0.55f);
            CapsuleComponent->SetEnableGravity(true);
            CapsuleComponent->SetSimulatePhysics(true);
            CapsuleComponent->RecreatePhysicsBody();
            CapsuleComponent->SyncPhysicsTransform();
            CapsuleComponent->AddImpulse(InImpulse);
            if (auto* body = CapsuleComponent->GetPhysicsBody()) {
                // Tip the corpse so the frozen mesh reads as a fall, not a standing statue.
                body->SetAngularVelocity({1.8f, 0.0f, 0.6f});
            }
        }
        bIsRagdoll = true;
        bMeshRagdoll = false;
    }

    void ACharacter::StopRagdoll() {
        if (!bIsRagdoll)
            return;
        if (bMeshRagdoll && Mesh)
            Mesh->StopRagdoll();
        if (CapsuleComponent) {
            CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            CapsuleComponent->SetCollisionProfileName("Pawn");
            CapsuleComponent->SetSimulatePhysics(false);
            CapsuleComponent->SetLinearDamping(0.01f);
            CapsuleComponent->RecreatePhysicsBody();
            CapsuleComponent->SyncPhysicsTransform();
            if (auto* body = CapsuleComponent->GetPhysicsBody())
                body->SetLinearVelocity(glm::vec3(0.0f));
        }
        bIsRagdoll = false;
        bMeshRagdoll = false;
    }

    void ACharacter::Landed(const FHitResult& InHit) {
        (void)InHit;
    }

    void ACharacter::OnMovementModeChanged(EMovementMode InPrevMode, EMovementMode InNewMode) {
        (void)InPrevMode;
        (void)InNewMode;
    }

    bool ACharacter::IsFalling() const {
        return CharacterMovement && CharacterMovement->IsFalling();
    }

    bool ACharacter::IsMovingOnGround() const {
        return CharacterMovement && CharacterMovement->IsMovingOnGround();
    }

    float ACharacter::GetVerticalVelocity() const {
        return CharacterMovement ? CharacterMovement->GetVelocity().y : 0.0f;
    }

    void ACharacter::LaunchCharacter(const glm::vec3& InVelocity) {
        if (!CharacterMovement)
            return;
        glm::vec3 v = CharacterMovement->GetVelocity() + InVelocity;
        if (InVelocity.y > 0.0f)
            v.y = std::max(v.y, InVelocity.y);
        CharacterMovement->SetVelocity(v);
        if (v.y > 0.5f || glm::length(glm::vec3(v.x, 0.0f, v.z)) > 0.5f)
            CharacterMovement->SetMovementMode(EMovementMode::Falling);
    }

    void ACharacter::UpdateAnimInstance(UAnimInstance& InAnim) const {
        InAnim.ApplyRepState(AnimRepState);
        InAnim.SetFloat("VerticalSpeed", GetVerticalVelocity());
        InAnim.SetBool("bIsFalling", IsFalling());
    }

    void ACharacter::UpdatePhysicsVolume() {
        PhysicsVolume = nullptr;
        if (!World)
            return;
        int32_t bestPri = -1;
        for (const auto& actor : World->GetAllActors()) {
            auto* volume = dynamic_cast<APhysicsVolume*>(actor.get());
            if (!volume || !volume->EncompassesPoint(GetActorLocation()))
                continue;
            if (volume->Priority >= bestPri) {
                bestPri = volume->Priority;
                PhysicsVolume = volume;
            }
        }
    }

    void ACharacter::DrawCharacterDebug() const {
        if (!FGameplayDebugger::ShowCharacter())
            return;

        const glm::vec3 origin = GetActorLocation();
        const glm::vec3 actorFwd = GetActorForwardVector();
        glm::vec3 meshFwd = actorFwd;
        if (Mesh) {
            glm::mat4 rel = glm::toMat4(glm::quat(glm::radians(Mesh->GetRelativeRotation())));
            if (Mesh->GetSkeletalMesh() &&
                Mesh->GetSkeletalMesh()->GetAssetForwardAxis() == EAssetForwardAxis::SourcePosZ)
                rel = glm::rotate(rel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec3 local = glm::vec3(rel * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f));
            meshFwd = glm::normalize(glm::vec3(GetTransform().GetTransform() * glm::vec4(local, 0.0f)));
        }
        glm::vec3 camFwd = GetControlLookDirection();
        FDebugRenderer::DrawDebugLine(origin, origin + actorFwd * 1.4f, glm::vec4(0.2f, 1.0f, 0.3f, 1.0f));
        FDebugRenderer::DrawDebugLine(origin + glm::vec3(0, 0.15f, 0), origin + glm::vec3(0, 0.15f, 0) + meshFwd * 1.4f,
                                      glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
        FDebugRenderer::DrawDebugLine(origin + glm::vec3(0, 0.3f, 0), origin + glm::vec3(0, 0.3f, 0) + camFwd * 1.6f,
                                      glm::vec4(0.3f, 0.7f, 1.0f, 1.0f));

        char line[160];
        std::snprintf(line, sizeof(line),
                      "Authority:%s Local:%s Mode:%s V=%.1f Falling:%s Ground:%s pitch=%.1f yaw=%.1f actorP=%.1f",
                      HasAuthority() ? "Server" : "Client", IsLocallyControlled() ? "true" : "false",
                      CharacterMovement ? MovementModeName(CharacterMovement->GetMovementMode()) : "None",
                      glm::length(CharacterMovement ? CharacterMovement->GetVelocity() : glm::vec3(0.0f)),
                      IsFalling() ? "true" : "false", IsMovingOnGround() ? "true" : "false", GetControlPitch(),
                      GetControlYaw(), GetActorRotation().x);
        PrintString(line, 0.12f, glm::vec4(0.85f, 1.0f, 0.7f, 1.0f), 9300);
    }

} // namespace Leon
