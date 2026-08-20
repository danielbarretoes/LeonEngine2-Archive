#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/ToolbarPanel.h>
#include <leon/editor/PieAspectFit.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/net/NetProtocol.h>
#include <string>

namespace leon::editor {
namespace {

const char* PlayNetModeLabel(EEditorPlayNetMode mode) {
    switch (mode) {
    case EEditorPlayNetMode::ListenServer:
        return "Listen Server";
    case EEditorPlayNetMode::Client:
        return "Client";
    case EEditorPlayNetMode::Standalone:
    default:
        return "Standalone";
    }
}

} // namespace

void ToolbarPanel::Draw(EditorContext& ctx) {
    ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 36.0f), ImVec2(FLT_MAX, 64.0f));
    if (!ui::BeginPanel("Toolbar",
                        {.extraFlags = ImGuiWindowFlags_NoCollapse, .compactPadding = true})) {
        ui::EndPanel();
        return;
    }

    DrawPlayControls(ctx);

    ImGui::SameLine();
    ui::Separator(true);
    ImGui::SameLine();

    const ui::EUiVariant buildVariant =
        ctx.lightingOutOfDate ? ui::EUiVariant::Warning : ui::EUiVariant::Ghost;
    if (ui::ToolbarButton("##build_lights", ELucideIcon::Sun, "Build Lighting",
                          ctx.lightingOutOfDate
                              ? "Lighting out of date — build lighting to refresh baked map data"
                              : "Build Lighting",
                          buildVariant, ui::EUiSize::Md)) {
        ctx.requestBuildLights = true;
    }
    if (ctx.lightingOutOfDate) {
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::TextColored(ui::Tokens().warningHover, "Out of date");
    }
    if (!ctx.buildLightsStatus.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", ctx.buildLightsStatus.c_str());
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|  %s%s", ctx.levelPath.empty() ? "(unsaved)" : ctx.levelPath.c_str(),
                        ctx.dirty ? " *" : "");

    ui::EndPanel();
}

void ToolbarPanel::DrawPlayControls(EditorContext& ctx) {
    if (!ctx.piePlaying) {
        if (ui::ToolbarButton("##pie_play", ELucideIcon::Play, "Play", "Start play in editor",
                              ui::EUiVariant::Primary, ui::EUiSize::Md)) {
            ctx.requestPieStart = true;
        }
        ImGui::SameLine(0.0f, 4.0f);
        if (ui::IconButton("##pie_settings", ELucideIcon::Settings, ui::EUiVariant::Ghost,
                           ui::EUiSize::Md,
                           "Play Settings — Viewport/Window, Aspect, Players, Net Mode")) {
            ImGui::OpenPopup("##PlaySettingsPopup");
        }
        if (ui::BeginPopupPanel("##PlaySettingsPopup", 260.0f)) {
            ui::PushFormLayout();
            ui::SectionLabel("Play Mode");
            int playModeIdx = ctx.playMode == EEditorPlayMode::NewEditorWindow ? 1 : 0;
            const char* playModes[] = {"Selected Viewport", "New Window"};
            if (ui::SegmentedControl("##PlayModeSeg", &playModeIdx, playModes, 2)) {
                ctx.playMode = playModeIdx == 1 ? EEditorPlayMode::NewEditorWindow
                                                : EEditorPlayMode::SelectedViewport;
            }

            ui::SectionLabel("Aspect Ratio");
            ui::UiSelectDesc aspectDesc;
            aspectDesc.preview = PieAspectLabel(ctx.pieAspect);
            aspectDesc.width = -1.0f;
            if (ui::BeginSelect("##PieAspect", aspectDesc)) {
                const EEditorPieAspect aspects[] = {
                    EEditorPieAspect::Fit16x9, EEditorPieAspect::Fit16x10,
                    EEditorPieAspect::Fit4x3,  EEditorPieAspect::Fit1x1,
                    EEditorPieAspect::Fit9x16, EEditorPieAspect::Stretch,
                };
                for (const EEditorPieAspect a : aspects) {
                    if (ui::SelectItem(PieAspectLabel(a), ctx.pieAspect == a)) {
                        ctx.pieAspect = a;
                    }
                }
                ui::EndSelect();
            }

            ui::SectionLabel("Number of Players");
            int players = ctx.pieNumberOfPlayers;
            if (ui::DragInt("##PiePlayers", nullptr, &players, 0.08f, 1, leon::net::kMaxPlayers,
                            -1.0f)) {
                ctx.pieNumberOfPlayers = std::clamp(players, 1, leon::net::kMaxPlayers);
            }

            ui::SectionLabel("Net Mode");
            int netIdx = 0;
            switch (ctx.pieNetMode) {
            case EEditorPlayNetMode::ListenServer:
                netIdx = 1;
                break;
            case EEditorPlayNetMode::Client:
                netIdx = 2;
                break;
            case EEditorPlayNetMode::Standalone:
            default:
                netIdx = 0;
                break;
            }
            const char* netModes[] = {"Standalone", "Listen", "Client"};
            if (ui::SegmentedControl("##PieNetSeg", &netIdx, netModes, 3)) {
                ctx.pieNetMode = netIdx == 1   ? EEditorPlayNetMode::ListenServer
                                 : netIdx == 2 ? EEditorPlayNetMode::Client
                                               : EEditorPlayNetMode::Standalone;
            }
            if (ctx.pieNetMode == EEditorPlayNetMode::Client) {
                char addr[64];
                (void)std::snprintf(addr, sizeof(addr), "%s", ctx.pieClientAddress.c_str());
                ui::FieldLabel("Join Address");
                if (ui::InputText("##join_addr", nullptr, addr, sizeof(addr),
                                  {.hint = "127.0.0.1"})) {
                    ctx.pieClientAddress = addr;
                }
            }
            ui::PopFormLayout();
            ui::EndPopupPanel();
        }
        return;
    }

    if (ui::ToolbarButton("##pie_pause", ctx.piePaused ? ELucideIcon::Play : ELucideIcon::Pause,
                          ctx.piePaused ? "Resume" : "Pause")) {
        ctx.requestPiePauseToggle = true;
    }
    ImGui::SameLine();
    if (ui::ToolbarButton("##pie_stop", ELucideIcon::Square, "Stop", "Stop play in editor",
                          ui::EUiVariant::Destructive)) {
        ctx.requestPieStop = true;
    }
    ImGui::SameLine();
    ImGui::TextColored(ctx.piePaused ? ui::Tokens().warningHover : ui::Tokens().success,
                       ctx.piePaused ? "PAUSED" : "PLAYING");
    ImGui::SameLine();
    ImGui::TextDisabled("%s x%d", PlayNetModeLabel(ctx.pieNetMode), ctx.pieNumberOfPlayers);
}

} // namespace leon::editor
