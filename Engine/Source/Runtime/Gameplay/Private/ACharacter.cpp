#include "Gameplay/ACharacter.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Renderer/FRenderingMath.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    ACharacter::ACharacter(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : APawn(InHandle, InWorld, InName) {}

    void ACharacter::PostInitializeComponents() {
        if (!HasComponent<UCameraComponent>()) {
            FPerspectiveCamera camera(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            AddComponent<UCameraComponent>(camera);
        }
        auto& t = GetTransform();
        t.Translation.y = FloorZ + EyeHeight;
    }

    void ACharacter::Tick(float DeltaSeconds) {
        if (IsControlled()) {
            SetupPlayerInputComponent(DeltaSeconds);
        }
        // Always stick to floor plane (no physics).
        auto& transform = GetTransform();
        transform.Translation.y = FloorZ + EyeHeight;
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
        glm::vec3& position = transform.Translation;

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
            position += forward * delta;
        if (FInput::IsKeyPressed(input.MoveBackwardKey))
            position -= forward * delta;
        if (FInput::IsKeyPressed(input.MoveLeftKey))
            position -= right * delta;
        if (FInput::IsKeyPressed(input.MoveRightKey))
            position += right * delta;

        position.y = FloorZ + EyeHeight;

        if (HasComponent<UCameraComponent>()) {
            auto& camComp = GetComponent<UCameraComponent>();
            camComp.Camera.SetPosition(position);
            camComp.Camera.SetRotation(Pitch, Yaw);
        }

        transform.Rotation = EulerLookingAlong(look);
    }

} // namespace Leon
