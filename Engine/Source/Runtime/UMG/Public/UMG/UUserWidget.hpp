#pragma once

#include "UMG/UWidget.hpp"
#include <memory>

namespace Leon {

    class APlayerController;
    class AHUD;

    /**
     * @brief Unreal Engine aligned gameplay-authored UI User Widget.
     */
    class UUserWidget : public UWidget {
    public:
        UUserWidget(const std::string& InName = "UserWidget");
        ~UUserWidget() override = default;

        virtual void Construct() {}
        virtual void Destruct() {}

        void AddToViewport(int32_t InZOrder = 0);
        void RemoveFromParent() override;

        void SetWidgetTree(const TRef<UWidget>& InRootWidget);
        TRef<UWidget> GetWidgetTree() const { return RootWidget; }

        void SetOwningPlayer(APlayerController* InPC) { OwningPlayer = InPC; }
        APlayerController* GetOwningPlayer() const;

        void Paint(const FGeometry& InAllottedGeometry) override;
        void Tick(float InDeltaTime) override;

        bool OnMouseMove(const glm::vec2& InMousePos) override;
        bool OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) override;
        bool OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) override;

        int32_t GetZOrder() const { return ZOrder; }

        template <typename T, typename... TArgs>
        static TRef<T> CreateWidget(APlayerController* InOwningPlayer, TArgs&&... InArgs) {
            auto widget = std::make_shared<T>(std::forward<TArgs>(InArgs)...);
            widget->SetOwningPlayer(InOwningPlayer);
            widget->Construct();
            return widget;
        }

    protected:
        TRef<UWidget> RootWidget = nullptr;
        APlayerController* OwningPlayer = nullptr;
        int32_t ZOrder = 0;
        bool bConstructed = false;
    };

} // namespace Leon
