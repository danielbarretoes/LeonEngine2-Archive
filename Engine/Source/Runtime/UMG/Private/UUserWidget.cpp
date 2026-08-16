#include "UMG/UUserWidget.hpp"
#include "UMG/UCanvasPanel.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/AHUD.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    UUserWidget::UUserWidget(const std::string& InName) : UWidget(InName) {}

    APlayerController* UUserWidget::GetOwningPlayer() const {
        return OwningPlayer;
    }

    void UUserWidget::SetWidgetTree(const TRef<UWidget>& InRootWidget) {
        RootWidget = InRootWidget;
    }

    void UUserWidget::AddToViewport(int32_t InZOrder) {
        ZOrder = InZOrder;
        if (!OwningPlayer)
            return;

        AHUD* hud = OwningPlayer->GetHUD();
        if (!hud)
            return;

        auto selfPtr = std::static_pointer_cast<UUserWidget>(shared_from_this());
        hud->AddWidgetToViewport(selfPtr, ZOrder);
    }

    void UUserWidget::RemoveFromParent() {
        if (OwningPlayer) {
            AHUD* hud = OwningPlayer->GetHUD();
            if (hud) {
                auto selfPtr = std::static_pointer_cast<UUserWidget>(shared_from_this());
                hud->RemoveWidgetFromViewport(selfPtr);
                UWidget::RemoveFromParent();
                return;
            }
        }
        UWidget::RemoveFromParent();
    }

    void UUserWidget::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;

        // Keep user widgets fullscreen so canvas anchors track the viewport.
        SetSize(InAllottedGeometry.Size);
        SetPosition({0.0f, 0.0f});

        if (RootWidget && RootWidget->IsVisible()) {
            if (auto* canvas = dynamic_cast<UCanvasPanel*>(RootWidget.get())) {
                canvas->SetSize(InAllottedGeometry.Size);
                canvas->SetPosition({0.0f, 0.0f});
                canvas->PerformLayout(InAllottedGeometry.Size);
            }

            FGeometry rootGeom;
            rootGeom.Position = {0.0f, 0.0f};
            rootGeom.Size = InAllottedGeometry.Size;
            rootGeom.AbsolutePosition = InAllottedGeometry.AbsolutePosition;

            RootWidget->Paint(rootGeom);
        }
    }

    void UUserWidget::Tick(float InDeltaTime) {
        UWidget::Tick(InDeltaTime);
        if (RootWidget) {
            RootWidget->Tick(InDeltaTime);
        }
    }

    bool UUserWidget::OnMouseMove(const glm::vec2& InMousePos) {
        if (!IsVisible())
            return false;
        if (RootWidget && RootWidget->IsHitTestable()) {
            return RootWidget->OnMouseMove(InMousePos);
        }
        return false;
    }

    bool UUserWidget::OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) {
        if (!IsVisible())
            return false;
        if (RootWidget && RootWidget->IsHitTestable()) {
            return RootWidget->OnMouseButtonDown(InButton, InMousePos);
        }
        return false;
    }

    bool UUserWidget::OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) {
        if (!IsVisible())
            return false;
        if (RootWidget && RootWidget->IsHitTestable()) {
            return RootWidget->OnMouseButtonUp(InButton, InMousePos);
        }
        return false;
    }

} // namespace Leon
