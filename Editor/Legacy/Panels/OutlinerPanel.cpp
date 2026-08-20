#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <imgui_internal.h>
#include <leon/editor/EditorDisplayNames.h>
#include <leon/editor/EditorCommands.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/OutlinerPanel.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/gameplay/Actor.h>
#include <leon/gameplay/SceneComponent.h>
#include <leon/gameplay/World.h>
#include <leon/level/Level.h>
#include <string>
#include <typeinfo>

namespace leon::editor {
namespace {

enum class EOutlinerIcon : std::uint8_t {
    Level,
    StaticMeshes,
    StaticMesh,
    Lights,
    DirectionalLight,
    PointLight,
    SpotLight,
    PlayerStarts,
    PlayerStart,
    TriggerVolumes,
    TriggerVolume,
    PainVolumes,
    PainVolume,
    AISpawns,
    AISpawn,
    TextRenderActors,
    TextRenderActor,
    World,
    Blueprint,
    Actor,
    Component,
};

[[nodiscard]] const char* PrettyTypeName(const char* mangled) {
    if (mangled == nullptr) {
        return "Unknown";
    }
    if (std::strncmp(mangled, "class ", 6) == 0) {
        return mangled + 6;
    }
    if (std::strncmp(mangled, "struct ", 7) == 0) {
        return mangled + 7;
    }
    return mangled;
}

void DrawVisibilityIcon(ImDrawList* draw, ImVec2 p, float size, bool hidden) {
    DrawLucideIcon(draw, p, ImVec2(p.x + size, p.y + size),
                   hidden ? ELucideIcon::EyeOff : ELucideIcon::Eye,
                   hidden ? IM_COL32(200, 90, 90, 255) : IM_COL32(210, 210, 215, 255));
}

void DrawLockIcon(ImDrawList* draw, ImVec2 p, float size, bool locked) {
    DrawLucideIcon(draw, p, ImVec2(p.x + size, p.y + size),
                   locked ? ELucideIcon::Lock : ELucideIcon::LockOpen,
                   locked ? IM_COL32(230, 180, 70, 255) : IM_COL32(140, 140, 145, 220));
}

void DrawOutlinerIcon(ImDrawList* draw, ImVec2 p, float size, EOutlinerIcon kind) {
    if (draw == nullptr || size < 4.0f) {
        return;
    }
    ELucideIcon lucide = ELucideIcon::Circle;
    ImU32 color = IM_COL32(200, 200, 210, 255);
    switch (kind) {
    case EOutlinerIcon::Level:
        lucide = ELucideIcon::Map;
        color = IM_COL32(120, 180, 255, 255);
        break;
    case EOutlinerIcon::StaticMeshes:
        lucide = ELucideIcon::Boxes;
        color = IM_COL32(80, 190, 130, 255);
        break;
    case EOutlinerIcon::StaticMesh:
        lucide = ELucideIcon::Box;
        color = IM_COL32(100, 210, 150, 255);
        break;
    case EOutlinerIcon::Lights:
        lucide = ELucideIcon::Sun;
        color = IM_COL32(255, 210, 70, 255);
        break;
    case EOutlinerIcon::DirectionalLight:
        lucide = ELucideIcon::Sun;
        color = IM_COL32(255, 220, 90, 255);
        break;
    case EOutlinerIcon::PointLight:
        lucide = ELucideIcon::Lightbulb;
        color = IM_COL32(255, 230, 120, 255);
        break;
    case EOutlinerIcon::SpotLight:
        lucide = ELucideIcon::Sun;
        color = IM_COL32(255, 200, 80, 255);
        break;
    case EOutlinerIcon::PlayerStarts:
    case EOutlinerIcon::PlayerStart:
        lucide = ELucideIcon::User;
        color = IM_COL32(100, 220, 140, 255);
        break;
    case EOutlinerIcon::TriggerVolumes:
    case EOutlinerIcon::TriggerVolume:
        lucide = ELucideIcon::Zap;
        color = IM_COL32(80, 210, 230, 255);
        break;
    case EOutlinerIcon::PainVolumes:
    case EOutlinerIcon::PainVolume:
        lucide = ELucideIcon::Flame;
        color = IM_COL32(240, 90, 80, 255);
        break;
    case EOutlinerIcon::AISpawns:
    case EOutlinerIcon::AISpawn:
        lucide = ELucideIcon::Crosshair;
        color = IM_COL32(240, 170, 70, 255);
        break;
    case EOutlinerIcon::TextRenderActors:
    case EOutlinerIcon::TextRenderActor:
        lucide = ELucideIcon::FileText;
        color = IM_COL32(255, 235, 140, 255);
        break;
    case EOutlinerIcon::World:
        lucide = ELucideIcon::Globe;
        color = IM_COL32(120, 180, 230, 255);
        break;
    case EOutlinerIcon::Blueprint:
        lucide = ELucideIcon::Braces;
        color = IM_COL32(100, 160, 255, 255);
        break;
    case EOutlinerIcon::Actor:
        lucide = ELucideIcon::Box;
        color = IM_COL32(220, 190, 100, 255);
        break;
    case EOutlinerIcon::Component:
        lucide = ELucideIcon::Component;
        color = IM_COL32(180, 180, 200, 255);
        break;
    }
    DrawLucideIcon(draw, p, ImVec2(p.x + size, p.y + size), lucide, color);
}

[[nodiscard]] float OutlinerIconSize() {
    return ImGui::GetTextLineHeight();
}

/// Compact shared row height for folders, actors, components, and leaves.
[[nodiscard]] float OutlinerRowHeight() {
    return ImGui::GetTextLineHeight() + 4.0f;
}

/// Tree folder / actor / component row: arrow + icon + label (label drawn manually).
[[nodiscard]] bool OutlinerTreeNode(const char* id, const char* label, ImGuiTreeNodeFlags flags,
                                    EOutlinerIcon icon) {
    const float iconSz = OutlinerIconSize();
    const float pad = 4.0f;
    const float rowH = OutlinerRowHeight();
    // Leading space → label_size.y == FontSize; FramePadding matches OutlinerRowHeight().
    char hiddenId[96];
    (void)std::snprintf(hiddenId, sizeof(hiddenId), " ##%s", id);
    flags |= ImGuiTreeNodeFlags_FramePadding;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x,
                                                           (rowH - ImGui::GetFontSize()) * 0.5f));
    const bool open = ImGui::TreeNodeEx(hiddenId, flags);
    ImGui::PopStyleVar();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float x = min.x + ImGui::GetTreeNodeToLabelSpacing();
    const float yIcon = min.y + (max.y - min.y - iconSz) * 0.5f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    DrawOutlinerIcon(draw, ImVec2(x, yIcon), iconSz, icon);
    const float yText = min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f;
    draw->AddText(ImVec2(x + iconSz + pad, yText), ImGui::GetColorU32(ImGuiCol_Text), label);
    return open;
}

/// Selectable leaf with type icon drawn in the row.
/// Extra TreeNode label spacing aligns the icon under parent folder labels (not under the arrow).
/// Do not use SpanAllColumns — that starts the item at WorkRect.Min.x and kills tree indent.
[[nodiscard]] bool OutlinerSelectable(const char* id, const char* label, bool selected,
                                      EOutlinerIcon icon, float width) {
    const float iconSz = OutlinerIconSize();
    const float pad = 4.0f;
    const float rowH = OutlinerRowHeight();
    const float labelIndent = ImGui::GetTreeNodeToLabelSpacing();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelIndent);
    ImGui::PushID(id);
    const float selW = std::max(40.0f, width - labelIndent);
    // NoPadWithHalfSpacing: default Selectable pads ±ItemSpacing/2 and made leaves look taller
    // than FramePadding TreeNodes.
    const bool clicked = ImGui::Selectable(
        " ##row", selected, ImGuiSelectableFlags_NoPadWithHalfSpacing, ImVec2(selW, rowH));
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float y = min.y + (max.y - min.y - iconSz) * 0.5f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    DrawOutlinerIcon(draw, ImVec2(min.x + 2.0f, y), iconSz, icon);
    const ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
    draw->AddText(ImVec2(min.x + 2.0f + iconSz + pad,
                         min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f),
                  textCol, label);
    ImGui::PopID();
    return clicked;
}

[[nodiscard]] bool MatchesOutlinerFilter(const char* filter, const char* text) {
    if (filter == nullptr || filter[0] == '\0') {
        return true;
    }
    if (text == nullptr) {
        return false;
    }
    const std::size_t n = std::strlen(filter);
    const std::size_t m = std::strlen(text);
    if (n > m) {
        return false;
    }
    for (std::size_t i = 0; i + n <= m; ++i) {
        bool ok = true;
        for (std::size_t j = 0; j < n; ++j) {
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + j])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(filter[j])));
            if (a != b) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return true;
        }
    }
    return false;
}

void DrawSceneComponentTree(SceneComponent& component, int depth) {
    char label[160];
    const char* typeName = PrettyTypeName(typeid(component).name());
    if (depth == 0) {
        (void)std::snprintf(label, sizeof(label), "Root (%s)", typeName);
    } else {
        (void)std::snprintf(label, sizeof(label), "%s", typeName);
    }

    char idBuf[64];
    (void)std::snprintf(idBuf, sizeof(idBuf), "sc%p", static_cast<void*>(&component));

    const auto& children = component.GetAttachChildren();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (depth == 0) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    const bool open = OutlinerTreeNode(idBuf, label, flags, EOutlinerIcon::Component);
    if (!children.empty() && open) {
        for (SceneComponent* child : children) {
            if (child != nullptr) {
                DrawSceneComponentTree(*child, depth + 1);
            }
        }
        ImGui::TreePop();
    }
}

} // namespace

void OutlinerPanel::CancelRename() {
    renaming_ = false;
    renameKind_ = EEditorSelectionKind::None;
    renameIndex_ = 0;
    renameBuf_[0] = '\0';
    renameFocusPending_ = false;
}

void OutlinerPanel::BeginRename(EditorContext& ctx, EEditorSelectionKind kind, std::size_t index,
                                const std::string& currentLabel) {
    if (EditorCommands::ActorLabelPtr(ctx, kind, index) == nullptr) {
        return;
    }
    renaming_ = true;
    renameKind_ = kind;
    renameIndex_ = index;
    (void)std::snprintf(renameBuf_, sizeof(renameBuf_), "%s", currentLabel.c_str());
    renameFocusPending_ = true;
}

bool OutlinerPanel::ApplyRename(EditorContext& ctx) {
    std::string* storage = EditorCommands::ActorLabelPtr(ctx, renameKind_, renameIndex_);
    if (storage == nullptr) {
        CancelRename();
        return false;
    }
    if (ctx.history != nullptr) {
        ctx.history->Capture(ctx);
    }
    *storage = renameBuf_;
    ctx.MarkDirty();
    CancelRename();
    return true;
}

void OutlinerPanel::HandleDelete(EditorContext& ctx) {
    if (ctx.contentBrowserFocused) {
        return;
    }
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        return;
    }
    if (ImGui::GetIO().WantTextInput) {
        return;
    }
    if (!ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        return;
    }
    EditorCommands::DeleteSelection(ctx, ctx.history);
}

void OutlinerPanel::HandleRenameHotkey(EditorContext& ctx) {
    const bool renameRequested = ctx.requestRenameSelected;
    ctx.requestRenameSelected = false;
    if (renaming_ || ImGui::GetIO().WantTextInput) {
        return;
    }
    if (!renameRequested && (!ImGui::IsKeyPressed(ImGuiKey_F2) ||
                             !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))) {
        return;
    }
    if (!ctx.selection.IsValid()) {
        return;
    }
    char visible[128]{};
    const EEditorSelectionKind kind = ctx.selection.kind;
    const std::size_t i = ctx.selection.index;
    if (EditorCommands::ActorLabelPtr(ctx, kind, i) == nullptr) {
        return;
    }
    if (kind == EEditorSelectionKind::StaticMesh && ctx.level != nullptr &&
        i < ctx.level->StaticMeshes().size()) {
        const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[i];
        if (!mesh.tag.empty()) {
            (void)std::snprintf(visible, sizeof(visible), "%s", mesh.tag.c_str());
        } else {
            const char* cls = mesh.editorClass.empty() ? "StaticMesh" : mesh.editorClass.c_str();
            FormatIndexedActorLabel(cls, i, visible, sizeof(visible));
        }
    } else if (kind == EEditorSelectionKind::BlueprintInstance && ctx.level != nullptr &&
               i < ctx.level->BlueprintInstances().size()) {
        const BlueprintInstance& bp = ctx.level->BlueprintInstances()[i];
        (void)std::snprintf(visible, sizeof(visible), "%s",
                            bp.actorLabel.empty() ? "Blueprint" : bp.actorLabel.c_str());
    } else if (kind == EEditorSelectionKind::TriggerVolume) {
        FormatIndexedActorLabel("TriggerVolume", i, visible, sizeof(visible));
        if (ctx.level != nullptr && i < ctx.level->TriggerVolumes().size() &&
            !ctx.level->TriggerVolumes()[i].tag.empty()) {
            (void)std::snprintf(visible, sizeof(visible), "%s",
                                ctx.level->TriggerVolumes()[i].tag.c_str());
        }
    } else if (kind == EEditorSelectionKind::PainCausingVolume) {
        FormatIndexedActorLabel("PainCausingVolume", i, visible, sizeof(visible));
        if (ctx.level != nullptr && i < ctx.level->PainCausingVolumes().size() &&
            !ctx.level->PainCausingVolumes()[i].tag.empty()) {
            (void)std::snprintf(visible, sizeof(visible), "%s",
                                ctx.level->PainCausingVolumes()[i].tag.c_str());
        }
    } else if (kind == EEditorSelectionKind::AISpawnPoint) {
        FormatIndexedActorLabel("AISpawnPoint", i, visible, sizeof(visible));
        if (ctx.level != nullptr && i < ctx.level->AISpawnPoints().size() &&
            !ctx.level->AISpawnPoints()[i].tag.empty()) {
            (void)std::snprintf(visible, sizeof(visible), "%s",
                                ctx.level->AISpawnPoints()[i].tag.c_str());
        }
    } else if (kind == EEditorSelectionKind::TextRenderActor) {
        FormatIndexedActorLabel("TextRenderActor", i, visible, sizeof(visible));
        if (ctx.level != nullptr && i < ctx.level->TextRenderActors().size() &&
            !ctx.level->TextRenderActors()[i].Text.empty()) {
            (void)std::snprintf(visible, sizeof(visible), "%s",
                                ctx.level->TextRenderActors()[i].Text.c_str());
        }
    } else {
        return;
    }
    BeginRename(ctx, kind, i, visible);
}

void OutlinerPanel::Draw(EditorContext& ctx) {
    if (!ctx.showOutliner) {
        return;
    }
    if (!ui::BeginPanel("World Outliner", {.pOpen = &ctx.showOutliner})) {
        ui::EndPanel();
        return;
    }

    HandleDelete(ctx);
    HandleRenameHotkey(ctx);

    (void)ui::SearchField("##outliner_search", searchBuf_, sizeof(searchBuf_));
    const char* filter = searchBuf_;

    auto drawLeaf = [&](EEditorSelectionKind kind, std::size_t i, const char* id,
                        const char* visible, EOutlinerIcon icon) {
        if (!MatchesOutlinerFilter(filter, visible)) {
            return;
        }
        const bool selected = ctx.IsSelected(kind, i);
        const bool isRenaming = renaming_ && renameKind_ == kind && renameIndex_ == i;
        if (isRenaming) {
            ImGui::PushID(id);
            if (renameFocusPending_) {
                ImGui::SetKeyboardFocusHere();
                renameFocusPending_ = false;
            }
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetTreeNodeToLabelSpacing());
            ui::UiInputDesc renameDesc;
            renameDesc.size = ui::EUiSize::Sm;
            renameDesc.width = -1.0f;
            renameDesc.flags =
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
            if (ui::InputText("##rename", nullptr, renameBuf_, sizeof(renameBuf_), renameDesc)) {
                (void)ApplyRename(ctx);
            }
            if (ImGui::IsItemDeactivatedAfterEdit() && renaming_) {
                (void)ApplyRename(ctx);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                CancelRename();
            }
            ImGui::PopID();
            return;
        }
        ImGui::PushID(id);
        const float rowStartX = ImGui::GetCursorPosX();
        const float rowStartY = ImGui::GetCursorPosY();
        const float avail = ImGui::GetContentRegionAvail().x;
        const float rowH = OutlinerRowHeight();
        // Icon-sized controls — full rowH squares made leaf rows look much taller than folders.
        const float btnW = OutlinerIconSize() + 2.0f;
        const float btnPadY = std::max(0.0f, (rowH - btnW) * 0.5f);
        // Leave room for eye/lock so Selectable does not span under them.
        const float selWidth = std::max(40.0f, avail - btnW * 2.2f);
        if (OutlinerSelectable("##row", visible, selected, icon, selWidth)) {
            ctx.Select(kind, i, ImGui::GetIO().KeyCtrl);
        }
        if (ImGui::BeginPopupContextItem("##outliner_leaf_ctx")) {
            if (!ctx.IsSelected(kind, i)) {
                ctx.Select(kind, i, false);
            }
            if (EditorCommands::DrawSelectionContextMenuItems(ctx)) {
                BeginRename(ctx, kind, i, visible);
            }
            ImGui::EndPopup();
        }
        if (selected && ImGui::IsItemHovered() &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            EditorCommands::ActorLabelPtr(ctx, kind, i) != nullptr) {
            BeginRename(ctx, kind, i, visible);
        }
        // Overlay eye/lock on the same row; absolute cursor avoids SameLine/NewLine growing the
        // line.
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImGui::SetCursorPos(ImVec2(rowStartX + avail - btnW * 2.1f, rowStartY + btnPadY));
        if (bool* hidden = EditorCommands::HiddenPtr(ctx, kind, i)) {
            if (ImGui::InvisibleButton("##vis", ImVec2(btnW, btnW))) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx, "Toggle Visibility");
                }
                *hidden = !*hidden;
                ctx.MarkDirty();
            }
            DrawVisibilityIcon(draw, ImGui::GetItemRectMin(), btnW, *hidden);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Visibility");
            }
        } else {
            ImGui::Dummy(ImVec2(btnW, btnW));
        }
        ImGui::SetCursorPos(ImVec2(rowStartX + avail - btnW * 1.05f, rowStartY + btnPadY));
        if (bool* locked = EditorCommands::EditorLockedPtr(ctx, kind, i)) {
            if (ImGui::InvisibleButton("##lock", ImVec2(btnW, btnW))) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx, "Toggle Lock");
                }
                *locked = !*locked;
                ctx.MarkDirty();
            }
            DrawLockIcon(draw, ImGui::GetItemRectMin(), btnW, *locked);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Lock");
            }
        } else {
            ImGui::Dummy(ImVec2(btnW, btnW));
        }
        ImGui::SetCursorPos(ImVec2(rowStartX, rowStartY + rowH + ImGui::GetStyle().ItemSpacing.y));
        ImGui::PopID();
    };

    char labelBuf[160];
    constexpr ImGuiTreeNodeFlags kFolderFlags = ImGuiTreeNodeFlags_DefaultOpen |
                                                ImGuiTreeNodeFlags_SpanAvailWidth |
                                                ImGuiTreeNodeFlags_OpenOnArrow;

    // Clear parent→child step (theme default 16 is fine; keep slightly larger than arrow column).
    const float treeStep =
        std::max(ImGui::GetStyle().IndentSpacing, ImGui::GetTreeNodeToLabelSpacing());
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, treeStep);
    // Tight vertical rhythm shared by TreeNodes and leaf Selectables.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 2.0f));

    if (OutlinerTreeNode("folder_level", "Level", kFolderFlags, EOutlinerIcon::Level)) {
        if (OutlinerTreeNode("folder_sm", "Static meshes", kFolderFlags,
                             EOutlinerIcon::StaticMeshes) &&
            ctx.level) {
            for (std::size_t i = 0; i < ctx.level->StaticMeshes().size(); ++i) {
                const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[i];
                if (mesh.blueprintInstanceId != 0) {
                    continue; // shown under Blueprints instance
                }
                char id[32];
                (void)std::snprintf(id, sizeof(id), "mesh%zu", i);
                char visible[128];
                if (!mesh.tag.empty()) {
                    (void)std::snprintf(visible, sizeof(visible), "%s", mesh.tag.c_str());
                } else {
                    const char* cls =
                        mesh.editorClass.empty() ? "StaticMesh" : mesh.editorClass.c_str();
                    FormatIndexedActorLabel(cls, i, visible, sizeof(visible));
                }
                drawLeaf(EEditorSelectionKind::StaticMesh, i, id, visible,
                         EOutlinerIcon::StaticMesh);
            }
            ImGui::TreePop();
        }
        if (OutlinerTreeNode("folder_lights", "Lights", kFolderFlags, EOutlinerIcon::Lights) &&
            ctx.level) {
            for (std::size_t i = 0; i < ctx.level->DirectionalLights().size(); ++i) {
                if (ctx.level->DirectionalLights()[i].blueprintInstanceId != 0) {
                    continue;
                }
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "Directional light %zu", i);
                char id[32];
                (void)std::snprintf(id, sizeof(id), "dir%zu", i);
                drawLeaf(EEditorSelectionKind::DirectionalLight, i, id, labelBuf,
                         EOutlinerIcon::DirectionalLight);
            }
            for (std::size_t i = 0; i < ctx.level->PointLights().size(); ++i) {
                if (ctx.level->PointLights()[i].blueprintInstanceId != 0) {
                    continue;
                }
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "Point light %zu", i);
                char id[32];
                (void)std::snprintf(id, sizeof(id), "pt%zu", i);
                drawLeaf(EEditorSelectionKind::PointLight, i, id, labelBuf,
                         EOutlinerIcon::PointLight);
            }
            for (std::size_t i = 0; i < ctx.level->SpotLights().size(); ++i) {
                if (ctx.level->SpotLights()[i].blueprintInstanceId != 0) {
                    continue;
                }
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "Spot light %zu", i);
                char id[32];
                (void)std::snprintf(id, sizeof(id), "sp%zu", i);
                drawLeaf(EEditorSelectionKind::SpotLight, i, id, labelBuf,
                         EOutlinerIcon::SpotLight);
            }
            ImGui::TreePop();
        }
        if (OutlinerTreeNode("folder_ps", "Player starts", kFolderFlags,
                             EOutlinerIcon::PlayerStarts) &&
            ctx.level) {
            for (std::size_t i = 0; i < ctx.level->PlayerStarts().size(); ++i) {
                FormatIndexedActorLabel("PlayerStart", i, labelBuf, sizeof(labelBuf));
                char id[32];
                (void)std::snprintf(id, sizeof(id), "ps%zu", i);
                drawLeaf(EEditorSelectionKind::PlayerStart, i, id, labelBuf,
                         EOutlinerIcon::PlayerStart);
            }
            ImGui::TreePop();
        }
        if (OutlinerTreeNode("folder_tv", "Trigger volumes", kFolderFlags,
                             EOutlinerIcon::TriggerVolumes) &&
            ctx.level) {
            for (std::size_t i = 0; i < ctx.level->TriggerVolumes().size(); ++i) {
                if (ctx.level->TriggerVolumes()[i].blueprintInstanceId != 0) {
                    continue;
                }
                const TriggerVolume& vol = ctx.level->TriggerVolumes()[i];
                if (!vol.tag.empty()) {
                    (void)std::snprintf(labelBuf, sizeof(labelBuf), "%s", vol.tag.c_str());
                } else {
                    FormatIndexedActorLabel("TriggerVolume", i, labelBuf, sizeof(labelBuf));
                }
                char id[32];
                (void)std::snprintf(id, sizeof(id), "tv%zu", i);
                drawLeaf(EEditorSelectionKind::TriggerVolume, i, id, labelBuf,
                         EOutlinerIcon::TriggerVolume);
            }
            ImGui::TreePop();
        }
        if (OutlinerTreeNode("folder_pcv", "Pain volumes", kFolderFlags,
                             EOutlinerIcon::PainVolumes) &&
            ctx.level) {
            for (std::size_t i = 0; i < ctx.level->PainCausingVolumes().size(); ++i) {
                const PainCausingVolume& vol = ctx.level->PainCausingVolumes()[i];
                if (!vol.tag.empty()) {
                    (void)std::snprintf(labelBuf, sizeof(labelBuf), "%s", vol.tag.c_str());
                } else {
                    FormatIndexedActorLabel("PainCausingVolume", i, labelBuf, sizeof(labelBuf));
                }
                char id[32];
                (void)std::snprintf(id, sizeof(id), "pcv%zu", i);
                drawLeaf(EEditorSelectionKind::PainCausingVolume, i, id, labelBuf,
                         EOutlinerIcon::PainVolume);
            }
            ImGui::TreePop();
        }
        if (OutlinerTreeNode("folder_ais", "AI spawn points", kFolderFlags,
                             EOutlinerIcon::AISpawns) &&
            ctx.level) {
            for (std::size_t i = 0; i < ctx.level->AISpawnPoints().size(); ++i) {
                const AISpawnPoint& point = ctx.level->AISpawnPoints()[i];
                if (!point.tag.empty()) {
                    (void)std::snprintf(labelBuf, sizeof(labelBuf), "%s", point.tag.c_str());
                } else {
                    FormatIndexedActorLabel("AISpawnPoint", i, labelBuf, sizeof(labelBuf));
                }
                char id[32];
                (void)std::snprintf(id, sizeof(id), "ais%zu", i);
                drawLeaf(EEditorSelectionKind::AISpawnPoint, i, id, labelBuf,
                         EOutlinerIcon::AISpawn);
            }
            ImGui::TreePop();
        }
        if (OutlinerTreeNode("folder_tr", "Text renders", kFolderFlags,
                             EOutlinerIcon::TextRenderActors) &&
            ctx.level) {
            for (std::size_t i = 0; i < ctx.level->TextRenderActors().size(); ++i) {
                const TextRenderActor& tr = ctx.level->TextRenderActors()[i];
                const char* firstLine = tr.Text.c_str();
                const char* nl = std::strchr(tr.Text.c_str(), '\n');
                if (nl != nullptr && nl > tr.Text.c_str()) {
                    (void)std::snprintf(labelBuf, sizeof(labelBuf), "%.*s",
                                        static_cast<int>(nl - tr.Text.c_str()), tr.Text.c_str());
                } else if (!tr.Text.empty()) {
                    (void)std::snprintf(labelBuf, sizeof(labelBuf), "%s", firstLine);
                } else {
                    FormatIndexedActorLabel("TextRenderActor", i, labelBuf, sizeof(labelBuf));
                }
                char id[32];
                (void)std::snprintf(id, sizeof(id), "tr%zu", i);
                drawLeaf(EEditorSelectionKind::TextRenderActor, i, id, labelBuf,
                         EOutlinerIcon::TextRenderActor);
            }
            ImGui::TreePop();
        }
        if (OutlinerTreeNode("folder_bp", "Blueprints", kFolderFlags, EOutlinerIcon::Blueprint) &&
            ctx.level) {
            for (std::size_t i = 0; i < ctx.level->BlueprintInstances().size(); ++i) {
                const BlueprintInstance& bp = ctx.level->BlueprintInstances()[i];
                const char* label = bp.actorLabel.empty() ? "Blueprint" : bp.actorLabel.c_str();
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "%s", label);
                char id[32];
                (void)std::snprintf(id, sizeof(id), "bp%zu", i);
                drawLeaf(EEditorSelectionKind::BlueprintInstance, i, id, labelBuf,
                         EOutlinerIcon::Blueprint);
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    if (OutlinerTreeNode("folder_world", "World", kFolderFlags, EOutlinerIcon::World)) {
        if (ctx.world == nullptr) {
            ImGui::TextDisabled("(no World)");
        } else if (!ctx.piePlaying && ctx.world->ActorCount() == 0) {
            ImGui::TextDisabled("(start play in editor to spawn gameplay actors)");
        } else {
            std::size_t i = 0;
            ctx.world->ForEachActor([&](Actor& actor) {
                const char* pretty = PrettyTypeName(typeid(actor).name());
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "%s", pretty);
                if (!MatchesOutlinerFilter(filter, labelBuf)) {
                    ++i;
                    return;
                }

                const bool selected = ctx.selection.Equals(EEditorSelectionKind::Actor, i);
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                           ImGuiTreeNodeFlags_DefaultOpen;
                if (selected) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                char id[32];
                (void)std::snprintf(id, sizeof(id), "actor%zu", i);
                ImGui::PushID(static_cast<int>(i) + 10000);
                const bool open = OutlinerTreeNode(id, labelBuf, flags, EOutlinerIcon::Actor);
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    ctx.Select(EEditorSelectionKind::Actor, i);
                }
                if (open) {
                    DrawSceneComponentTree(actor.GetRootComponent(), 0);
                    ImGui::TreePop();
                }
                ImGui::PopID();
                ++i;
            });
            if (i == 0) {
                ImGui::TextDisabled("(empty)");
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopStyleVar(2);
    ui::EndPanel();
}

} // namespace leon::editor
