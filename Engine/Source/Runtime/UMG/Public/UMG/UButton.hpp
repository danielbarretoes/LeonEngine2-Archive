#pragma once

#include "UMG/UPanelWidget.hpp"
#include <functional>

namespace Leon {

    enum class EButtonState { Normal, Hovered, Pressed, Disabled };

    /**
     * @brief Simple delegate event for button clicks.
     */
    class FOnButtonClickedEvent {
    public:
        using FCallback = std::function<void()>;

        void AddLambda(FCallback InCallback) { Callbacks.push_back(InCallback); }

        void AddDynamic(FCallback InCallback) { Callbacks.push_back(InCallback); }

        void Broadcast() {
            for (auto& cb : Callbacks) {
                if (cb)
                    cb();
            }
        }

        void Clear() { Callbacks.clear(); }

        FOnButtonClickedEvent& operator=(FCallback InCallback) {
            Callbacks.clear();
            if (InCallback)
                Callbacks.push_back(InCallback);
            return *this;
        }

    private:
        std::vector<FCallback> Callbacks;
    };

    /**
     * @brief Unreal Engine aligned interactive button widget.
     */
    class UButton : public UPanelWidget {
    public:
        UButton(const std::string& InName = "Button");
        ~UButton() override = default;

        // --- Interaction State ---
        void SetIsEnabled(bool bInEnabled) { bIsEnabled = bInEnabled; }
        bool IsEnabled() const { return bIsEnabled; }
        bool IsHovered() const { return bIsHovered; }
        bool IsPressed() const { return bIsPressed; }

        EButtonState GetCurrentState() const;

        // --- Colors & Styling ---
        void SetNormalColor(const glm::vec4& InColor) { NormalColor = InColor; }
        void SetHoveredColor(const glm::vec4& InColor) { HoveredColor = InColor; }
        void SetPressedColor(const glm::vec4& InColor) { PressedColor = InColor; }
        void SetDisabledColor(const glm::vec4& InColor) { DisabledColor = InColor; }
        void SetBorder(const glm::vec4& InBorderColor, float InBorderWidth = 1.0f) {
            BorderColor = InBorderColor;
            BorderWidth = InBorderWidth;
        }

        // --- Content & Events ---
        void SetContent(const TRef<UWidget>& InContent);
        TRef<UWidget> GetContent() const;

        FOnButtonClickedEvent OnClicked;

        // --- Overrides ---
        void Paint(const FGeometry& InAllottedGeometry) override;
        bool OnMouseMove(const glm::vec2& InMousePos) override;
        bool OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) override;
        bool OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) override;

    private:
        bool bIsEnabled = true;
        bool bIsHovered = false;
        bool bIsPressed = false;

        glm::vec4 NormalColor{0.18f, 0.22f, 0.28f, 0.95f};
        glm::vec4 HoveredColor{0.25f, 0.35f, 0.50f, 1.0f};
        glm::vec4 PressedColor{0.12f, 0.18f, 0.25f, 1.0f};
        glm::vec4 DisabledColor{0.10f, 0.10f, 0.10f, 0.6f};

        glm::vec4 BorderColor{0.35f, 0.55f, 0.85f, 0.8f};
        float BorderWidth = 1.5f;
    };

} // namespace Leon
