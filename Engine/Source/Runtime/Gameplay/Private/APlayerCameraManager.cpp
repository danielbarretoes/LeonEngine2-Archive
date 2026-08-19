#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace Leon {

    namespace {
        float WrapDeltaDegrees(float InFrom, float InTo) {
            float d = InTo - InFrom;
            while (d > 180.0f)
                d -= 360.0f;
            while (d < -180.0f)
                d += 360.0f;
            return d;
        }

        FPerspectiveCamera LerpCameraPose(const FPerspectiveCamera& InFrom, const FPerspectiveCamera& InTo, float InT) {
            const float t = std::clamp(InT, 0.0f, 1.0f);
            FPerspectiveCamera out = InTo;
            out.SetPosition(glm::mix(InFrom.GetPosition(), InTo.GetPosition(), t));
            out.SetRotation(glm::mix(InFrom.GetPitch(), InTo.GetPitch(), t),
                            InFrom.GetYaw() + WrapDeltaDegrees(InFrom.GetYaw(), InTo.GetYaw()) * t);
            out.SetProjection(glm::mix(InFrom.GetFOV(), InTo.GetFOV(), t), InTo.GetAspectRatio(), InTo.GetNearClip(),
                              InTo.GetFarClip());
            return out;
        }

        FPerspectiveCamera ResolveIdealCamera(APlayerCameraManager& InMgr, APlayerController* InPC, UWorld* InWorld,
                                              AActor* InViewTarget, const FPerspectiveCamera& InFallback) {
            if (InViewTarget) {
                if (auto* character = dynamic_cast<ACharacter*>(InViewTarget)) {
                    glm::vec3 loc, fwd;
                    character->GetViewPoint(loc, fwd);
                    (void)fwd;
                    FPerspectiveCamera cam = InFallback;
                    cam.SetPosition(loc);
                    cam.SetRotation(character->GetControlPitch(), character->GetControlYaw());
                    return cam;
                }
                if (InViewTarget->HasComponent<FCameraComponent>())
                    return InViewTarget->GetComponent<FCameraComponent>().Camera;
            }
            if (InPC) {
                APawn* pawn = InPC->GetPawn();
                if (pawn && pawn->HasComponent<FCameraComponent>())
                    return pawn->GetComponent<FCameraComponent>().Camera;
            }
            if (InWorld) {
                auto view = InWorld->GetRegistry().view<FCameraComponent, FTransformComponent>();
                for (auto entity : view) {
                    const auto& camComp = view.get<FCameraComponent>(entity);
                    if (camComp.bPrimary)
                        return camComp.Camera;
                }
            }
            (void)InMgr;
            return InFallback;
        }
    } // namespace

    APlayerCameraManager::APlayerCameraManager(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("APlayerCameraManager");
        Camera.SetPosition({0.0f, 3.5f, 10.5f});
        Camera.SetRotation(-10.0f, -90.0f);
    }

    void APlayerCameraManager::InitializeFor(APlayerController* InPC) {
        PlayerController = InPC;
    }

    void APlayerCameraManager::SetViewTarget(AActor* InNewTarget, float InBlendTime) {
        if (InBlendTime > 0.0f && InNewTarget && InNewTarget != ViewTarget) {
            BlendFrom = Camera;
            BlendDuration = InBlendTime;
            BlendElapsed = 0.0f;
            bBlending = true;
        } else {
            bBlending = false;
            BlendDuration = 0.0f;
            BlendElapsed = 0.0f;
        }
        ViewTarget = InNewTarget;
    }

    void APlayerCameraManager::SetAspectRatio(float InAspect) {
        Camera.SetProjection(Camera.GetFOV(), InAspect, Camera.GetNearClip(), Camera.GetFarClip());
    }

    void APlayerCameraManager::UpdateCamera(float DeltaSeconds) {
        const FPerspectiveCamera ideal =
            ResolveIdealCamera(*this, PlayerController, World, ViewTarget, Camera);

        if (!bBlending || BlendDuration <= 0.0f) {
            Camera = ideal;
            return;
        }

        BlendElapsed += std::max(DeltaSeconds, 0.0f);
        float t = std::clamp(BlendElapsed / BlendDuration, 0.0f, 1.0f);
        t = t * t * (3.0f - 2.0f * t);
        Camera = LerpCameraPose(BlendFrom, ideal, t);
        if (BlendElapsed >= BlendDuration)
            bBlending = false;
    }

} // namespace Leon
