#include "UMG/UCanvasPanel.hpp"
#include "UMG/UPanelWidget.hpp"
#include "UMG/UTextBlock.hpp"
#include "UMG/FUIRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    UCanvasPanel::UCanvasPanel(const std::string& InName) : UPanelWidget(InName) {}

    FCanvasPanelSlot* UCanvasPanel::FindSlot(const TRef<UWidget>& InChild) {
        for (auto& slot : Slots) {
            if (slot.Content == InChild)
                return &slot;
        }
        return nullptr;
    }

    void UCanvasPanel::ApplySlotLayout(FCanvasPanelSlot& InSlot, const glm::vec2& InParentSize) {
        if (!InSlot.Content)
            return;

        const float x0 = InSlot.Anchors.Minimum.x * InParentSize.x + InSlot.Offsets.Left;
        const float y0 = InSlot.Anchors.Minimum.y * InParentSize.y + InSlot.Offsets.Top;
        const float x1 = InSlot.Anchors.Maximum.x * InParentSize.x - InSlot.Offsets.Right;
        const float y1 = InSlot.Anchors.Maximum.y * InParentSize.y - InSlot.Offsets.Bottom;

        InSlot.Content->SetPosition({x0, y0});
        InSlot.Content->SetSize({std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)});
    }

    void UCanvasPanel::PerformLayout(const glm::vec2& InParentSize) {
        for (auto& slot : Slots) {
            ApplySlotLayout(slot, InParentSize);
        }
    }

    void UCanvasPanel::AddChild(const TRef<UWidget>& InChild) {
        if (!InChild)
            return;
        // Absolute top-left: offsets encode Position + Size via Right/Bottom negative of size
        const glm::vec2 pos = InChild->GetPosition();
        const glm::vec2 size = InChild->GetSize();
        AddChild(InChild, FAnchors::TopLeft(), FMargin(pos.x, pos.y, -(pos.x + size.x), -(pos.y + size.y)));
    }

    void UCanvasPanel::AddChild(const TRef<UWidget>& InChild, const FAnchors& InAnchors, const FMargin& InOffsets) {
        if (!InChild)
            return;
        if (FindSlot(InChild))
            return;

        InChild->SetParent(this);
        Children.push_back(InChild);
        Slots.push_back({InChild, InAnchors, InOffsets});
        ApplySlotLayout(Slots.back(), GetSize());
    }

    bool UCanvasPanel::RemoveChild(const TRef<UWidget>& InChild) {
        Slots.erase(std::remove_if(Slots.begin(), Slots.end(),
                                     [&](const FCanvasPanelSlot& s) { return s.Content == InChild; }),
                      Slots.end());
        return UPanelWidget::RemoveChild(InChild);
    }

    void UCanvasPanel::ClearChildren() {
        Slots.clear();
        UPanelWidget::ClearChildren();
    }

    bool UCanvasPanel::SetChildLayout(const TRef<UWidget>& InChild, const FAnchors& InAnchors,
                                      const FMargin& InOffsets) {
        FCanvasPanelSlot* slot = FindSlot(InChild);
        if (!slot)
            return false;
        slot->Anchors = InAnchors;
        slot->Offsets = InOffsets;
        ApplySlotLayout(*slot, GetSize());
        return true;
    }

    void UCanvasPanel::ScaleLayout(float InFactor) {
        if (!std::isfinite(InFactor) || std::abs(InFactor - 1.0f) < 1.0e-4f)
            return;
        auto scaleFonts = [&](auto&& self, UWidget* widget) -> void {
            if (!widget)
                return;
            if (auto* text = dynamic_cast<UTextBlock*>(widget)) {
                text->SetFontScale(text->GetFontScale() * InFactor);
                const glm::vec2 sz = text->GetSize();
                text->SetSize({sz.x * InFactor, sz.y * InFactor});
            }
            if (auto* panel = dynamic_cast<UPanelWidget*>(widget)) {
                for (const auto& child : panel->GetAllChildren())
                    self(self, child.get());
            }
        };
        for (auto& slot : Slots) {
            slot.Offsets.Left *= InFactor;
            slot.Offsets.Top *= InFactor;
            slot.Offsets.Right *= InFactor;
            slot.Offsets.Bottom *= InFactor;
            scaleFonts(scaleFonts, slot.Content.get());
        }
        PerformLayout(GetSize());
    }

    void UCanvasPanel::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;

        PerformLayout(InAllottedGeometry.Size);

        if (BackgroundColor.a > 0.0f || (BorderWidth > 0.0f && BorderColor.a > 0.0f)) {
            glm::vec2 p0 = InAllottedGeometry.AbsolutePosition;
            glm::vec2 p1 = p0 + InAllottedGeometry.Size;
            FUIRenderer::DrawBorderQuad(p0, p1, BackgroundColor, BorderColor, BorderWidth);
        }

        for (auto& child : Children) {
            if (!child || !child->IsVisible())
                continue;

            FGeometry childGeom;
            childGeom.Position = child->GetPosition();
            childGeom.Size = child->GetSize();
            childGeom.AbsolutePosition = InAllottedGeometry.AbsolutePosition + child->GetPosition();

            child->Paint(childGeom);
        }
    }

} // namespace Leon
