#include "Gameplay/ACharacter.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Renderer/FRenderingMath.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"

#include <algorithm>
#include <cmath>

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

        UWorld::FHitResult hit;
        glm::vec3 applied = delta;
        if (World->SweepAABB(minB, maxB, delta, this, hit) && hit.bBlockingHit) {
            float safe = glm::length(delta) > 1e-6f ? std::max(0.0f, hit.Distance - 0.01f) / glm::length(delta) : 0.0f;
            applied = delta * std::min(safe, 1.0f);
        }

        auto& transform = GetTransform();
        transform.Translation += applied;
        SnapToFloor();
        return applied;
    }

    void ACharacter::SnapToFloor() {
        auto& transform = GetTransform();
        float standY = FloorZ;
        if (World) {
            glm::vec3 feetMin, feetMax;
            GetCapsuleAABB(feetMin, feetMax);
            glm::vec3 probeMin = feetMin;
            glm::vec3 probeMax = feetMax;
            probeMin.y = FloorZ - 4.0f;
            probeMax.y = transform.Translation.y - EyeHeight + 0.05f;
            UWorld::FHitResult floorHit;
            if (World->OverlapAABB(probeMin, probeMax, this, floorHit) && floorHit.Actor) {
                if (floorHit.Actor->HasComponent<FBoxCollisionComponent>()) {
                    const auto& box = floorHit.Actor->GetComponent<FBoxCollisionComponent>();
                    glm::vec3 wmax =
                        floorHit.Actor->GetActorLocation() + box.LocalMax * floorHit.Actor->GetActorScale();
                    standY = std::max(standY, wmax.y);
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
        transform.Translation.y = standY + EyeHeight;
    }

    void ACharacter::PostInitializeComponents() {
        if (!HasComponent<UCameraComponent>()) {
            FPerspectiveCamera camera(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            AddComponent<UCameraComponent>(camera);
        }
        auto& t = GetTransform();
        t.Translation.y = FloorZ + EyeHeight;
    }

    void ACharacter::Tick(float DeltaSeconds) {
        if (GetLocalRole() == ENetRole::SimulatedProxy)
            return;
        if (IsControlled()) {
            SetupPlayerInputComponent(DeltaSeconds);
        }
        SnapToFloor();
        auto& transform = GetTransform();
        if (HasComponent<UCameraComponent>()) {
            GetComponent<UCameraComponent>().Camera.SetPosition(transform.Translation);
        }
    }

    void ACharacter::SetupPlayerInputComponent(float DeltaSeconds) {
        if (APlayerController* pc = GetController()) {
            if (!pc->IsGameInputAllowed()) {
                bFirstMouse = true;
                return;
            }
        }

        const FInputSettings& input = FInputSettings::Get();
        auto& transform = GetTransform();

        if (input.bEnableMouseLook && FInput::IsMouseButtonPressed(Mouse::ButtonRight)) {
            auto [mx, my] = FInput::GetMousePosition();
            if (bFirstMouse) {
                LastMousePos = {mx, my};
                bFirstMouse = false;
            }

            float offsetX = (mx - LastMousePos.x) * LookSensitivity;
            float offsetY = (LastMousePos.y - my) * LookSensitivity;
            LastMousePos = {mx, my};

            Yaw += offsetX;
            Pitch += offsetY;
            Pitch = std::clamp(Pitch, -89.0f, 89.0f);
        } else {
            bFirstMouse = true;
        }

        // Horizontal forward on XZ (ignore pitch for movement)
        glm::vec3 forward;
        forward.x = std::cos(glm::radians(Yaw));
        forward.y = 0.0f;
        forward.z = std::sin(glm::radians(Yaw));
        forward = glm::normalize(forward);
        glm::vec3 look;
        look.x = std::cos(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));
        look.y = std::sin(glm::radians(Pitch));
        look.z = std::sin(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));
        look = SafeNormalize(look, glm::vec3(0.0f, 0.0f, -1.0f));

        glm::vec3 right, up;
        StableViewBasis(glm::vec3(forward.x, 0.0f, forward.z), right, up);
        (void)up;

        float speed = MoveSpeed;
        if (FInput::IsKeyPressed(input.SprintKey)) {
            speed *= SprintMultiplier;
        }
        float delta = speed * DeltaSeconds;

        if (FInput::IsKeyPressed(input.MoveForwardKey))
            MoveBlocked(forward * delta);
        if (FInput::IsKeyPressed(input.MoveBackwardKey))
            MoveBlocked(-forward * delta);
        if (FInput::IsKeyPressed(input.MoveLeftKey))
            MoveBlocked(-right * delta);
        if (FInput::IsKeyPressed(input.MoveRightKey))
            MoveBlocked(right * delta);

        glm::vec3& position = transform.Translation;

        if (HasComponent<UCameraComponent>()) {
            auto& camComp = GetComponent<UCameraComponent>();
            camComp.Camera.SetPosition(position);
            camComp.Camera.SetRotation(Pitch, Yaw);
        }

        transform.Rotation = EulerLookingAlong(look);
    }

} // namespace Leon
