#include "Editor/Panels/FWorldSettingsPanel.hpp"
#include "Core/FLog.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/AWorldSettings.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include <imgui.h>
#include <string>

namespace Leon::Editor {

    namespace {
        FWorldSettingsComponent* FindWorldSettingsComponent(UWorld* InWorld) {
            if (!InWorld)
                return nullptr;
            for (auto& ActorRef : InWorld->GetAllActors()) {
                if (!ActorRef)
                    continue;
                if (auto* Ws = dynamic_cast<AWorldSettings*>(ActorRef.get()))
                    return &Ws->GetWorldSettings();
                if (ActorRef->HasComponent<FWorldSettingsComponent>())
                    return &ActorRef->GetComponent<FWorldSettingsComponent>();
            }
            return nullptr;
        }
    } // namespace

    void FWorldSettingsPanel::Draw(UWorld* InWorld, const std::string& InProjectDefaultGameMode, bool* bInOutOpen) {
        FEditorWidgets::BeginPanelWindow(FPanelWindowTitles::WorldSettings, bInOutOpen, ELucideIcon::Globe);

        try {
            if (!InWorld) {
                ImGui::TextDisabled("No active world loaded.");
                ImGui::End();
                return;
            }

            FWorldSettingsComponent* Ws = FindWorldSettingsComponent(InWorld);
            if (!Ws) {
                ImGui::TextDisabled("No WorldSettings actor in this map.");
                ImGui::End();
                return;
            }

            const FWorldSettingsComponent Defaults;

            if (ImGui::CollapsingHeader("GameMode", ImGuiTreeNodeFlags_DefaultOpen)) {
                FEditorWidgets::BeginPropertyGrid();

                const std::vector<std::string> GameModes =
                    UClassRegistry::Get().GetRegisteredClassNamesContaining("GameMode");
                const std::string ProjectDefault =
                    InProjectDefaultGameMode.empty() ? "AGameModeBase" : InProjectDefaultGameMode;
                const std::string NoneLabel = "None (" + ProjectDefault + ")";
                static const std::string EmptyDefault;

                FEditorWidgets::DrawPropertyClassSelect("GameMode Override", "##GameModeClass", Ws->GameModeClass,
                                                        GameModes, NoneLabel.c_str(), &EmptyDefault);

                FEditorWidgets::EndPropertyGrid();
                ImGui::TextDisabled("None = project DefaultGameMode. Override to force a GameMode for this map.");
            }

            if (ImGui::CollapsingHeader("Lightmass / Static Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                FEditorWidgets::BeginPropertyGrid();
                FEditorWidgets::DrawPropertyCheckbox("Enable Static Lighting", "##EnableStaticLighting",
                                                     &Ws->bStaticLighting, nullptr, &Defaults.bStaticLighting);
                int LightmapRes = static_cast<int>(Ws->LightmapResolution);
                const int DefaultLightmapRes = static_cast<int>(Defaults.LightmapResolution);
                if (FEditorWidgets::DrawPropertyDragInt("Default Lightmap Resolution", "##LightmapRes", &LightmapRes,
                                                        16, 32, 4096, nullptr, &DefaultLightmapRes)) {
                    Ws->LightmapResolution = static_cast<uint32_t>(LightmapRes);
                }
                FEditorWidgets::EndPropertyGrid();
            }
        } catch (const std::exception& e) {
            LE_CORE_ERROR("FWorldSettingsPanel: Exception during Draw: {}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "World Settings Error: %s", e.what());
        } catch (...) {
            LE_CORE_ERROR("FWorldSettingsPanel: Unknown exception during Draw");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "World Settings: Unknown error encountered");
        }

        ImGui::End();
    }

} // namespace Leon::Editor
