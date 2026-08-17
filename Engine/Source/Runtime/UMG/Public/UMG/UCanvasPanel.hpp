#pragma once

#include "UMG/UPanelWidget.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace Leon {

    /** Normalized [0,1] anchors on the parent (Unreal UMG style). */
    struct FAnchors {
        glm::vec2 Minimum{0.0f, 0.0f};
        glm::vec2 Maximum{0.0f, 0.0f};

        static FAnchors TopLeft() { return {}; }
        static FAnchors TopRight() { return {{1.0f, 0.0f}, {1.0f, 0.0f}}; }
        static FAnchors TopCenter() { return {{0.5f, 0.0f}, {0.5f, 0.0f}}; }
        static FAnchors BottomLeft() { return {{0.0f, 1.0f}, {0.0f, 1.0f}}; }
        static FAnchors BottomRight() { return {{1.0f, 1.0f}, {1.0f, 1.0f}}; }
        static FAnchors BottomCenter() { return {{0.5f, 1.0f}, {0.5f, 1.0f}}; }
        static FAnchors Center() { return {{0.5f, 0.5f}, {0.5f, 0.5f}}; }
        static FAnchors Fill() { return {{0.0f, 0.0f}, {1.0f, 1.0f}}; }
    };

    /** Pixel offsets from the anchored rectangle (Left/Top outward from min; Right/Bottom from max). */
    struct FMargin {
        float Left = 0.0f;
        float Top = 0.0f;
        float Right = 0.0f;
        float Bottom = 0.0f;

        FMargin() = default;
        FMargin(float InUniform) : Left(InUniform), Top(InUniform), Right(InUniform), Bottom(InUniform) {}
        FMargin(float InLeft, float InTop, float InRight, float InBottom)
            : Left(InLeft), Top(InTop), Right(InRight), Bottom(InBottom) {}
    };

    struct FCanvasPanelSlot {
        TRef<UWidget> Content;
        FAnchors Anchors;
        FMargin Offsets;
    };

    /**
     * @brief Unreal Engine aligned 2D layout canvas panel with anchors.
     */
    class UCanvasPanel : public UPanelWidget {
    public:
        UCanvasPanel(const std::string& InName = "CanvasPanel");
        ~UCanvasPanel() override = default;

        void SetBackgroundColor(const glm::vec4& InColor) { BackgroundColor = InColor; }
        const glm::vec4& GetBackgroundColor() const { return BackgroundColor; }

        void SetBorder(const glm::vec4& InBorderColor, float InBorderWidth = 1.0f) {
            BorderColor = InBorderColor;
            BorderWidth = InBorderWidth;
        }

        void AddChild(const TRef<UWidget>& InChild) override;
        bool RemoveChild(const TRef<UWidget>& InChild) override;
        void ClearChildren() override;

        /** Adds a child with UMG-style anchors and offsets. */
        void AddChild(const TRef<UWidget>& InChild, const FAnchors& InAnchors, const FMargin& InOffsets);

        bool SetChildLayout(const TRef<UWidget>& InChild, const FAnchors& InAnchors, const FMargin& InOffsets);

        /** Resolves child Position/Size from anchors given parent size. */
        void PerformLayout(const glm::vec2& InParentSize);

        void Paint(const FGeometry& InAllottedGeometry) override;

        const std::vector<FCanvasPanelSlot>& GetSlots() const { return Slots; }

    private:
        FCanvasPanelSlot* FindSlot(const TRef<UWidget>& InChild);
        void ApplySlotLayout(FCanvasPanelSlot& InSlot, const glm::vec2& InParentSize);

        std::vector<FCanvasPanelSlot> Slots;
        glm::vec4 BackgroundColor{0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 BorderColor{0.0f, 0.0f, 0.0f, 0.0f};
        float BorderWidth = 0.0f;
    };

} // namespace Leon
