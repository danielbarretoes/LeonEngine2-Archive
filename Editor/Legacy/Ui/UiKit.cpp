#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/editor/ui/UiTokens.h>

namespace leon::editor::ui {
namespace {

struct VariantColors {
    ImVec4 fill{};
    ImVec4 fillHover{};
    ImVec4 fillActive{};
    ImVec4 border{};
    ImVec4 text{};
    ImVec4 icon{};
};

[[nodiscard]] VariantColors ResolveVariant(EUiVariant variant, bool disabled) {
    const UiPalette& t = Tokens();
    VariantColors c;
    switch (variant) {
    case EUiVariant::Primary:
        c.fill = t.accent;
        c.fillHover = t.accentHover;
        c.fillActive = t.accentActive;
        c.border = t.accentActive;
        c.text = t.textOnAccent;
        c.icon = t.textOnAccent;
        break;
    case EUiVariant::Secondary:
        c.fill = t.bg;
        c.fillHover = t.bgHover;
        c.fillActive = t.bgActive;
        c.border = t.borderStrong;
        c.text = t.text;
        c.icon = t.text;
        break;
    case EUiVariant::Ghost:
        c.fill = ImVec4(0, 0, 0, 0);
        c.fillHover = t.bgHover;
        c.fillActive = t.bgActive;
        c.border = ImVec4(0, 0, 0, 0);
        c.text = t.text;
        c.icon = t.text;
        break;
    case EUiVariant::Outline:
        c.fill = ImVec4(0, 0, 0, 0);
        c.fillHover = t.bgHover;
        c.fillActive = t.bgActive;
        c.border = t.borderStrong;
        c.text = t.text;
        c.icon = t.text;
        break;
    case EUiVariant::Destructive:
        c.fill = t.danger;
        c.fillHover = t.dangerHover;
        c.fillActive = Rgb8(120, 28, 28);
        c.border = t.dangerHover;
        c.text = t.textOnAccent;
        c.icon = t.textOnAccent;
        break;
    case EUiVariant::Warning:
        c.fill = t.warning;
        c.fillHover = t.warningHover;
        c.fillActive = Rgb8(140, 90, 20);
        c.border = t.warningHover;
        c.text = Rgb8(20, 16, 8);
        c.icon = Rgb8(20, 16, 8);
        break;
    case EUiVariant::Default:
    default:
        c.fill = t.bgElevated;
        c.fillHover = t.bgHover;
        c.fillActive = t.bgActive;
        c.border = t.border;
        c.text = t.text;
        c.icon = t.text;
        break;
    }
    if (disabled) {
        c.fill.w *= 0.45f;
        c.fillHover = c.fill;
        c.fillActive = c.fill;
        c.border.w *= 0.45f;
        c.text = t.textMuted;
        c.icon = t.textMuted;
    }
    return c;
}

[[nodiscard]] float MeasureLabelWidth(const char* label) {
    if (label == nullptr || label[0] == '\0') {
        return 0.0f;
    }
    return ImGui::CalcTextSize(label, nullptr, true).x;
}

[[nodiscard]] ImVec2 ComputeButtonSize(const UiButtonDesc& desc, const UiSizeMetrics& m) {
    const bool hasLabel = desc.label != nullptr && desc.label[0] != '\0';
    const bool hasIcon = desc.icon.present;
    float w = desc.width;
    if (w <= 0.0f) {
        w = m.padX * 2.0f;
        if (hasIcon) {
            w += m.icon;
        }
        if (hasIcon && hasLabel) {
            w += m.iconGap;
        }
        if (hasLabel) {
            w += MeasureLabelWidth(desc.label);
        }
        if (!hasIcon && !hasLabel) {
            w = m.height;
        }
        if (hasIcon && !hasLabel) {
            w = m.height;
        }
    }
    if (desc.stretch) {
        w = std::max(w, ImGui::GetContentRegionAvail().x);
    }
    return ImVec2(w, m.height);
}

void DrawLucideInBox(ImDrawList* draw, ImVec2 center, float box, ELucideIcon icon, ImU32 color) {
    const ImVec2 min(center.x - box * 0.5f, center.y - box * 0.5f);
    const ImVec2 max(center.x + box * 0.5f, center.y + box * 0.5f);
    DrawLucideIcon(draw, min, max, icon, color);
}

void MaybeTooltip(const char* tip) {
    if (tip != nullptr && tip[0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("%s", tip);
    }
}

[[nodiscard]] bool DrawStyledButton(const char* id, const UiButtonDesc& desc,
                                    bool forcePrimaryFill) {
    UiButtonDesc styled = desc;
    if (forcePrimaryFill) {
        styled.variant = EUiVariant::Primary;
    }
    const UiSizeMetrics m = SizeMetrics(styled.size);
    const VariantColors colors = ResolveVariant(styled.variant, styled.disabled);
    const ImVec2 size = ComputeButtonSize(styled, m);
    const bool hasLabel = styled.label != nullptr && styled.label[0] != '\0';
    const bool hasIcon = styled.icon.present;

    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, m.rounding);

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##btn", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const bool clicked = pressed && !styled.disabled;

    ImVec4 fill = colors.fill;
    if (!styled.disabled) {
        if (active) {
            fill = colors.fillActive;
        } else if (hovered) {
            fill = colors.fillHover;
        }
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 max(cursor.x + size.x, cursor.y + size.y);
    draw->AddRectFilled(cursor, max, ToU32(fill), m.rounding);
    if (colors.border.w > 0.01f) {
        draw->AddRect(cursor, max, ToU32(colors.border), m.rounding, 0, 1.0f);
    }

    const float midY = cursor.y + size.y * 0.5f;
    float contentX = cursor.x + m.padX;
    if (hasIcon && !hasLabel) {
        contentX = cursor.x + size.x * 0.5f;
        DrawLucideInBox(draw, ImVec2(contentX, midY), m.icon, styled.icon.id, ToU32(colors.icon));
    } else {
        if (hasIcon) {
            DrawLucideInBox(draw, ImVec2(contentX + m.icon * 0.5f, midY), m.icon, styled.icon.id,
                            ToU32(colors.icon));
            contentX += m.icon + m.iconGap;
        }
        if (hasLabel) {
            const ImVec2 textSize = ImGui::CalcTextSize(styled.label, nullptr, true);
            const float textY = midY - textSize.y * 0.5f;
            draw->AddText(ImVec2(contentX, textY), ToU32(colors.text), styled.label);
        }
    }

    MaybeTooltip(styled.tooltip);

    ImGui::PopStyleVar();
    ImGui::PopID();
    return clicked;
}

void PushControlStyle() {
    const UiPalette& t = Tokens();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, t.frame);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, t.frameHover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, t.frameActive);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, t.borderStrong);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, t.accent);
}

void PopControlStyle() {
    ImGui::PopStyleColor(5);
}

thread_local EUiSize g_selectSize = EUiSize::Md;
thread_local int g_formLayoutDepth = 0;
thread_local float g_formLabelWidth = 0.0f;
thread_local bool g_panelContentStyled = false;
thread_local bool g_panelFrameStyled = false;

[[nodiscard]] float ResolveControlWidth(float width) {
    if (width < 0.0f) {
        return ImGui::GetContentRegionAvail().x;
    }
    if (width <= 0.0f) {
        const float avail = ImGui::GetContentRegionAvail().x;
        if (g_formLayoutDepth > 0) {
            return std::max(96.0f, avail);
        }
        return std::max(Layout().controlMinWidth, avail);
    }
    return width;
}

[[nodiscard]] ImVec2 ComputeFramePadding(EUiSize size) {
    const UiSizeMetrics m = SizeMetrics(size);
    const float textH = ImGui::GetTextLineHeight();
    const float padY = std::max(m.padY, (m.height - textH) * 0.5f);
    return ImVec2(m.padX, padY);
}

[[nodiscard]] float LabelColumnX(float width) {
    if (width > 0.0f) {
        return width;
    }
    if (g_formLabelWidth > 0.0f) {
        return g_formLabelWidth;
    }
    return Layout().labelWidth;
}

void PushControlFrame(EUiSize size) {
    PushControlStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ComputeFramePadding(size));
}

void PopControlFrame() {
    ImGui::PopStyleVar();
    PopControlStyle();
}

[[nodiscard]] float SpacingPx(EUiSpacing spacing) {
    const UiLayoutMetrics& L = Layout();
    switch (spacing) {
    case EUiSpacing::Xs:
        return L.spacingXs;
    case EUiSpacing::Sm:
        return L.spacingSm;
    case EUiSpacing::Md:
        return L.spacingMd;
    case EUiSpacing::Lg:
        return L.spacingLg;
    }
    return L.spacingSm;
}

void AdvanceFixedRow(EUiSize size, ImVec2 rowStart, float rowWidth) {
    const UiSizeMetrics m = SizeMetrics(size);
    ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + m.height));
    ImGui::Dummy(ImVec2(rowWidth, 0.0f));
}

[[nodiscard]] ImVec2 BeginFixedRow(EUiSize size, float rowWidth) {
    const UiSizeMetrics m = SizeMetrics(size);
    const float width = rowWidth < 0.0f ? ImGui::GetContentRegionAvail().x : rowWidth;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##uikit_row", ImVec2(width, m.height));
    return start;
}

void PlaceControlInRow(EUiSize size, ImVec2 rowStart, float rowWidth, float xOffset) {
    PushControlFrame(size);
    const float frameH = ImGui::GetFrameHeight();
    const float rowH = SizeMetrics(size).height;
    const float y = rowStart.y + (rowH - frameH) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(rowStart.x + xOffset, y));
    ImGui::SetNextItemWidth(std::max(48.0f, rowWidth - xOffset));
}

} // namespace

bool Button(const char* id, const UiButtonDesc& desc) {
    return DrawStyledButton(id, desc, false);
}

bool Button(const char* id, const char* label, EUiVariant variant, EUiSize size) {
    UiButtonDesc desc;
    desc.label = label;
    desc.variant = variant;
    desc.size = size;
    return Button(id, desc);
}

bool IconButton(const char* id, ELucideIcon icon, EUiVariant variant, EUiSize size,
                const char* tooltip) {
    UiButtonDesc desc;
    desc.icon = UiIcon(icon);
    desc.variant = variant;
    desc.size = size;
    desc.tooltip = tooltip;
    return Button(id, desc);
}

bool ToolbarButton(const char* id, ELucideIcon icon, const char* label, const char* tooltip,
                   EUiVariant variant, EUiSize size) {
    UiButtonDesc desc;
    desc.icon = UiIcon(icon);
    desc.label = label;
    desc.variant = variant;
    desc.size = size;
    desc.tooltip = tooltip;
    return Button(id, desc);
}

bool ToggleButton(const char* id, bool* value, const UiButtonDesc& desc) {
    if (value == nullptr) {
        return false;
    }
    UiButtonDesc styled = desc;
    if (*value) {
        styled.variant = EUiVariant::Primary;
    } else if (styled.variant == EUiVariant::Primary) {
        styled.variant = EUiVariant::Secondary;
    }
    const bool clicked = Button(id, styled);
    if (clicked) {
        *value = !*value;
    }
    return clicked;
}

bool BeginSelect(const char* id, const UiSelectDesc& desc) {
    const UiSizeMetrics m = SizeMetrics(desc.size);
    g_selectSize = desc.size;
    const VariantColors colors = ResolveVariant(EUiVariant::Default, desc.disabled);
    const UiPalette& t = Tokens();
    const UiLayoutMetrics& L = Layout();

    float width = desc.width;
    if (width < 0.0f) {
        width = ImGui::GetContentRegionAvail().x;
    } else if (width <= 0.0f) {
        width = ImGui::CalcItemWidth();
    }
    width = std::max(width, m.height * 2.0f);

    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, m.rounding);

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImVec2 size(width, m.height);
    const bool openRequest = ImGui::InvisibleButton("##select", size) && !desc.disabled;
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    ImVec4 fill = colors.fill;
    if (!desc.disabled) {
        if (active) {
            fill = colors.fillActive;
        } else if (hovered) {
            fill = colors.fillHover;
        }
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 max(cursor.x + size.x, cursor.y + size.y);
    draw->AddRectFilled(cursor, max, ToU32(fill), m.rounding);
    draw->AddRect(cursor, max, ToU32(colors.border), m.rounding, 0, 1.0f);

    const float midY = cursor.y + size.y * 0.5f;
    float contentX = cursor.x + m.padX;
    const float chevronReserve = m.icon + m.padX;
    const float textMaxX = max.x - chevronReserve - 2.0f;

    if (desc.previewIcon.present) {
        DrawLucideInBox(draw, ImVec2(contentX + m.icon * 0.5f, midY), m.icon, desc.previewIcon.id,
                        ToU32(colors.icon));
        contentX += m.icon + m.iconGap;
    }

    const char* preview = desc.preview != nullptr ? desc.preview : "";
    const ImVec2 textSize = ImGui::CalcTextSize(preview, nullptr, true);
    const float textY = midY - textSize.y * 0.5f;
    draw->PushClipRect(ImVec2(contentX, cursor.y), ImVec2(textMaxX, max.y), true);
    draw->AddText(ImVec2(contentX, textY), ToU32(desc.disabled ? t.textMuted : colors.text),
                  preview);
    draw->PopClipRect();

    const float cx = max.x - m.padX - m.icon * 0.5f;
    const float cy = midY;
    const float s = m.icon * 0.28f;
    const ImU32 chevronCol = ToU32(t.textMuted);
    draw->AddLine(ImVec2(cx - s, cy - s * 0.35f), ImVec2(cx, cy + s * 0.45f), chevronCol, 1.4f);
    draw->AddLine(ImVec2(cx, cy + s * 0.45f), ImVec2(cx + s, cy - s * 0.35f), chevronCol, 1.4f);

    MaybeTooltip(desc.tooltip);

    if (openRequest) {
        ImGui::OpenPopup("##select_popup");
    }

    ImGui::SetNextWindowPos(ImVec2(cursor.x, max.y + Layout().popupOffset));
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(FLT_MAX, 300.0f));
    if (!ImGui::BeginPopup("##select_popup", ImGuiWindowFlags_NoMove)) {
        ImGui::PopStyleVar();
        ImGui::PopID();
        return false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, L.popupPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(L.itemSpacing.x, L.itemInnerSpacing.y));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, L.popupRounding);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, t.popupBg);
    ImGui::PushStyleColor(ImGuiCol_Border, t.borderStrong);
    return true;
}

bool SelectItem(const char* label, bool selected, UiIcon icon) {
    const UiSizeMetrics m = SizeMetrics(g_selectSize);
    const UiPalette& t = Tokens();
    const float rowH = m.height;
    ImGui::PushID(label != nullptr ? label : "##empty");
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool pressed = ImGui::InvisibleButton("##item", ImVec2(width, rowH));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = pressed;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 max(cursor.x + width, cursor.y + rowH);
    if (selected) {
        draw->AddRectFilled(cursor, max, ToU32(t.accentMuted), m.rounding);
        draw->AddRect(cursor, max, ToU32(t.accent), m.rounding, 0, 1.0f);
    } else if (hovered) {
        draw->AddRectFilled(cursor, max, ToU32(t.bgHover), m.rounding);
    }

    const float midY = cursor.y + rowH * 0.5f;
    float x = cursor.x + m.padX;
    if (icon.present) {
        DrawLucideInBox(draw, ImVec2(x + m.icon * 0.5f, midY), m.icon, icon.id, ToU32(t.text));
        x += m.icon + m.iconGap;
    }
    const char* text = label != nullptr ? label : "";
    const ImVec2 textSize = ImGui::CalcTextSize(text, nullptr, true);
    draw->AddText(ImVec2(x, midY - textSize.y * 0.5f), ToU32(t.text), text);

    if (clicked) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopID();
    return clicked;
}

void EndSelect() {
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::EndPopup();
    ImGui::PopStyleVar();
    ImGui::PopID();
}

bool Checkbox(const char* id, const char* label, bool* value, EUiSize size) {
    if (value == nullptr) {
        return false;
    }
    const UiSizeMetrics m = SizeMetrics(size);
    const UiPalette& t = Tokens();
    const float box = std::min(m.icon + 4.0f, m.height - 4.0f);

    ImGui::PushID(id);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float labelW = MeasureLabelWidth(label);
    const float totalW = box + (labelW > 0.0f ? m.iconGap + labelW : 0.0f);
    ImGui::InvisibleButton("##cb", ImVec2(totalW, m.height));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    if (clicked) {
        *value = !*value;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float midY = cursor.y + m.height * 0.5f;
    const ImVec2 bmin(cursor.x, midY - box * 0.5f);
    const ImVec2 bmax(cursor.x + box, midY + box * 0.5f);
    const ImVec4 fill = *value ? t.accent : (hovered ? t.bgHover : t.bgElevated);
    draw->AddRectFilled(bmin, bmax, ToU32(fill), 3.0f);
    draw->AddRect(bmin, bmax, ToU32(*value ? t.accentActive : t.borderStrong), 3.0f, 0, 1.0f);
    if (*value) {
        const float x0 = bmin.x + box * 0.22f;
        const float y0 = midY;
        const float x1 = bmin.x + box * 0.42f;
        const float y1 = bmax.y - box * 0.28f;
        const float x2 = bmax.x - box * 0.20f;
        const float y2 = bmin.y + box * 0.28f;
        draw->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), ToU32(t.textOnAccent), 1.6f);
        draw->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), ToU32(t.textOnAccent), 1.6f);
    }
    if (label != nullptr && label[0] != '\0') {
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);
        draw->AddText(ImVec2(bmax.x + m.iconGap, midY - textSize.y * 0.5f), ToU32(t.text), label);
    }
    ImGui::PopID();
    return clicked;
}

bool SelectFromList(const char* id, int* index, const char* const* items, int count, EUiSize size,
                    float width) {
    if (index == nullptr || items == nullptr || count <= 0) {
        return false;
    }
    const int clamped = std::clamp(*index, 0, count - 1);
    UiSelectDesc desc;
    desc.preview = items[clamped];
    desc.size = size;
    desc.width = width;
    bool changed = false;
    if (BeginSelect(id, desc)) {
        for (int i = 0; i < count; ++i) {
            if (SelectItem(items[i], i == clamped) && i != *index) {
                *index = i;
                changed = true;
            }
        }
        EndSelect();
    }
    return changed;
}

bool SegmentedControl(const char* id, int* index, const char* const* labels, int count,
                      EUiSize size) {
    if (index == nullptr || labels == nullptr || count <= 0) {
        return false;
    }
    ImGui::PushID(id);
    bool changed = false;
    const float gap = Layout().segmentedGap;
    for (int i = 0; i < count; ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, gap);
        }
        UiButtonDesc desc;
        desc.label = labels[i];
        desc.size = size;
        desc.variant = (i == *index) ? EUiVariant::Primary : EUiVariant::Secondary;
        char btnId[32];
        (void)std::snprintf(btnId, sizeof(btnId), "##seg%d", i);
        if (Button(btnId, desc) && i != *index) {
            *index = i;
            changed = true;
        }
    }
    ImGui::PopID();
    return changed;
}

bool DialogButton(const char* id, const char* label, EUiVariant variant, float width) {
    UiButtonDesc desc;
    desc.label = label;
    desc.variant = variant;
    desc.size = EUiSize::Md;
    desc.width = width;
    return Button(id, desc);
}

bool BeginPanel(const char* title, const UiPanelDesc& desc) {
    const UiLayoutMetrics& L = Layout();
    ImVec2 padding = L.panelPadding;
    if (desc.zeroPadding) {
        padding = L.zeroPanelPadding;
    } else if (desc.compactPadding) {
        padding = L.compactPanelPadding;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    const bool open = ImGui::Begin(title, desc.pOpen, desc.extraFlags);
    ImGui::PopStyleVar();
    g_panelContentStyled = open;
    if (open) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, L.itemSpacing);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, L.itemInnerSpacing);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ComputeFramePadding(EUiSize::Md));
        g_panelFrameStyled = true;
    }
    return open;
}

void EndPanel() {
    if (g_panelFrameStyled) {
        ImGui::PopStyleVar();
        g_panelFrameStyled = false;
    }
    if (g_panelContentStyled) {
        ImGui::PopStyleVar(2);
        g_panelContentStyled = false;
    }
    ImGui::End();
}

bool BeginPopupPanel(const char* id, float minWidth) {
    const UiPalette& t = Tokens();
    const UiLayoutMetrics& L = Layout();
    if (minWidth > 0.0f) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(minWidth, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    }
    if (!ImGui::BeginPopup(id)) {
        return false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, L.popupPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, L.itemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, L.itemInnerSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, L.popupRounding);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, t.popupBg);
    ImGui::PushStyleColor(ImGuiCol_Border, t.borderStrong);
    return true;
}

void EndPopupPanel() {
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
    ImGui::EndPopup();
}

void FieldLabel(const char* label, float width) {
    const UiPalette& t = Tokens();
    const float w = LabelColumnX(width);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, t.textMuted);
    ImGui::TextUnformatted(label != nullptr ? label : "");
    ImGui::PopStyleColor();
    ImGui::SameLine(w);
}

void FieldLabelBlock(const char* label) {
    const UiPalette& t = Tokens();
    const UiLayoutMetrics& L = Layout();
    if (label != nullptr && label[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, t.textMuted);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, L.sectionLabelGap));
    }
}

bool SliderFloat(const char* id, const char* label, float* value, float vMin, float vMax,
                 const char* format, float width) {
    if (value == nullptr) {
        return false;
    }
    PushControlFrame(EUiSize::Md);
    if (label != nullptr && label[0] != '\0') {
        FieldLabel(label);
    }
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ResolveControlWidth(width));
    const bool changed = ImGui::SliderFloat("##slider", value, vMin, vMax, format);
    ImGui::PopID();
    PopControlFrame();
    return changed;
}

bool DragFloat(const char* id, const char* label, float* value, float speed, float vMin, float vMax,
               const char* format, float width) {
    if (value == nullptr) {
        return false;
    }
    PushControlFrame(EUiSize::Md);
    if (label != nullptr && label[0] != '\0') {
        FieldLabel(label);
    }
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ResolveControlWidth(width));
    const bool changed = (vMin < vMax)
                             ? ImGui::DragFloat("##drag", value, speed, vMin, vMax, format)
                             : ImGui::DragFloat("##drag", value, speed, 0.0f, 0.0f, format);
    ImGui::PopID();
    PopControlFrame();
    return changed;
}

bool DragFloat3(const char* id, const char* label, float* value, float speed, float width) {
    if (value == nullptr) {
        return false;
    }
    PushControlFrame(EUiSize::Md);
    if (label != nullptr && label[0] != '\0') {
        FieldLabel(label);
    }
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ResolveControlWidth(width));
    const bool changed = ImGui::DragFloat3("##drag3", value, speed);
    ImGui::PopID();
    PopControlFrame();
    return changed;
}

bool DragInt(const char* id, const char* label, int* value, float speed, int vMin, int vMax,
             float width) {
    if (value == nullptr) {
        return false;
    }
    PushControlFrame(EUiSize::Md);
    if (label != nullptr && label[0] != '\0') {
        FieldLabel(label);
    }
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ResolveControlWidth(width));
    const bool changed = (vMin < vMax) ? ImGui::DragInt("##drag", value, speed, vMin, vMax)
                                       : ImGui::DragInt("##drag", value, speed);
    ImGui::PopID();
    PopControlFrame();
    return changed;
}

void PushFormLayout(float labelWidth) {
    g_formLabelWidth = labelWidth > 0.0f ? labelWidth : Layout().labelWidth;
    if (g_formLayoutDepth == 0) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Layout().formRowSpacing);
    }
    ++g_formLayoutDepth;
}

void PopFormLayout() {
    if (g_formLayoutDepth > 0) {
        --g_formLayoutDepth;
        if (g_formLayoutDepth == 0) {
            g_formLabelWidth = 0.0f;
            ImGui::PopStyleVar();
        }
    }
}

void Spacing(EUiSpacing spacing) {
    ImGui::Dummy(ImVec2(0.0f, SpacingPx(spacing)));
}

void Separator(bool vertical) {
    const UiPalette& t = Tokens();
    const float margin = Layout().separatorMargin;
    if (vertical) {
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float h = SizeMetrics(EUiSize::Md).height;
        ImGui::Dummy(ImVec2(10.0f, h));
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float x = cursor.x + 5.0f;
        draw->AddLine(ImVec2(x, cursor.y + margin), ImVec2(x, cursor.y + h - margin),
                      ToU32(t.borderStrong), 1.0f);
    } else {
        ImGui::Dummy(ImVec2(0.0f, margin));
        ImGui::PushStyleColor(ImGuiCol_Separator, t.borderStrong);
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, margin));
    }
}

void SectionLabel(const char* label) {
    const UiPalette& t = Tokens();
    Spacing(EUiSpacing::Sm);
    ImGui::PushStyleColor(ImGuiCol_Text, t.textMuted);
    ImGui::TextUnformatted(label != nullptr ? label : "");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, Layout().sectionLabelGap));
}

void Hint(const char* text) {
    const UiPalette& t = Tokens();
    ImGui::Dummy(ImVec2(0.0f, Layout().hintGap));
    ImGui::PushStyleColor(ImGuiCol_Text, t.textMuted);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextWrapped("%s", text != nullptr ? text : "");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, Layout().hintGap));
}

bool CollapsingSection(const char* title, ImGuiTreeNodeFlags flags) {
    const UiPalette& t = Tokens();
    const UiLayoutMetrics& L = Layout();
    ImFont* medium = Fonts().medium;
    if (medium != nullptr) {
        ImGui::PushFont(medium);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(L.chromeFramePadding.x, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, t.bgElevated);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, t.bgHover);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, t.bgActive);
    ImGui::PushStyleColor(ImGuiCol_Text, t.text);
    const bool open = ImGui::CollapsingHeader(title, flags | ImGuiTreeNodeFlags_FramePadding);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    if (medium != nullptr) {
        ImGui::PopFont();
    }
    if (open) {
        Spacing(EUiSpacing::Xs);
    }
    return open;
}

void TextBody(const char* text) {
    ImGui::TextUnformatted(text != nullptr ? text : "");
}

void TextMuted(const char* text) {
    const UiPalette& t = Tokens();
    ImGui::PushStyleColor(ImGuiCol_Text, t.textMuted);
    ImGui::TextUnformatted(text != nullptr ? text : "");
    ImGui::PopStyleColor();
}

void TextCaption(const char* text) {
    TextMuted(text);
}

void TextTitle(const char* text) {
    ImFont* medium = Fonts().medium;
    if (medium != nullptr) {
        ImGui::PushFont(medium);
    }
    ImGui::TextUnformatted(text != nullptr ? text : "");
    if (medium != nullptr) {
        ImGui::PopFont();
    }
}

void TextError(const char* text) {
    const UiPalette& t = Tokens();
    ImGui::PushStyleColor(ImGuiCol_Text, t.danger);
    ImGui::TextWrapped("%s", text != nullptr ? text : "");
    ImGui::PopStyleColor();
}

void TextPath(const char* path) {
    const UiPalette& t = Tokens();
    ImGui::PushStyleColor(ImGuiCol_Text, t.textMuted);
    ImGui::TextWrapped("%s", path != nullptr ? path : "");
    ImGui::PopStyleColor();
}

void LabelValue(const char* label, const char* value) {
    FieldLabel(label);
    TextBody(value != nullptr ? value : "");
}

bool ColorEdit3(const char* id, const char* label, float* rgb, float width) {
    if (rgb == nullptr) {
        return false;
    }
    PushControlFrame(EUiSize::Md);
    if (label != nullptr && label[0] != '\0') {
        FieldLabel(label);
    }
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ResolveControlWidth(width));
    const bool changed =
        ImGui::ColorEdit3("##color", rgb, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::PopID();
    PopControlFrame();
    return changed;
}

bool DragFloat2(const char* id, const char* label, float* value, float speed, float width) {
    if (value == nullptr) {
        return false;
    }
    PushControlFrame(EUiSize::Md);
    if (label != nullptr && label[0] != '\0') {
        FieldLabel(label);
    }
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ResolveControlWidth(width));
    const bool changed = ImGui::DragFloat2("##drag2", value, speed);
    ImGui::PopID();
    PopControlFrame();
    return changed;
}

bool DragFloat4(const char* id, const char* label, float* value, float speed, float width) {
    if (value == nullptr) {
        return false;
    }
    PushControlFrame(EUiSize::Md);
    if (label != nullptr && label[0] != '\0') {
        FieldLabel(label);
    }
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ResolveControlWidth(width));
    const bool changed = ImGui::DragFloat4("##drag4", value, speed);
    ImGui::PopID();
    PopControlFrame();
    return changed;
}

bool BeginModal(const char* name, bool* pOpen, float minWidth) {
    const UiPalette& t = Tokens();
    const UiLayoutMetrics& L = Layout();
    if (minWidth > 0.0f) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(minWidth, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, L.modalPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, L.modalRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, L.itemSpacing);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, t.popupBg);
    ImGui::PushStyleColor(ImGuiCol_Border, t.borderStrong);
    if (!ImGui::BeginPopupModal(name, pOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
        return false;
    }
    return true;
}

void EndModal() {
    ImGui::EndPopup();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

bool InputText(const char* id, const char* label, char* buf, size_t bufSize,
               const UiInputDesc& desc) {
    if (buf == nullptr || bufSize == 0) {
        return false;
    }
    PushControlFrame(desc.size);
    if (label != nullptr && label[0] != '\0') {
        FieldLabel(label);
    }
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ResolveControlWidth(desc.width));
    ImGuiInputTextFlags flags = desc.flags;
    if (desc.disabled) {
        flags |= ImGuiInputTextFlags_ReadOnly;
    }
    const bool changed = [&]() {
        if (desc.callback != nullptr) {
            if (desc.hint != nullptr && desc.hint[0] != '\0') {
                return ImGui::InputTextWithHint("##input", desc.hint, buf, bufSize, flags,
                                                desc.callback, desc.callbackUserData);
            }
            return ImGui::InputText("##input", buf, bufSize, flags, desc.callback,
                                    desc.callbackUserData);
        }
        if (desc.hint != nullptr && desc.hint[0] != '\0') {
            return ImGui::InputTextWithHint("##input", desc.hint, buf, bufSize, flags);
        }
        return ImGui::InputText("##input", buf, bufSize, flags);
    }();
    ImGui::PopID();
    PopControlFrame();
    return changed;
}

bool InputTextWithHint(const char* id, const char* label, char* buf, size_t bufSize,
                       const char* hint, const UiInputDesc& desc) {
    UiInputDesc withHint = desc;
    withHint.hint = hint;
    return InputText(id, label, buf, bufSize, withHint);
}

bool TextArea(const char* id, const char* label, char* buf, size_t bufSize,
              const UiTextAreaDesc& desc) {
    if (buf == nullptr || bufSize == 0) {
        return false;
    }
    if (label != nullptr && label[0] != '\0') {
        FieldLabelBlock(label);
    }
    const float lineH = ImGui::GetTextLineHeightWithSpacing();
    const float height = std::max(lineH * 2.0f, lineH * desc.minLines);
    PushControlFrame(desc.size);
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ResolveControlWidth(desc.width));
    ImGuiInputTextFlags flags = desc.flags;
    if (desc.disabled) {
        flags |= ImGuiInputTextFlags_ReadOnly;
    }
    const bool changed =
        ImGui::InputTextMultiline("##textarea", buf, bufSize, ImVec2(-FLT_MIN, height), flags);
    ImGui::PopID();
    PopControlFrame();
    return changed;
}

bool InputInt(const char* id, const char* label, int* value, int step, int stepFast,
              ImGuiInputTextFlags flags, float width, EUiSize size) {
    if (value == nullptr) {
        return false;
    }
    PushControlFrame(size);
    if (label != nullptr && label[0] != '\0') {
        FieldLabel(label);
    }
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ResolveControlWidth(width));
    const bool changed = ImGui::InputInt("##input_int", value, step, stepFast, flags);
    ImGui::PopID();
    PopControlFrame();
    return changed;
}

bool SearchField(const char* id, char* buf, size_t bufSize, const char* hint, float width) {
    if (buf == nullptr || bufSize == 0) {
        return false;
    }
    const UiPalette& t = Tokens();
    const UiSizeMetrics m = SizeMetrics(EUiSize::Sm);
    const float iconSlot = m.padX + m.icon + m.iconGap;
    const float availW = width < 0.0f ? ImGui::GetContentRegionAvail().x : width;

    ImGui::PushID(id);
    const ImVec2 rowStart = BeginFixedRow(EUiSize::Sm, availW);
    const float midY = rowStart.y + m.height * 0.5f;
    DrawLucideInBox(ImGui::GetWindowDrawList(), ImVec2(rowStart.x + m.padX + m.icon * 0.5f, midY),
                    m.icon, ELucideIcon::Search, ToU32(t.textMuted));

    PlaceControlInRow(EUiSize::Sm, rowStart, availW, iconSlot);
    const bool changed =
        ImGui::InputTextWithHint("##search", hint != nullptr ? hint : "Search…", buf, bufSize);
    PopControlFrame();
    AdvanceFixedRow(EUiSize::Sm, rowStart, availW);
    ImGui::PopID();
    return changed;
}

bool BeginGraphCanvas(const char* id, const UiGraphCanvasDesc& desc, UiGraphCanvas* out) {
    const UiPalette& t = Tokens();
    ImVec2 size = desc.size;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (size.x <= 0.0f) {
        size.x = avail.x;
    }
    if (size.y <= 0.0f) {
        size.y = std::max(120.0f, avail.y);
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, t.graphBg);
    ImGui::PushStyleColor(ImGuiCol_Border, t.borderStrong);
    const bool open = ImGui::BeginChild(id, size, true);
    ImGui::PopStyleColor(2);
    if (!open) {
        return false;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 canvasMin = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 canvasMax(canvasMin.x + windowSize.x, canvasMin.y + windowSize.y);
    const ImVec2 scroll(ImGui::GetScrollX(), ImGui::GetScrollY());

    if (desc.showGrid && desc.gridStep > 4.0f) {
        const float step = desc.gridStep;
        const ImU32 gridCol = ToU32(t.graphGrid);
        const float offX = std::fmod(scroll.x, step);
        const float offY = std::fmod(scroll.y, step);
        for (float x = canvasMin.x - offX; x < canvasMax.x; x += step) {
            draw->AddLine(ImVec2(x, canvasMin.y), ImVec2(x, canvasMax.y), gridCol, 1.0f);
        }
        for (float y = canvasMin.y - offY; y < canvasMax.y; y += step) {
            draw->AddLine(ImVec2(canvasMin.x, y), ImVec2(canvasMax.x, y), gridCol, 1.0f);
        }
    }

    if (out != nullptr) {
        out->origin = canvasMin;
        out->size = ImGui::GetWindowSize();
        out->scroll = scroll;
        out->draw = draw;
        out->id = ImGui::GetID(id);
    }

    if (desc.emptyHint != nullptr && desc.emptyHint[0] != '\0' &&
        ImGui::GetContentRegionAvail().y > 40.0f) {
        // Hint is shown by caller when canvas has no nodes; reserve no space here.
    }
    return true;
}

void EndGraphCanvas() {
    ImGui::EndChild();
}

bool BeginGraphNode(const char* id, const char* title, const UiGraphNodeDesc& desc) {
    const UiPalette& t = Tokens();
    const UiLayoutMetrics& L = Layout();
    const ImVec4 accent = desc.accent.w > 0.01f ? desc.accent : t.graphAccent;
    const float width = desc.width > 0.0f ? desc.width : -FLT_MIN;
    const UiSizeMetrics m = SizeMetrics(EUiSize::Sm);
    const float headerH = m.height - 2.0f;

    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, m.rounding);

    const ImVec2 nodeStart = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float nodeW = desc.width > 0.0f ? desc.width : ImGui::GetContentRegionAvail().x;
    draw->AddRectFilled(nodeStart, ImVec2(nodeStart.x + nodeW, nodeStart.y + headerH),
                        ToU32(t.graphNodeHeader), m.rounding, ImDrawFlags_RoundCornersTop);
    draw->AddRectFilled(nodeStart, ImVec2(nodeStart.x + nodeW, nodeStart.y + 2.5f), ToU32(accent),
                        m.rounding, ImDrawFlags_RoundCornersTop);
    const char* titleText = title != nullptr ? title : "";
    const ImVec2 textSize = ImGui::CalcTextSize(titleText, nullptr, true);
    draw->AddText(
        ImVec2(nodeStart.x + L.graphNodePadding.x, nodeStart.y + (headerH - textSize.y) * 0.5f),
        ToU32(t.text), titleText);

    ImGui::Dummy(ImVec2(nodeW, headerH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, L.graphNodePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, L.graphNodeItemSpacing);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, t.graphNodeBg);
    ImGui::PushStyleColor(ImGuiCol_Border, desc.selected ? t.accent : t.borderStrong);
    const bool open = ImGui::BeginChild("##graph_node_body", ImVec2(width, 0.0f), true);
    return open;
}

void EndGraphNode() {
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::PopID();
    Spacing(EUiSpacing::Sm);
}

bool GraphPinInt(const char* id, const char* label, int* value, float width) {
    if (value == nullptr) {
        return false;
    }
    const UiPalette& t = Tokens();
    const UiSizeMetrics m = SizeMetrics(EUiSize::Sm);
    const float rowW = width < 0.0f ? ImGui::GetContentRegionAvail().x : width;
    const float labelCol = LabelColumnX(0.0f);
    const float pinX = 6.0f;
    const float labelX = 18.0f;

    ImGui::PushID(id);
    const ImVec2 rowStart = BeginFixedRow(EUiSize::Sm, rowW);
    const float midY = rowStart.y + m.height * 0.5f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddCircleFilled(ImVec2(rowStart.x + pinX, midY), 4.0f, ToU32(t.graphPin));
    draw->AddCircle(ImVec2(rowStart.x + pinX, midY), 4.0f, ToU32(t.borderStrong), 0, 1.0f);

    const ImVec2 labelSize = ImGui::CalcTextSize(label != nullptr ? label : "", nullptr, true);
    draw->AddText(ImVec2(rowStart.x + labelX, midY - labelSize.y * 0.5f), ToU32(t.textMuted),
                  label != nullptr ? label : "");

    PlaceControlInRow(EUiSize::Sm, rowStart, rowW, labelCol);
    const bool changed = ImGui::InputInt("##graph_pin", value, 1, 10);
    PopControlFrame();
    AdvanceFixedRow(EUiSize::Sm, rowStart, rowW);
    ImGui::PopID();
    return changed;
}

void GraphSectionHeader(const char* title, const char* subtitle) {
    const UiPalette& t = Tokens();
    Spacing(EUiSpacing::Sm);
    ImGui::PushStyleColor(ImGuiCol_Text, t.graphAccent);
    ImGui::TextUnformatted(title != nullptr ? title : "");
    ImGui::PopStyleColor();
    if (subtitle != nullptr && subtitle[0] != '\0') {
        Hint(subtitle);
    } else {
        ImGui::Dummy(ImVec2(0.0f, Layout().sectionLabelGap));
    }
    Separator();
}

} // namespace leon::editor::ui
