#include "Gameplay/AHUD.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/FOnScreenDebugMessage.hpp"
#include "Core/FApplication.hpp"
#include "UMG/FUIRenderer.hpp"
#include <algorithm>

namespace Leon {

    AHUD::AHUD(entt::entity InHandle, UWorld* InWorld, const std::string& InName) : AActor(InHandle, InWorld, InName) {
        SetClass("AHUD");
        SetCanEverTick(true);
    }

    void AHUD::AddWidgetToViewport(const TRef<UUserWidget>& InWidget, int32_t InZOrder) {
        if (!InWidget)
            return;

        auto it = std::find(ViewportWidgets.begin(), ViewportWidgets.end(), InWidget);
        if (it == ViewportWidgets.end()) {
            ViewportWidgets.push_back(InWidget);
        }

        (void)InZOrder;
        std::stable_sort(
            ViewportWidgets.begin(), ViewportWidgets.end(),
            [](const TRef<UUserWidget>& a, const TRef<UUserWidget>& b) { return a->GetZOrder() < b->GetZOrder(); });
    }

    void AHUD::RemoveWidgetFromViewport(const TRef<UUserWidget>& InWidget) {
        auto it = std::find(ViewportWidgets.begin(), ViewportWidgets.end(), InWidget);
        if (it != ViewportWidgets.end()) {
            (*it)->Destruct();
            ViewportWidgets.erase(it);
        }
    }

    void AHUD::RemoveAllWidgets() {
        for (auto& widget : ViewportWidgets) {
            if (widget)
                widget->Destruct();
        }
        ViewportWidgets.clear();
    }

    void AHUD::Tick(float DeltaSeconds) {
        for (auto& widget : ViewportWidgets) {
            if (widget && widget->IsVisible()) {
                widget->Tick(DeltaSeconds);
            }
        }
    }

    void AHUD::EndPlay() {
        RemoveAllWidgets();
        AActor::EndPlay();
    }

    void AHUD::DrawHUD() {
        if (!FApplication::HasInstance())
            return;

        uint32_t vpWidth = FApplication::Get().GetWindow().GetWidth();
        uint32_t vpHeight = FApplication::Get().GetWindow().GetHeight();
        if (vpWidth == 0 || vpHeight == 0)
            return;

        FUIRenderer::Begin(vpWidth, vpHeight);

        FGeometry rootGeom;
        rootGeom.Position = {0.0f, 0.0f};
        rootGeom.Size = {static_cast<float>(vpWidth), static_cast<float>(vpHeight)};
        rootGeom.AbsolutePosition = {0.0f, 0.0f};

        for (auto& widget : ViewportWidgets) {
            if (widget && widget->IsVisible()) {
                widget->Paint(rootGeom);
            }
        }

        FOnScreenDebugMessageManager::Get().Draw(static_cast<float>(vpWidth), static_cast<float>(vpHeight));

        FUIRenderer::End();
    }

    bool AHUD::OnMouseMove(const glm::vec2& InMousePos) {
        bool bHandled = false;
        for (auto it = ViewportWidgets.rbegin(); it != ViewportWidgets.rend(); ++it) {
            if (*it && (*it)->IsHitTestable()) {
                if ((*it)->OnMouseMove(InMousePos)) {
                    bHandled = true;
                }
            }
        }
        return bHandled;
    }

    bool AHUD::OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) {
        for (auto it = ViewportWidgets.rbegin(); it != ViewportWidgets.rend(); ++it) {
            if (*it && (*it)->IsHitTestable()) {
                if ((*it)->OnMouseButtonDown(InButton, InMousePos)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool AHUD::OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) {
        for (auto it = ViewportWidgets.rbegin(); it != ViewportWidgets.rend(); ++it) {
            if (*it && (*it)->IsHitTestable()) {
                if ((*it)->OnMouseButtonUp(InButton, InMousePos)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool AHUD::OnMouseWheel(float InWheelDelta, const glm::vec2& InMousePos) {
        for (auto it = ViewportWidgets.rbegin(); it != ViewportWidgets.rend(); ++it) {
            if (*it && (*it)->IsHitTestable()) {
                if ((*it)->OnMouseWheel(InWheelDelta, InMousePos))
                    return true;
            }
        }
        return false;
    }

} // namespace Leon
