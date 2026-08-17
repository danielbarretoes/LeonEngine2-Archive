#include "Gameplay/ADefaultPawn.hpp"
#include "Core/FInputSettings.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Renderer/FRenderingMath.hpp"
#include <algorithm>
#include <cmath>

namespace Leon {

    ADefaultPawn::ADefaultPawn(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : APawn(InHandle, InWorld, InName) {
        SetClass("ADefaultPawn");
    }

    void ADefaultPawn::PostInitializeComponents() {
        if (!HasComponent<UCameraComponent>()) {
            FPerspectiveCamera camera(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            AddComponent<UCameraComponent>(camera);
        }
    }

    void ADefaultPawn::Tick(float DeltaSeconds) {
        if (GetLocalRole() == ENetRole::SimulatedProxy)
            return;
        if (IsControlled()) {
            SetupPlayerInputComponent(DeltaSeconds);
        }
    }

    void ADefaultPawn::SetupPlayerInputComponent(float DeltaSeconds) {
        // Respect PlayerController input mode (UIOnly blocks game movement)
        if (APlayerController* pc = dynamic_cast<APlayerController*>(GetController())) {
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

        glm::vec3 front;
        front.x = std::cos(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));
        front.y = std::sin(glm::radians(Pitch));
        front.z = std::sin(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));
        front = SafeNormalize(front, glm::vec3(0.0f, 0.0f, -1.0f));

        glm::vec3 right, up;
        StableViewBasis(front, right, up);

        float speed = MoveSpeed;
        if (FInput::IsKeyPressed(input.SprintKey)) {
            speed *= SprintMultiplier;
        }

        float delta = speed * DeltaSeconds;

        if (FInput::IsKeyPressed(input.MoveForwardKey))
            position += front * delta;
        if (FInput::IsKeyPressed(input.MoveBackwardKey))
            position -= front * delta;
        if (FInput::IsKeyPressed(input.MoveLeftKey))
            position -= right * delta;
        if (FInput::IsKeyPressed(input.MoveRightKey))
            position += right * delta;
        if (FInput::IsKeyPressed(input.MoveUpKey))
            position += up * delta;
        if (FInput::IsKeyPressed(input.MoveDownKey))
            position -= up * delta;

        if (HasComponent<UCameraComponent>()) {
            auto& camComp = GetComponent<UCameraComponent>();
            camComp.Camera.SetPosition(position);
            camComp.Camera.SetRotation(Pitch, Yaw);
        }

        transform.Rotation = EulerLookingAlong(front);
    }

} // namespace Leon
