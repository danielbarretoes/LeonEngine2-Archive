#pragma once

#include "Core/Base.hpp"
#include "Gameplay/UObject.hpp"
#include "UMG/FUIRenderer.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon {

    enum class ESlateVisibility {
        Visible,             // Visible and interactive
        Collapsed,           // Hidden and takes no layout space
        Hidden,              // Hidden but retains layout space
        HitTestInvisible,    // Visible but not interactive (all children ignored)
        SelfHitTestInvisible // Visible, self not interactive but children are
    };

    struct FGeometry {
        glm::vec2 Position{0.0f, 0.0f};
        glm::vec2 Size{100.0f, 30.0f};
        glm::vec2 AbsolutePosition{0.0f, 0.0f};

        bool IsUnderLocation(const glm::vec2& InPoint) const {
            return InPoint.x >= AbsolutePosition.x && InPoint.x <= AbsolutePosition.x + Size.x &&
                   InPoint.y >= AbsolutePosition.y && InPoint.y <= AbsolutePosition.y + Size.y;
        }
    };

    class UPanelWidget;

    /**
     * @brief Unreal Engine aligned base visual UI element.
     */
    class UWidget : public UObject {
    public:
        UWidget(const std::string& InName = "Widget");
        ~UWidget() override = default;

        virtual void Paint(const FGeometry& InAllottedGeometry);
        virtual void Tick(float InDeltaTime) { (void)InDeltaTime; }

        // --- Layout & Geometry ---
        void SetPosition(const glm::vec2& InPos) { Position = InPos; }
        const glm::vec2& GetPosition() const { return Position; }

        void SetSize(const glm::vec2& InSize) { Size = InSize; }
        const glm::vec2& GetSize() const { return Size; }

        const FGeometry& GetCachedGeometry() const { return CachedGeometry; }

        // --- Visibility ---
        void SetVisibility(ESlateVisibility InVisibility) { Visibility = InVisibility; }
        ESlateVisibility GetVisibility() const { return Visibility; }
        bool IsVisible() const {
            return Visibility == ESlateVisibility::Visible ||
                   Visibility == ESlateVisibility::SelfHitTestInvisible ||
                   Visibility == ESlateVisibility::HitTestInvisible;
        }
        bool IsHitTestable() const { return Visibility == ESlateVisibility::Visible; }

        // --- Hierarchy ---
        UPanelWidget* GetParent() const { return Parent; }
        void SetParent(UPanelWidget* InParent) { Parent = InParent; }
        virtual void RemoveFromParent();

        // --- FInput Events ---
        virtual bool OnMouseMove(const glm::vec2& InMousePos) {
            (void)InMousePos;
            return false;
        }
        virtual bool OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) {
            (void)InButton;
            (void)InMousePos;
            return false;
        }
        virtual bool OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) {
            (void)InButton;
            (void)InMousePos;
            return false;
        }
        virtual bool OnMouseWheel(float InWheelDelta, const glm::vec2& InMousePos) {
            (void)InWheelDelta;
            (void)InMousePos;
            return false;
        }

        virtual bool HitTest(const glm::vec2& InPoint) const;

    protected:
        glm::vec2 Position{0.0f, 0.0f};
        glm::vec2 Size{100.0f, 30.0f};
        ESlateVisibility Visibility = ESlateVisibility::Visible;
        FGeometry CachedGeometry;
        UPanelWidget* Parent = nullptr;
    };

} // namespace Leon
