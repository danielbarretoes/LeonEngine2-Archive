#include "Engine/FMapSerializer.hpp"
#include "FMapSerializerInternals.hpp"
#include "Core/FLog.hpp"
#include "Core/FStringUtils.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ACameraActor.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Gameplay/AWorldSettings.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Engine/Components.hpp"
#include "RHI/IRenderDriver.hpp"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace Leon {

    FMapSerializer::FMapSerializer(const TRef<UWorld>& InWorld) : World(InWorld) {}

    bool FMapSerializer::Serialize(const std::string& InFilePath) {
        std::string yaml;
        if (!SerializeText(yaml)) {
            return false;
        }

        std::ofstream fout(InFilePath);
        if (!fout.is_open()) {
            LE_CORE_ERROR("FMapSerializer: Failed to open file for writing: {0}", InFilePath);
            return false;
        }

        fout << yaml;
        fout.close();
        LE_CORE_INFO("FMapSerializer: Successfully saved Map to '{0}'", InFilePath);
        return true;
    }

    bool FMapSerializer::SerializeText(std::string& OutYamlString) {
        if (!World) {
            LE_CORE_ERROR("FMapSerializer: Attempted to serialize a null UWorld!");
            return false;
        }

        std::stringstream ss;
        ss << "# LeonEngine2 Map Asset File (.lmap)\n";
        ss << "# Human-Readable & Deterministic Serialization\n";
        ss << "Map:\n";
        Indent(ss, 1);
        ss << "Name: \"" << World->GetName() << "\"\n";
        Indent(ss, 1);
        ss << "Version: \"2.1\"\n\n";

        // 1. Environment / Global Settings
        ss << "Environment:\n";
        const FWorldSettingsComponent* worldSettings = nullptr;
        const FSkyboxComponent* skyboxComp = nullptr;
        for (const auto& actorRef : World->GetAllActors()) {
            if (!actorRef)
                continue;
            if (!worldSettings && actorRef->HasComponent<FWorldSettingsComponent>())
                worldSettings = &actorRef->GetComponent<FWorldSettingsComponent>();
            if (!skyboxComp && actorRef->HasComponent<FSkyboxComponent>())
                skyboxComp = &actorRef->GetComponent<FSkyboxComponent>();
        }
        if (worldSettings) {
            WriteWorldSettings(ss, *worldSettings);
        }
        if (skyboxComp) {
            const auto& sky = *skyboxComp;
            Indent(ss, 1);
            ss << "Skybox:\n";
            Indent(ss, 2);
            ss << "Enabled: " << (sky.bEnabled ? "true" : "false") << "\n";
            Indent(ss, 2);
            ss << "Exposure: " << sky.Exposure << "\n";
            Indent(ss, 2);
            ss << "SunIntensity: " << sky.SunIntensity << "\n";
            Indent(ss, 2);
            ss << "EnvironmentIntensity: " << sky.EnvironmentIntensity << "\n";
            Indent(ss, 2);
            ss << "UseHDREnvironmentMap: " << (sky.bUseHDREnvironmentMap ? "true" : "false") << "\n";
            Indent(ss, 2);
            ss << "HDREnvironmentMap: \"" << sky.HDREnvironmentMapPath << "\"\n";
            Indent(ss, 2);
            ss << "SkyZenithColor: [" << sky.SkyZenithColor.r << ", " << sky.SkyZenithColor.g << ", "
               << sky.SkyZenithColor.b << "]\n";
            Indent(ss, 2);
            ss << "HorizonColor: [" << sky.HorizonColor.r << ", " << sky.HorizonColor.g << ", " << sky.HorizonColor.b
               << "]\n";
            Indent(ss, 2);
            ss << "GroundColor: [" << sky.GroundColor.r << ", " << sky.GroundColor.g << ", " << sky.GroundColor.b
               << "]\n";
            Indent(ss, 2);
            ss << "SunColor: [" << sky.SunColor.r << ", " << sky.SunColor.g << ", " << sky.SunColor.b << "]\n";
        } else {
            Indent(ss, 1);
            ss << "Skybox:\n";
            Indent(ss, 2);
            ss << "Enabled: false\n";
        }

        // 2. Actors Collection
        ss << "\nActors:\n";

        for (const auto& actorRef : World->GetAllActors()) {
            if (!actorRef)
                continue;
            const AActor& entity = *actorRef;

            if (entity.HasComponent<FSkyboxComponent>() || entity.HasComponent<FWorldSettingsComponent>()) {
                continue;
            }
            if (IsRuntimeFrameworkActor(entity)) {
                continue;
            }

            Indent(ss, 1);
            ss << "- Name: \""
               << (entity.HasComponent<FTagComponent>() ? entity.GetComponent<FTagComponent>().Tag : entity.GetName())
               << "\"\n";
            Indent(ss, 2);
            ss << "Class: \"" << entity.GetClass() << "\"\n";
            Indent(ss, 2);
            ss << "GUID: \"" << entity.GetActorGuid().ToString() << "\"\n";
            if (const auto* start = dynamic_cast<const APlayerStart*>(&entity)) {
                if (!start->GetPlayerStartTag().empty()) {
                    Indent(ss, 2);
                    ss << "PlayerStartTag: \"" << start->GetPlayerStartTag() << "\"\n";
                }
                if (start->GetTeamIndex() != 0) {
                    Indent(ss, 2);
                    ss << "TeamIndex: " << start->GetTeamIndex() << "\n";
                }
            }

            // Transform
            if (entity.HasComponent<FTransformComponent>()) {
                const auto& tc = entity.GetComponent<FTransformComponent>();
                Indent(ss, 2);
                ss << "Transform:\n";
                Indent(ss, 3);
                ss << "Translation: [" << tc.Translation.x << ", " << tc.Translation.y << ", " << tc.Translation.z
                   << "]\n";
                Indent(ss, 3);
                ss << "Rotation: [" << tc.Rotation.x << ", " << tc.Rotation.y << ", " << tc.Rotation.z << "]\n";
                Indent(ss, 3);
                ss << "Scale: [" << tc.Scale.x << ", " << tc.Scale.y << ", " << tc.Scale.z << "]\n";
            }

            // Camera Component
            if (entity.HasComponent<FCameraComponent>()) {
                const auto& cam = entity.GetComponent<FCameraComponent>();
                Indent(ss, 2);
                ss << "Camera:\n";
                Indent(ss, 3);
                ss << "Primary: " << (cam.bPrimary ? "true" : "false") << "\n";
                Indent(ss, 3);
                ss << "FOV: " << cam.Camera.GetFOV() << "\n";
                Indent(ss, 3);
                ss << "NearClip: " << cam.Camera.GetNearClip() << "\n";
                Indent(ss, 3);
                ss << "FarClip: " << cam.Camera.GetFarClip() << "\n";
            }

            // Static Mesh Component (Asset Based)
            if (entity.HasComponent<FStaticMeshComponent>()) {
                const auto& smc = entity.GetComponent<FStaticMeshComponent>();
                Indent(ss, 2);
                ss << "StaticMesh:\n";
                Indent(ss, 3);
                ss << "Asset: \"" << smc.AssetPath << "\"\n";
                Indent(ss, 3);
                ss << "CastShadows: " << (smc.bCastShadows ? "true" : "false") << "\n";
                Indent(ss, 3);
                ss << "ReceiveShadows: " << (smc.bReceiveShadows ? "true" : "false") << "\n";
                Indent(ss, 3);
                ss << "VisibleInReflection: " << (smc.bVisibleInReflection ? "true" : "false") << "\n";
                Indent(ss, 3);
                ss << "Mobility: " << ComponentMobilityToString(smc.Mobility) << "\n";
                Indent(ss, 3);
                ss << "LightmapResolution: " << smc.LightmapResolution << "\n";
                if (smc.LightmapIndex >= 0) {
                    Indent(ss, 3);
                    ss << "LightmapIndex: " << smc.LightmapIndex << "\n";
                    Indent(ss, 3);
                    ss << "LightmapScale: [" << smc.LightmapScale.x << ", " << smc.LightmapScale.y << "]\n";
                    Indent(ss, 3);
                    ss << "LightmapBias: [" << smc.LightmapBias.x << ", " << smc.LightmapBias.y << "]\n";
                }
                if (!smc.LightmapAssetPath.empty()) {
                    Indent(ss, 3);
                    ss << "LightmapAsset: \"" << smc.LightmapAssetPath << "\"\n";
                }

                if (!smc.MaterialOverridePaths.empty()) {
                    Indent(ss, 3);
                    ss << "MaterialOverrides:\n";
                    for (size_t slot = 0; slot < smc.MaterialOverridePaths.size(); ++slot) {
                        if (!smc.MaterialOverridePaths[slot].empty()) {
                            Indent(ss, 4);
                            ss << "- Slot: " << slot << "\n";
                            Indent(ss, 5);
                            ss << "Asset: \"" << smc.MaterialOverridePaths[slot] << "\"\n";
                        }
                    }
                }
            }

            // Procedural Mesh Component
            if (entity.HasComponent<FMeshComponent>()) {
                const auto& mc = entity.GetComponent<FMeshComponent>();
                Indent(ss, 2);
                ss << "StaticMesh:\n";
                Indent(ss, 3);
                ss << "Type: \"" << mc.MeshType << "\"\n";
                Indent(ss, 3);
                ss << "Size: " << mc.MeshSize << "\n";
                Indent(ss, 3);
                ss << "Width: " << mc.MeshWidth << "\n";
                Indent(ss, 3);
                ss << "Height: " << mc.MeshHeight << "\n";
                Indent(ss, 3);
                ss << "Depth: " << mc.MeshDepth << "\n";
                Indent(ss, 3);
                ss << "Radius: " << mc.MeshRadius << "\n";
                Indent(ss, 3);
                ss << "MetersPerUv: " << mc.MeshMetersPerUv << "\n";
                Indent(ss, 3);
                ss << "SubdivisionsX: " << mc.MeshSubdivX << "\n";
                Indent(ss, 3);
                ss << "SubdivisionsZ: " << mc.MeshSubdivZ << "\n";
                Indent(ss, 3);
                ss << "Shader: \"" << mc.ShaderPath << "\"\n";
                Indent(ss, 3);
                ss << "CastShadows: " << (mc.bCastShadows ? "true" : "false") << "\n";
                Indent(ss, 3);
                ss << "ReceiveShadows: " << (mc.bReceiveShadows ? "true" : "false") << "\n";
                Indent(ss, 3);
                ss << "VisibleInReflection: " << (mc.bVisibleInReflection ? "true" : "false") << "\n";
                Indent(ss, 3);
                ss << "Mobility: " << ComponentMobilityToString(mc.Mobility) << "\n";
                Indent(ss, 3);
                ss << "LightmapResolution: " << mc.LightmapResolution << "\n";
                if (mc.LightmapIndex >= 0) {
                    Indent(ss, 3);
                    ss << "LightmapIndex: " << mc.LightmapIndex << "\n";
                    Indent(ss, 3);
                    ss << "LightmapScale: [" << mc.LightmapScale.x << ", " << mc.LightmapScale.y << "]\n";
                    Indent(ss, 3);
                    ss << "LightmapBias: [" << mc.LightmapBias.x << ", " << mc.LightmapBias.y << "]\n";
                }
                if (!mc.LightmapAssetPath.empty()) {
                    Indent(ss, 3);
                    ss << "LightmapAsset: \"" << mc.LightmapAssetPath << "\"\n";
                }
            }

            // Material Component
            if (entity.HasComponent<FMaterialComponent>()) {
                const auto& matComp = entity.GetComponent<FMaterialComponent>();
                Indent(ss, 2);
                ss << "Material:\n";
                Indent(ss, 3);
                ss << "Asset: \"" << matComp.AssetPath << "\"\n";
            }

            // Directional Light Component
            if (entity.HasComponent<FDirectionalLightComponent>()) {
                const auto& dlc = entity.GetComponent<FDirectionalLightComponent>();
                Indent(ss, 2);
                ss << "DirectionalLight:\n";
                Indent(ss, 3);
                ss << "Enabled: " << (dlc.bEnabled ? "true" : "false") << "\n";
                Indent(ss, 3);
                ss << "Direction: [" << dlc.Light.Direction.x << ", " << dlc.Light.Direction.y << ", "
                   << dlc.Light.Direction.z << "]\n";
                Indent(ss, 3);
                ss << "Color: [" << dlc.Light.Color.r << ", " << dlc.Light.Color.g << ", " << dlc.Light.Color.b
                   << "]\n";
                Indent(ss, 3);
                ss << "Intensity: " << dlc.Light.Intensity << "\n";
                Indent(ss, 3);
                ss << "Mobility: " << LightMobilityToString(dlc.Mobility) << "\n";
            }

            // Point Light Component
            if (entity.HasComponent<FPointLightComponent>()) {
                const auto& plc = entity.GetComponent<FPointLightComponent>();
                Indent(ss, 2);
                ss << "PointLight:\n";
                Indent(ss, 3);
                ss << "Enabled: " << (plc.bEnabled ? "true" : "false") << "\n";
                Indent(ss, 3);
                ss << "Color: [" << plc.Light.Color.r << ", " << plc.Light.Color.g << ", " << plc.Light.Color.b
                   << "]\n";
                Indent(ss, 3);
                ss << "Intensity: " << plc.Light.Intensity << "\n";
                Indent(ss, 3);
                ss << "Radius: " << plc.Light.Radius << "\n";
                Indent(ss, 3);
                ss << "Mobility: " << LightMobilityToString(plc.Mobility) << "\n";
            }

            // Spot Light Component
            if (entity.HasComponent<FSpotLightComponent>()) {
                const auto& slc = entity.GetComponent<FSpotLightComponent>();
                Indent(ss, 2);
                ss << "SpotLight:\n";
                Indent(ss, 3);
                ss << "Enabled: " << (slc.bEnabled ? "true" : "false") << "\n";
                Indent(ss, 3);
                ss << "Direction: [" << slc.Light.Direction.x << ", " << slc.Light.Direction.y << ", "
                   << slc.Light.Direction.z << "]\n";
                Indent(ss, 3);
                ss << "Color: [" << slc.Light.Color.r << ", " << slc.Light.Color.g << ", " << slc.Light.Color.b
                   << "]\n";
                Indent(ss, 3);
                ss << "Intensity: " << slc.Light.Intensity << "\n";
                Indent(ss, 3);
                ss << "Radius: " << slc.Light.Radius << "\n";
                Indent(ss, 3);
                ss << "CutOff: " << slc.Light.CutOff << "\n";
                Indent(ss, 3);
                ss << "OuterCutOff: " << slc.Light.OuterCutOff << "\n";
                Indent(ss, 3);
                ss << "Mobility: " << LightMobilityToString(slc.Mobility) << "\n";
            }

            // Text Component
            if (entity.HasComponent<FTextComponent>()) {
                const auto& txt = entity.GetComponent<FTextComponent>();
                Indent(ss, 2);
                ss << "Text:\n";
                Indent(ss, 3);
                ss << "Content: \"" << txt.Text << "\"\n";
                Indent(ss, 3);
                ss << "Color: [" << txt.Color.r << ", " << txt.Color.g << ", " << txt.Color.b << ", " << txt.Color.a
                   << "]\n";
                Indent(ss, 3);
                ss << "Scale: " << txt.Size << "\n";
                Indent(ss, 3);
                ss << "LineSpacing: " << txt.LineSpacing << "\n";
                Indent(ss, 3);
                ss << "Alignment: " << static_cast<int>(txt.Alignment) << "\n";
                Indent(ss, 3);
                ss << "DoubleSided: " << (txt.bDoubleSided ? "true" : "false") << "\n";
            }
        }

        OutYamlString = ss.str();
        return true;
    }

} // namespace Leon
