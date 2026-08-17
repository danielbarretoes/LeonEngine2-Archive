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
        const glm::vec3 pos = GetActorLocation();
        const float r = CapsuleRadius;
        OutMin = glm::vec3(pos.x - r, pos.y - EyeHeight, pos.z - r);
        OutMax = glm::vec3(pos.x + r, pos.y + 0.2f, pos.z + r);
    }

    glm::vec3 ACharacter::MoveBlocked(const glm::vec3& InWorldDelta) {
        if (!World || glm::dot(InWorldDelta, InWorldDelta) < 1e-12f)
            return glm::vec3(0.0f);

        glm::vec3 minB, maxB;
        GetCapsuleAABB(minB, maxB);
        glm::vec3 delta = InWorldDelta;
        delta.y = 0.0f;
        glm::vec3 start = (minB + maxB) * 0.5f;
        glm::vec3 end = start + delta;
        float radius = CapsuleRadius;

        glm::vec3 applied = delta;
        std::vector<FHitResult> hits;
        if (World->SweepMultiByChannel(start, end, radius, ECollisionChannel::WorldStatic, this, hits) > 0) {
            for (const auto& candidate : hits) {
                if (!candidate.bBlockingHit)
                    continue;
                // Horizontal move: walkable floors/ceilings must not consume the step.
                if (std::abs(candidate.Normal.y) > 0.7f)
                    continue;
                float len = glm::length(delta);
                float safe = len > 1e-6f ? std::max(0.0f, candidate.Distance - 0.01f) / len : 0.0f;
                applied = delta * std::min(safe, 1.0f);
                break;
            }
        }

        auto& transform = GetTransform();
        transform.Translation += applied;
        return applied;
    }

    void ACharacter::SnapToFloor() {
        auto& transform = GetTransform();
        float standY = FloorZ;
        if (World) {
            glm::vec3 feetMin, feetMax;
            GetCapsuleAABB(feetMin, feetMax);
            glm::vec3 probe = transform.Translation;
            probe.y = (FloorZ + transform.Translation.y - EyeHeight) * 0.5f;
            glm::vec3 half(CapsuleRadius, std::max(0.2f, (transform.Translation.y - EyeHeight - FloorZ) * 0.5f + 0.1f),
                           CapsuleRadius);
            std::vector<FHitResult> hits;
            if (World->OverlapMultiByChannel(probe, half, ECollisionChannel::WorldStatic, this, hits) > 0) {
                for (const auto& floorHit : hits) {
                    if (!floorHit.Actor)
                        continue;
                    if (floorHit.Actor->HasComponent<FBoxCollisionComponent>()) {
                        const auto& box = floorHit.Actor->GetComponent<FBoxCollisionComponent>();
                        glm::vec3 wmax =
                            floorHit.Actor->GetActorLocation() + box.LocalMax * floorHit.Actor->GetActorScale();
                        standY = std::max(standY, wmax.y);
                    } else if (auto boxComp = floorHit.Actor->FindActorComponent<UBoxComponent>()) {
                        glm::vec3 top = boxComp->GetComponentLocation();
                        top.y += boxComp->GetBoxExtent().y;
                        standY = std::max(standY, top.y);
                    } else if (floorHit.Actor->HasComponent<UStaticMeshComponent>() &&
                               floorHit.Actor->GetComponent<UStaticMeshComponent>().StaticMesh) {
                        const auto& sm = *floorHit.Actor->GetComponent<UStaticMeshComponent>().StaticMesh;
                        glm::vec3 s = floorHit.Actor->GetActorScale();
                        standY = std::max(standY, floorHit.Actor->GetActorLocation().y + sm.GetBoundsMax().y * s.y);
                    } else if (floorHit.Actor->HasComponent<FMeshComponent>()) {
                        const auto& mesh = floorHit.Actor->GetComponent<FMeshComponent>();
                        float top = mesh.MeshSize * 0.5f;
                        if (mesh.MeshType == "Plane")
                            top = 0.05f;
                        standY = std::max(standY, floorHit.Actor->GetActorLocation().y + top);
                    }
                }
            }
        }
        transform.Translation.y = standY + EyeHeight;
        if (CharacterMovement && CharacterMovement->IsMovingOnGround()) {
            auto v = CharacterMovement->GetVelocity();
            v.y = 0.0f;
            CharacterMovement->SetVelocity(v);
        }
    }

    void ACharacter::PostInitializeComponents() {
        const float halfHeight = GetCapsuleHeight() * 0.5f;
        if (!CapsuleComponent) {
            CapsuleComponent = AddActorComponent<UCapsuleComponent>("CapsuleComponent");
            CapsuleComponent->SetCapsuleSize(CapsuleRadius, halfHeight);
            CapsuleComponent->SetRelativeLocation(glm::vec3(0.0f, -EyeHeight + halfHeight, 0.0f));
            CapsuleComponent->SetCollisionObjectType(ECollisionChannel::Pawn);
            CapsuleComponent->SetCollisionProfileName("Pawn");
        }
        if (!CharacterMovement)
            CharacterMovement = AddActorComponent<UCharacterMovementComponent>("CharacterMovement");
        if (CharacterMovement)
            CharacterMovement->SetFloorZ(FloorZ);

        if (!HasComponent<UCameraComponent>()) {
            FPerspectiveCamera camera(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            AddComponent<UCameraComponent>(camera);
        }
        if (!SpringArm)
            SpringArm = AddActorComponent<USpringArmComponent>("SpringArm");
        if (!Mesh) {
            Mesh = AddActorComponent<USkeletalMeshComponent>("CharacterMesh");
            Mesh->SetRelativeLocation(glm::vec3(0.0f, -EyeHeight, 0.0f));
        }
        if (!HasComponent<FSkeletalMeshComponent>())
            AddComponent<FSkeletalMeshComponent>();
        auto& t = GetTransform();
        t.Translation.y = FloorZ + EyeHeight;
        LastLocation = t.Translation;
        bHasLastLocation = true;
    }

    void ACharacter::Tick(float DeltaSeconds) {
        if (GetLocalRole() == ENetRole::SimulatedProxy)
            return;
        if (IsLocallyControlled())
            SetupPlayerInputComponent(DeltaSeconds);
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
        (void)DeltaSeconds;
        const FInputSettings& input = FInputSettings::Get();
        const bool bLookHeld = !bRequireHeldButton || FInput::IsMouseButtonPressed(Mouse::ButtonRight);
        if (input.bEnableMouseLook && bLookHeld) {
            auto [mx, my] = FInput::GetMousePosition();
            if (bFirstMouse) {
                LastMousePos = {mx, my};
                bFirstMouse = false;
            }
            Yaw += (mx - LastMousePos.x) * LookSensitivity;
            Pitch += (LastMousePos.y - my) * LookSensitivity;
            Pitch = std::clamp(Pitch, -89.0f, 89.0f);
            LastMousePos = {mx, my};
            SetControlRotation({Pitch, Yaw, 0.0f});
        } else {
            bFirstMouse = true;
        }
    }

    void ACharacter::ApplyMoveInput(float DeltaSeconds, float InSpeedScale) {
        const FInputSettings& input = FInputSettings::Get();
        glm::vec3 forward = GetControlPlanarForward();
        glm::vec3 right(-forward.z, 0.0f, forward.x);

        const float speed = GetMoveSpeed() * std::max(InSpeedScale, 0.0f);
        glm::vec3 wish(0.0f);
        if (FInput::IsKeyPressed(input.MoveForwardKey))
            wish += forward;
        if (FInput::IsKeyPressed(input.MoveBackwardKey))
            wish -= forward;
        if (FInput::IsKeyPressed(input.MoveLeftKey))
            wish -= right;
        if (FInput::IsKeyPressed(input.MoveRightKey))
            wish += right;
        if (glm::length(wish) > 1e-4f && CharacterMovement)
            CharacterMovement->AddInputVector(glm::normalize(wish) * speed);
        (void)DeltaSeconds;
        if (FInput::IsKeyPressed(input.JumpKey))
            Jump();
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

    void ACharacter::UpdateCameraFromView() {
        auto& transform = GetTransform();
        glm::vec3 look = GetControlLookDirection();
        glm::vec3 right, up;
        StableViewBasis(look, right, up);

        glm::vec3 camPos = transform.Translation;
        if (bThirdPerson && SpringArm) {
            SpringArm->bDoCollisionTest = true;
            SpringArm->UpdateDesiredArmLocation(transform.Translation, look, right, up);
            camPos = SpringArm->GetTargetLocation();
        }

        if (HasComponent<UCameraComponent>()) {
            auto& camComp = GetComponent<UCameraComponent>();
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
        AnimRepState.SetFlag(FAnimRepState::FlagCrouched, false);
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
        return CharacterMovement && CharacterMovement->CanJump();
    }

    void ACharacter::Landed(const FHitResult& InHit) { (void)InHit; }

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

        if (FGameplayDebugger::ShowPhysics() && CapsuleComponent) {
            glm::vec3 minB, maxB;
            GetCapsuleAABB(minB, maxB);
            FDebugRenderer::DrawDebugCapsule((minB + maxB) * 0.5f, CapsuleRadius, (maxB.y - minB.y) * 0.5f,
                                             glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
        }
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
