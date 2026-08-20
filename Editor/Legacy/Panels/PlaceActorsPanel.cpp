#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <leon/editor/EditorDisplayNames.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorCatalogs.h>
#include <leon/editor/EditorCommands.h>
#include <leon/editor/EditorFileDialog.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/PlaceActorsPanel.h>
#include <leon/editor/ui/UiKit.h>

namespace leon::editor {
namespace {

[[nodiscard]] ELucideIcon LucideForPlaceKind(EEditorPlaceActorsKind kind) {
    switch (kind) {
    case EEditorPlaceActorsKind::Cube:
    case EEditorPlaceActorsKind::Sphere:
    case EEditorPlaceActorsKind::Plane:
    case EEditorPlaceActorsKind::Cylinder:
        return ELucideIcon::Box;
    case EEditorPlaceActorsKind::BlockingVolume:
        return ELucideIcon::Hexagon;
    case EEditorPlaceActorsKind::PointLight:
        return ELucideIcon::Lightbulb;
    case EEditorPlaceActorsKind::SpotLight:
        return ELucideIcon::Sun;
    case EEditorPlaceActorsKind::DirectionalLight:
        return ELucideIcon::Sun;
    case EEditorPlaceActorsKind::TriggerVolume:
        return ELucideIcon::Zap;
    case EEditorPlaceActorsKind::PainCausingVolume:
        return ELucideIcon::Flame;
    case EEditorPlaceActorsKind::PlayerStart:
        return ELucideIcon::User;
    case EEditorPlaceActorsKind::AISpawnPoint:
        return ELucideIcon::Crosshair;
    case EEditorPlaceActorsKind::TextRenderActor:
        return ELucideIcon::FileText;
    case EEditorPlaceActorsKind::BlueprintClass:
        return ELucideIcon::Braces;
    case EEditorPlaceActorsKind::None:
    default:
        return ELucideIcon::Plus;
    }
}

[[nodiscard]] bool MatchesPlaceFilter(const char* filter, const char* text) {
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

bool SelectablePlaceClass(EditorContext& ctx, EEditorPlaceActorsKind kind, const char* label,
                          const char* filter, const std::string& blueprintPath = {}) {
    if (!MatchesPlaceFilter(filter, label)) {
        return false;
    }
    const bool selected =
        ctx.placeActorsKind == kind && (kind != EEditorPlaceActorsKind::BlueprintClass ||
                                        ctx.placeActorsBlueprintPath == blueprintPath);
    const float iconSz = ImGui::GetTextLineHeight();
    const float rowH = ImGui::GetFrameHeight();
    ImGui::PushID(label);
    const bool clicked = ImGui::Selectable(" ##place", selected, 0, ImVec2(0.0f, rowH));
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float y = min.y + (max.y - min.y - iconSz) * 0.5f;
    const ImU32 iconCol = ui::ToU32(ui::Tokens().text);
    DrawLucideIcon(draw, ImVec2(min.x + 2.0f, y), ImVec2(min.x + 2.0f + iconSz, y + iconSz),
                   LucideForPlaceKind(kind), iconCol);
    draw->AddText(ImVec2(min.x + 2.0f + iconSz + 6.0f,
                         min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f),
                  ImGui::GetColorU32(ImGuiCol_Text), label);
    ImGui::PopID();
    if (clicked) {
        ctx.BeginPlaceActors(kind, blueprintPath);
        return true;
    }
    return false;
}

[[nodiscard]] bool CategoryHasMatches(const char* filter,
                                      std::initializer_list<const char*> labels) {
    if (filter == nullptr || filter[0] == '\0') {
        return true;
    }
    for (const char* label : labels) {
        if (MatchesPlaceFilter(filter, label)) {
            return true;
        }
    }
    return false;
}

} // namespace

void PlaceActorsPanel::RefreshBlueprints(const EditorContext& ctx) {
    if (ctx.requestContentRefresh) {
        blueprintsProjectKey_.clear();
    }
    if (!blueprintsProjectKey_.empty() && blueprintsProjectKey_ == ctx.projectPath) {
        return;
    }
    blueprintsProjectKey_ = ctx.projectPath;
    blueprints_ = CollectEditorBlueprints(ctx.projectPath);
}

void PlaceActorsPanel::DrawCategoryBasic(EditorContext& ctx, const char* filter) {
    if (!CategoryHasMatches(filter, {"Cube", "Sphere", "Plane", "Cylinder", "Blocking volume",
                                     "Player start",
                                     "Text render"})) {
        return;
    }
    if (!ui::CollapsingSection("Basic")) {
        return;
    }
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::Cube, PlaceActorsKindLabel(EEditorPlaceActorsKind::Cube),
                         filter);
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::Sphere, PlaceActorsKindLabel(EEditorPlaceActorsKind::Sphere),
                         filter);
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::Plane, PlaceActorsKindLabel(EEditorPlaceActorsKind::Plane),
                         filter);
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::Cylinder,
                         PlaceActorsKindLabel(EEditorPlaceActorsKind::Cylinder), filter);
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::BlockingVolume,
                         PlaceActorsKindLabel(EEditorPlaceActorsKind::BlockingVolume), filter);
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::PlayerStart,
                         PlaceActorsKindLabel(EEditorPlaceActorsKind::PlayerStart), filter);
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::TextRenderActor,
                         PlaceActorsKindLabel(EEditorPlaceActorsKind::TextRenderActor), filter);
}

void PlaceActorsPanel::DrawCategoryLights(EditorContext& ctx, const char* filter) {
    if (!CategoryHasMatches(filter, {"Point light", "Spot light", "Directional light"})) {
        return;
    }
    if (!ui::CollapsingSection("Lights")) {
        return;
    }
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::PointLight,
                         PlaceActorsKindLabel(EEditorPlaceActorsKind::PointLight), filter);
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::SpotLight,
                         PlaceActorsKindLabel(EEditorPlaceActorsKind::SpotLight), filter);
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::DirectionalLight,
                         PlaceActorsKindLabel(EEditorPlaceActorsKind::DirectionalLight), filter);
}

void PlaceActorsPanel::DrawCategoryVolumes(EditorContext& ctx, const char* filter) {
    if (!CategoryHasMatches(filter, {"Trigger volume", "Pain volume", "AI spawn point"})) {
        return;
    }
    if (!ui::CollapsingSection("Volumes")) {
        return;
    }
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::TriggerVolume,
                         PlaceActorsKindLabel(EEditorPlaceActorsKind::TriggerVolume), filter);
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::PainCausingVolume,
                         PlaceActorsKindLabel(EEditorPlaceActorsKind::PainCausingVolume), filter);
    SelectablePlaceClass(ctx, EEditorPlaceActorsKind::AISpawnPoint,
                         PlaceActorsKindLabel(EEditorPlaceActorsKind::AISpawnPoint), filter);
}

void PlaceActorsPanel::DrawCategoryBlueprints(EditorContext& ctx, const char* filter) {
    RefreshBlueprints(ctx);
    bool hasBlueprintMatch = MatchesPlaceFilter(filter, "Browse Blueprint Class");
    if (!hasBlueprintMatch) {
        for (const EditorBlueprintEntry& bp : blueprints_) {
            if (MatchesPlaceFilter(filter, bp.displayName.c_str())) {
                hasBlueprintMatch = true;
                break;
            }
        }
    }
    if (!hasBlueprintMatch) {
        return;
    }
    if (!ui::CollapsingSection("Blueprints")) {
        return;
    }
    if (blueprints_.empty()) {
        ui::Hint("No blueprint assets under Content/Blueprints.");
    } else {
        for (const EditorBlueprintEntry& bp : blueprints_) {
            SelectablePlaceClass(ctx, EEditorPlaceActorsKind::BlueprintClass,
                                 bp.displayName.c_str(), filter, bp.authoringPath);
        }
    }
    if (MatchesPlaceFilter(filter, "Browse Blueprint Class")) {
        ui::UiButtonDesc browse;
        browse.label = "Browse Blueprint Class…";
        browse.icon = ui::UiIcon(ELucideIcon::FolderOpen);
        browse.variant = ui::EUiVariant::Secondary;
        browse.size = ui::EUiSize::Sm;
        if (ui::Button("##browse_bp_class", browse)) {
            const std::string picked =
                EditorPickOpenFile("Leon Blueprint\0*.lbp\0All\0*.*\0", "Place Blueprint Class");
            if (!picked.empty()) {
                const std::string rel = MakePackRelativeAssetPath(ctx, picked);
                ctx.BeginPlaceActors(EEditorPlaceActorsKind::BlueprintClass,
                                     rel.empty() ? picked : rel);
            }
        }
    }
}

void PlaceActorsPanel::DrawCategoryRecent(EditorContext& ctx, const char* filter) {
    if (ctx.placeActorsRecent.empty()) {
        return;
    }
    bool hasRecentMatch = false;
    for (const PlaceActorsRecentEntry& e : ctx.placeActorsRecent) {
        const std::string label = e.label.empty() ? PlaceActorsKindLabel(e.kind) : e.label;
        if (MatchesPlaceFilter(filter, label.c_str())) {
            hasRecentMatch = true;
            break;
        }
    }
    if (!hasRecentMatch) {
        return;
    }
    if (!ui::CollapsingSection("Recent")) {
        return;
    }
    for (std::size_t i = 0; i < ctx.placeActorsRecent.size(); ++i) {
        const PlaceActorsRecentEntry& e = ctx.placeActorsRecent[i];
        ImGui::PushID(static_cast<int>(i));
        const std::string label = e.label.empty() ? PlaceActorsKindLabel(e.kind) : e.label;
        SelectablePlaceClass(ctx, e.kind, label.c_str(), filter, e.blueprintPath);
        ImGui::PopID();
    }
}

void PlaceActorsPanel::Draw(EditorContext& ctx) {
    if (!ctx.showPlaceActors) {
        return;
    }
    if (ctx.requestFocusPlaceActors) {
        ImGui::SetNextWindowFocus();
        ctx.requestFocusPlaceActors = false;
    }
    if (!ui::BeginPanel("Place Actors", {.pOpen = &ctx.showPlaceActors})) {
        ui::EndPanel();
        return;
    }

    ui::SectionLabel("Modes");
    ui::Hint("Select a class, then LMB in the Viewport. Esc cancels.");
    if (ctx.IsPlaceActorsActive()) {
        ImGui::Text("Placing: %s", ctx.placeActorsKind == EEditorPlaceActorsKind::BlueprintClass
                                       ? (ctx.placeActorsBlueprintPath.empty()
                                              ? "Blueprint class"
                                              : ctx.placeActorsBlueprintPath.c_str())
                                       : PlaceActorsKindLabel(ctx.placeActorsKind));
        if (ui::Button("##cancel_place", "Cancel Place", ui::EUiVariant::Destructive,
                       ui::EUiSize::Sm)) {
            ctx.ClearPlaceActors();
        }
    } else {
        ImGui::TextDisabled("No active place class.");
    }

    (void)ui::SearchField("##place_search", searchBuf_, sizeof(searchBuf_), "Search classes…");

    const char* filter = searchBuf_;
    ui::Separator();

    DrawCategoryBasic(ctx, filter);
    DrawCategoryLights(ctx, filter);
    DrawCategoryVolumes(ctx, filter);
    DrawCategoryBlueprints(ctx, filter);
    DrawCategoryRecent(ctx, filter);

    if (filter[0] != '\0' &&
        !CategoryHasMatches(filter,
                            {"Cube", "Sphere", "Plane", "Blocking volume", "Player start",
                             "Text render", "Point light", "Spot light", "Directional light",
                             "Trigger volume", "Pain volume", "AI spawn point"})) {
        RefreshBlueprints(ctx);
        bool blueprintMatch = MatchesPlaceFilter(filter, "Browse Blueprint Class");
        for (const EditorBlueprintEntry& bp : blueprints_) {
            if (MatchesPlaceFilter(filter, bp.displayName.c_str())) {
                blueprintMatch = true;
                break;
            }
        }
        for (const PlaceActorsRecentEntry& e : ctx.placeActorsRecent) {
            const std::string label = e.label.empty() ? PlaceActorsKindLabel(e.kind) : e.label;
            if (MatchesPlaceFilter(filter, label.c_str())) {
                blueprintMatch = true;
                break;
            }
        }
        if (!blueprintMatch) {
            ui::Hint("No matching place classes.");
        }
    }

    ui::EndPanel();
}

} // namespace leon::editor
