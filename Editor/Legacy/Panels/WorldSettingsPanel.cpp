#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <iostream>
#include <leon/core/Paths.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/EditorProject.h>
#include <leon/editor/EngineContent.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/WorldSettingsPanel.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/level/Level.h>
#include <leon/render/PostProcess.h>
#include <leon/render/Renderer.h>
#include <leon/render/ResourceCache.h>
#include <leon/render/Scalability.h>

namespace leon::editor {
namespace {

[[nodiscard]] std::string BasenameLabel(const std::string& path) {
    if (path.empty()) {
        return "(none)";
    }
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

void PersistEditorScalability(EditorContext& ctx) {
    if (ctx.projectPath.empty() || ctx.renderer == nullptr || ctx.resources == nullptr) {
        return;
    }
    std::string err;
    if (!EditorProjectService::WriteProjectEditorScalability(
            ctx.projectPath, ctx.renderer->GetPostProcessSettings(),
            ctx.resources->GetTextureQuality(), err)) {
        std::cerr << "Editor: failed to save DefaultScalability.ini: " << err << '\n';
    }
}

} // namespace

void WorldSettingsPanel::RefreshCatalogs(EditorContext& ctx) {
    if (ctx.requestContentRefresh) {
        catalogsProjectKey_.clear();
    }
    if (!catalogsProjectKey_.empty() && catalogsProjectKey_ == ctx.projectPath) {
        return;
    }
    catalogsProjectKey_ = ctx.projectPath;
    skyboxes_ = CollectEditorSkyboxes(ctx.projectPath);
}

void WorldSettingsPanel::Draw(EditorContext& ctx) {
    if (!ctx.showWorldSettings) {
        return;
    }
    if (!ui::BeginPanel("World Settings", {.pOpen = &ctx.showWorldSettings})) {
        ui::EndPanel();
        return;
    }

    RefreshCatalogs(ctx);

    if (ctx.level == nullptr || ctx.resources == nullptr) {
        ui::Hint("Open a level to edit World Settings");
        ui::EndPanel();
        return;
    }

    Level& level = *ctx.level;
    ui::PushFormLayout();

    if (ui::CollapsingSection("Level")) {
        char nameBuf[128];
        (void)std::snprintf(nameBuf, sizeof(nameBuf), "%s", level.Name().c_str());
        ui::FieldLabel("Name");
        if (ui::InputText("##level_name", nullptr, nameBuf, sizeof(nameBuf))) {
            level.SetName(nameBuf);
            ctx.MarkDirty();
        }
        if (ImGui::IsItemActivated() && ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }

        const std::vector<std::string> modes = ListEditorGameModes(ctx.projectPath);
        std::string current = level.GameMode();
        const std::string inheritId = ctx.projectGlobalDefaultGameMode.empty()
                                          ? std::string{"Default"}
                                          : ctx.projectGlobalDefaultGameMode;
        const std::string comboCurrent = current.empty() ? inheritId : current;
        bool currentKnown = false;
        for (const std::string& mode : modes) {
            if (mode == comboCurrent) {
                currentKnown = true;
                break;
            }
        }
        const std::string comboPreview =
            current.empty() ? (inheritId + " (project default)")
                            : (currentKnown ? comboCurrent : (comboCurrent + " (custom)"));
        ui::SectionLabel("Game mode override");
        ui::UiSelectDesc modeDesc;
        modeDesc.preview = comboPreview.c_str();
        modeDesc.previewIcon = ui::UiIcon(ELucideIcon::Cpu);
        modeDesc.width = -1.0f;
        if (ui::BeginSelect("##GameModeOverride", modeDesc)) {
            for (const std::string& mode : modes) {
                const bool selected = (!current.empty() && mode == current);
                if (ui::SelectItem(mode.c_str(), selected)) {
                    if (ctx.history != nullptr) {
                        ctx.history->Capture(ctx);
                    }
                    level.SetGameMode(mode);
                    ctx.MarkDirty();
                }
            }
            ui::EndSelect();
        }
        ui::Hint(
            "Empty override uses project default game mode. Default = free look. "
            "third-person = character mode in play in editor. "
            "Product packs play via build game / shipping — not linked into the editor.");

        char gmBuf[128];
        (void)std::snprintf(gmBuf, sizeof(gmBuf), "%s", level.GameMode().c_str());
        ui::FieldLabel("Custom ID");
        if (ui::InputText("##custom_gm", nullptr, gmBuf, sizeof(gmBuf))) {
            level.SetGameMode(gmBuf);
            ctx.MarkDirty();
        }
        if (ImGui::IsItemActivated() && ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
    }

    if (ui::CollapsingSection("Environment")) {
        const std::string currentSky = level.EnvironmentPath();
        const std::string skyPreview = BasenameLabel(currentSky);
        ui::SectionLabel("Skybox");
        ui::UiSelectDesc skyDesc;
        skyDesc.preview = skyPreview.c_str();
        skyDesc.previewIcon = ui::UiIcon(ELucideIcon::Globe);
        skyDesc.width = -1.0f;
        if (ui::BeginSelect("##Skybox", skyDesc)) {
            const bool noneSelected = currentSky.empty();
            if (ui::SelectItem("(none)", noneSelected)) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                level.SetEnvironment(nullptr);
                level.SetEnvironmentPath({});
                ctx.MarkDirty();
            }
            for (const EditorSkyboxEntry& entry : skyboxes_) {
                const bool selected =
                    entry.authoringPath == currentSky || entry.absolutePath == currentSky ||
                    BasenameLabel(entry.authoringPath) == BasenameLabel(currentSky);
                if (ui::SelectItem(entry.displayName.c_str(), selected,
                                   ui::UiIcon(ELucideIcon::Image))) {
                    if (ctx.history != nullptr) {
                        ctx.history->Capture(ctx);
                    }
                    const std::string loadPath = entry.absolutePath.empty()
                                                     ? ResolveAssetPath(entry.authoringPath)
                                                     : entry.absolutePath;
                    auto env = ctx.resources->LoadEnvMap(loadPath);
                    if (env) {
                        level.SetEnvironment(std::move(env));
                        level.SetEnvironmentPath(
                            MakePackRelativeAssetPath(ctx, entry.authoringPath));
                        ctx.MarkDirty();
                    }
                }
            }
            ui::EndSelect();
        }

        float exposure = level.EnvironmentExposure();
        if (ui::SliderFloat("##env_exposure", "Exposure", &exposure, 0.05f, 8.0f, "%.2f")) {
            if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            level.SetEnvironmentExposure(exposure);
            ctx.MarkDirty();
        }

        if (ctx.renderer != nullptr) {
            ImGui::SeparatorText("Graphics");
            ui::Hint(
                "Graphics preset: Base (forward lit) → + Shadows (directional map) → "
                "+ Post (HDR tonemap + planar mirror). Stored in Config/DefaultScalability.ini.");
            PostProcessSettings& post = ctx.renderer->GetPostProcessSettings();
            int quality = static_cast<int>(post.quality);
            const char* qualities[] = {"Base", "+ Shadows", "+ Post"};
            ui::FieldLabel("Graphics Preset");
            if (ui::SelectFromList("##pp_quality", &quality, qualities, 3, ui::EUiSize::Sm,
                                   -1.0f)) {
                ctx.renderer->SetPostProcessQuality(static_cast<EPostProcessQuality>(quality));
                PersistEditorScalability(ctx);
            }

            int textureQuality = static_cast<int>(ctx.resources->GetTextureQuality().quality);
            const char* textureQualities[] = {"Low", "Medium", "High"};
            ui::FieldLabel("Textures");
            if (ui::SelectFromList("##tex_quality", &textureQuality, textureQualities, 3,
                                   ui::EUiSize::Sm, -1.0f)) {
                TextureQualitySettings tq = ctx.resources->GetTextureQuality();
                ApplyTextureQuality(tq, static_cast<ETextureQuality>(textureQuality));
                ctx.resources->SetTextureQuality(tq);
                PersistEditorScalability(ctx);
            }
        }

        if (ctx.requestPickHdr && !ctx.pendingHdrPickPath.empty()) {
            if (ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            const std::string path = MakePackRelativeAssetPath(ctx, ctx.pendingHdrPickPath);
            ctx.pendingHdrPickPath.clear();
            ctx.requestPickHdr = false;
            auto env = ctx.resources->LoadEnvMap(ResolveAssetPath(path));
            if (!env) {
                env = ctx.resources->LoadEnvMap(path);
            }
            if (env) {
                level.SetEnvironment(std::move(env));
                level.SetEnvironmentPath(path);
                ctx.MarkDirty();
                catalogsProjectKey_.clear();
            }
        }
        {
            ui::UiButtonDesc pickDesc;
            pickDesc.label = "Pick HDR from Content…";
            pickDesc.icon = ui::UiIcon(ELucideIcon::FolderOpen);
            pickDesc.variant = ui::EUiVariant::Secondary;
            pickDesc.size = ui::EUiSize::Md;
            if (ui::Button("##pick_hdr", pickDesc)) {
                ctx.BeginPickHdr();
            }
        }
    }

    ui::PopFormLayout();
    ui::EndPanel();
}

} // namespace leon::editor
