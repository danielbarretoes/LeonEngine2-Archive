#pragma once

#include "Gameplay/AActor.hpp"
#include "UMG/UUserWidget.hpp"
#include <vector>

namespace Leon {

    class APlayerController;

    /**
     * @brief Unreal Engine aligned HUD base actor owned by APlayerController.
     * Manages viewport widgets, custom 2D canvas drawing, and UI input dispatch.
     */
    class AHUD : public AActor {
    public:
        AHUD() = default;
        AHUD(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "HUD");
        ~AHUD() override = default;

        virtual void DrawHUD();

        void Tick(float DeltaSeconds) override;
        void EndPlay() override;

        void AddWidgetToViewport(const TRef<UUserWidget>& InWidget, int32_t InZOrder = 0);
        void RemoveWidgetFromViewport(const TRef<UUserWidget>& InWidget);
        void RemoveAllWidgets();

        const std::vector<TRef<UUserWidget>>& GetViewportWidgets() const { return ViewportWidgets; }

        void SetPlayerController(APlayerController* InPC) { PlayerController = InPC; }
        APlayerController* GetPlayerController() const { return PlayerController; }

        // --- FInput Dispatch ---
        bool OnMouseMove(const glm::vec2& InMousePos);
        bool OnMouseButtonDown(int InButton, const glm::vec2& InMousePos);
        bool OnMouseButtonUp(int InButton, const glm::vec2& InMousePos);

    protected:
        APlayerController* PlayerController = nullptr;
        std::vector<TRef<UUserWidget>> ViewportWidgets;
    };

} // namespace Leon
