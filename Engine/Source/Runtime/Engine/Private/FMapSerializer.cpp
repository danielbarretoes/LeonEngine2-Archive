#include "Engine/FMapSerializer.hpp"
#include "Core/FLog.hpp"
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
#include <vector>

namespace Leon {

    FMapSerializer::FMapSerializer(const TRef<UWorld>& InWorld) : World(InWorld) {}

    static void Indent(std::stringstream& ss, int level) {
        for (int i = 0; i < level; ++i)
            ss << "  ";
    }

    static std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    static std::string StripQuotes(const std::string& str) {
        std::string s = Trim(str);
        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            return s.substr(1, s.size() - 2);
        }
        return s;
    }

    static std::string LightingQualityToString(ELightingBuildQuality InQuality) {
        switch (InQuality) {
        case ELightingBuildQuality::Preview:
            return "Preview";
        case ELightingBuildQuality::Production:
            return "Production";
        default:
            return "Draft";
        }
    }

    static ELightingBuildQuality StringToLightingQuality(const std::string& InStr) {
        if (InStr == "Preview")
            return ELightingBuildQuality::Preview;
        if (InStr == "Production")
            return ELightingBuildQuality::Production;
        return ELightingBuildQuality::Draft;
    }

    static bool IsRuntimeFrameworkActor(const AActor& InActor) {
        return dynamic_cast<const AGameModeBase*>(&InActor) || dynamic_cast<const AGameStateBase*>(&InActor) ||
               dynamic_cast<const APlayerController*>(&InActor) || dynamic_cast<const APlayerState*>(&InActor) ||
               dynamic_cast<const AHUD*>(&InActor) || dynamic_cast<const APlayerCameraManager*>(&InActor) ||
               dynamic_cast<const ADefaultPawn*>(&InActor) || dynamic_cast<const ACharacter*>(&InActor) ||
               dynamic_cast<const AWorldSettings*>(&InActor);
    }

    static void WriteWorldSettings(std::stringstream& ss, const FWorldSettingsComponent& ws) {
        Indent(ss, 1);
        ss << "WorldSettings:\n";
        Indent(ss, 2);
        ss << "StaticLighting: " << (ws.bStaticLighting ? "true" : "false") << "\n";
        Indent(ss, 2);
        ss << "LightingBuildQuality: " << LightingQualityToString(ws.LightingBuildQuality) << "\n";
        Indent(ss, 2);
        ss << "LightmapResolution: " << ws.LightmapResolution << "\n";
        Indent(ss, 2);
        ss << "NumIndirectBounces: " << ws.NumIndirectBounces << "\n";
        Indent(ss, 2);
        ss << "SamplesPerTexel: " << ws.SamplesPerTexel << "\n";
        Indent(ss, 2);
        ss << "IndirectIntensity: " << ws.IndirectIntensity << "\n";
        Indent(ss, 2);
        ss << "AmbientOcclusion: " << (ws.bAmbientOcclusion ? "true" : "false") << "\n";
        Indent(ss, 2);
        ss << "AOIntensity: " << ws.AOIntensity << "\n";
        Indent(ss, 2);
        ss << "AORadius: " << ws.AORadius << "\n";
        Indent(ss, 2);
        ss << "TexelPadding: " << ws.TexelPadding << "\n";
        Indent(ss, 2);
        ss << "WorldScale: " << ws.WorldScale << "\n";
        if (!ws.LightmapAssetPath.empty()) {
            Indent(ss, 2);
            ss << "LightmapAsset: \"" << ws.LightmapAssetPath << "\"\n";
        }
        if (ws.LightmapBakeHash != 0) {
            Indent(ss, 2);
            ss << "LightmapBakeHash: \"" << std::hex << ws.LightmapBakeHash << std::dec << "\"\n";
        }
    }

    static std::vector<float> ParseFloatArray(const std::string& valStr) {
        std::vector<float> result;
        std::string s = Trim(valStr);
        if (!s.empty() && s.front() == '[' && s.back() == ']') {
            s = s.substr(1, s.size() - 2);
        }
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item = Trim(item);
            if (!item.empty()) {
                try {
                    result.push_back(std::stof(item));
                } catch (...) {
                    result.push_back(0.0f);
                }
            }
        }
        return result;
    }

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
            ss << "HorizonColor: [" << sky.HorizonColor.r << ", " << sky.HorizonColor.g << ", "
               << sky.HorizonColor.b << "]\n";
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
            if (entity.HasComponent<UCameraComponent>()) {
                const auto& cam = entity.GetComponent<UCameraComponent>();
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
            if (entity.HasComponent<UStaticMeshComponent>()) {
                const auto& smc = entity.GetComponent<UStaticMeshComponent>();
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
            if (entity.HasComponent<UDirectionalLightComponent>()) {
                const auto& dlc = entity.GetComponent<UDirectionalLightComponent>();
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
            if (entity.HasComponent<UPointLightComponent>()) {
                const auto& plc = entity.GetComponent<UPointLightComponent>();
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
            if (entity.HasComponent<USpotLightComponent>()) {
                const auto& slc = entity.GetComponent<USpotLightComponent>();
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

    bool FMapSerializer::Deserialize(const std::string& InFilePath) {
        std::ifstream fin(InFilePath);
        if (!fin.is_open()) {
            LE_CORE_ERROR("FMapSerializer: Failed to open map file '{0}'", InFilePath);
            return false;
        }

        std::stringstream ss;
        ss << fin.rdbuf();
        fin.close();

        bool result = DeserializeText(ss.str());
        if (result) {
            LE_CORE_INFO("FMapSerializer: Successfully loaded Map from '{0}'", InFilePath);
        }
        return result;
    }

    struct FActorDeserializationData {
        std::string Name = "Actor";
        std::string ClassName = "AActor";
        FUUID Guid;
        std::string PlayerStartTag;
        glm::vec3 Translation{0.0f};
        glm::vec3 Rotation{0.0f};
        glm::vec3 Scale{1.0f};

        bool bHasCamera = false;
        bool bCameraPrimary = true;
        float CameraFOV = 45.0f;
        float CameraNear = 0.1f;
        float CameraFar = 1000.0f;

        bool bHasStaticMeshAsset = false;
        std::string StaticMeshAssetPath;
        std::vector<std::pair<size_t, std::string>> MaterialOverrides;
        size_t CurrentOverrideSlot = 0;

        bool bHasMesh = false;
        std::string MeshType = "Cube";
        float MeshSize = 1.0f;
        float MeshWidth = 1.0f;
        float MeshHeight = 1.0f;
        float MeshDepth = 1.0f;
        float MeshRadius = 0.5f;
        unsigned int MeshSubdivX = 24;
        unsigned int MeshSubdivZ = 24;
        std::string ShaderPath = "Engine/Assets/Shaders/PBR_Lit.glsl";
        bool bCastShadows = true;
        bool bReceiveShadows = true;
        bool bVisibleInReflection = true;
        EComponentMobility Mobility = EComponentMobility::Static;
        uint32_t LightmapResolution = 64;
        int32_t LightmapIndex = -1;
        glm::vec2 LightmapScale{1.0f, 1.0f};
        glm::vec2 LightmapBias{0.0f, 0.0f};
        std::string LightmapAssetPath;

        bool bHasMaterial = false;
        std::string MaterialAssetPath;
        TRef<FMaterialInstance> MaterialInstance;

        bool bHasDirLight = false;
        bool bDirLightEnabled = true;
        ELightMobility DirLightMobility = ELightMobility::Movable;
        FDirectionalLight DirLight;

        bool bHasPointLight = false;
        bool bPointLightEnabled = true;
        ELightMobility PointLightMobility = ELightMobility::Movable;
        FPointLight PointLight;

        bool bHasSpotLight = false;
        bool bSpotLightEnabled = true;
        ELightMobility SpotLightMobility = ELightMobility::Movable;
        FSpotLight SpotLight;

        bool bHasText = false;
        FTextComponent TextComp;
    };

    bool FMapSerializer::DeserializeText(const std::string& InYamlString) {
        if (!World) {
            LE_CORE_ERROR("FMapSerializer: Attempted to deserialize into a null UWorld!");
            return false;
        }

        std::stringstream ss(InYamlString);
        std::string line;

        enum class EParserSection { None, Map, Environment, WorldSettings, Skybox, Actors, InActor };
        enum class EActorComponentSection {
            None,
            Transform,
            Camera,
            StaticMesh,
            MaterialOverrides,
            MaterialOverrideItem,
            Mesh,
            Material,
            DirectionalLight,
            PointLight,
            SpotLight,
            Text
        };

        EParserSection currentSection = EParserSection::None;
        EActorComponentSection currentCompSection = EActorComponentSection::None;

        FSkyboxComponent skybox;
        bool bHasSkybox = false;
        FWorldSettingsComponent worldSettings;
        bool bHasWorldSettings = false;
        bool bLegacyBakeOnSkybox = false;

        std::vector<FActorDeserializationData> actors;
        FActorDeserializationData currentActor;
        bool bParsingActor = false;

        while (std::getline(ss, line)) {
            std::string trimmed = Trim(line);
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }

            // Top-Level Section headers
            if (trimmed == "Map:" || trimmed.rfind("Map:", 0) == 0) {
                currentSection = EParserSection::Map;
                size_t colon = trimmed.find(':');
                std::string val = Trim(trimmed.substr(colon + 1));
                if (!val.empty()) {
                    World->SetName(StripQuotes(val));
                }
                continue;
            }

            if (trimmed == "Environment:") {
                currentSection = EParserSection::Environment;
                continue;
            }

            if (trimmed == "Actors:" || trimmed == "Entities:") {
                if (bParsingActor) {
                    actors.push_back(currentActor);
                    bParsingActor = false;
                }
                currentSection = EParserSection::Actors;
                continue;
            }

            // Parse Map Info block
            if (currentSection == EParserSection::Map) {
                size_t colon = trimmed.find(':');
                if (colon != std::string::npos) {
                    std::string key = Trim(trimmed.substr(0, colon));
                    std::string val = StripQuotes(trimmed.substr(colon + 1));
                    if (key == "Name") {
                        World->SetName(val);
                    }
                }
                continue;
            }

            // Parse Environment block
            if (currentSection == EParserSection::Environment || currentSection == EParserSection::Skybox ||
                currentSection == EParserSection::WorldSettings) {
                if (trimmed == "WorldSettings:") {
                    currentSection = EParserSection::WorldSettings;
                    bHasWorldSettings = true;
                    continue;
                }
                if (trimmed == "Skybox:") {
                    currentSection = EParserSection::Skybox;
                    bHasSkybox = true;
                    continue;
                }

                auto applyBakeKey = [&](FWorldSettingsComponent& ws, const std::string& key, const std::string& val) {
                    if (key == "StaticLighting")
                        ws.bStaticLighting = (val == "true");
                    else if (key == "LightingBuildQuality")
                        ws.LightingBuildQuality = StringToLightingQuality(val);
                    else if (key == "LightmapResolution")
                        ws.LightmapResolution = static_cast<uint32_t>(std::stoul(val));
                    else if (key == "NumIndirectBounces")
                        ws.NumIndirectBounces = static_cast<uint32_t>(std::stoul(val));
                    else if (key == "SamplesPerTexel")
                        ws.SamplesPerTexel = static_cast<uint32_t>(std::stoul(val));
                    else if (key == "IndirectIntensity")
                        ws.IndirectIntensity = std::stof(val);
                    else if (key == "AmbientOcclusion")
                        ws.bAmbientOcclusion = (val == "true");
                    else if (key == "AOIntensity")
                        ws.AOIntensity = std::stof(val);
                    else if (key == "AORadius")
                        ws.AORadius = std::stof(val);
                    else if (key == "TexelPadding")
                        ws.TexelPadding = std::stof(val);
                    else if (key == "WorldScale")
                        ws.WorldScale = std::stof(val);
                    else if (key == "LightmapAsset")
                        ws.LightmapAssetPath = val;
                    else if (key == "LightmapBakeHash")
                        ws.LightmapBakeHash = std::strtoull(val.c_str(), nullptr, 16);
                    else
                        return false;
                    return true;
                };

                if (currentSection == EParserSection::WorldSettings) {
                    size_t colon = trimmed.find(':');
                    if (colon != std::string::npos) {
                        std::string key = Trim(trimmed.substr(0, colon));
                        std::string val = StripQuotes(trimmed.substr(colon + 1));
                        applyBakeKey(worldSettings, key, val);
                    }
                    continue;
                }

                if (currentSection == EParserSection::Skybox) {
                    size_t colon = trimmed.find(':');
                    if (colon != std::string::npos) {
                        std::string key = Trim(trimmed.substr(0, colon));
                        std::string val = StripQuotes(trimmed.substr(colon + 1));

                        if (key == "Enabled")
                            skybox.bEnabled = (val == "true");
                        else if (key == "Exposure")
                            skybox.Exposure = std::stof(val);
                        else if (key == "SunIntensity")
                            skybox.SunIntensity = std::stof(val);
                        else if (key == "EnvironmentIntensity")
                            skybox.EnvironmentIntensity = std::stof(val);
                        else if (key == "UseHDREnvironmentMap")
                            skybox.bUseHDREnvironmentMap = (val == "true");
                        else if (key == "HDREnvironmentMap" || key == "HDREnvironmentMapPath" ||
                                 key == "EnvironmentMap") {
                            skybox.HDREnvironmentMapPath = val;
                            if (!val.empty()) {
                                skybox.bUseHDREnvironmentMap = true;
                                skybox.HDREnvironmentMap = UAssetManager::GetTexture2D(val);
                            }
                        } else if (key == "ZenithColor" || key == "SkyZenithColor") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                skybox.SkyZenithColor = {v[0], v[1], v[2]};
                        } else if (key == "HorizonColor") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                skybox.HorizonColor = {v[0], v[1], v[2]};
                        } else if (key == "GroundColor") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                skybox.GroundColor = {v[0], v[1], v[2]};
                        } else if (key == "SunColor") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                skybox.SunColor = {v[0], v[1], v[2]};
                        } else if (applyBakeKey(worldSettings, key, val)) {
                            bLegacyBakeOnSkybox = true;
                        }
                    }
                }
                continue;
            }

            // Parse Actors block
            if (currentSection == EParserSection::Actors || currentSection == EParserSection::InActor) {
                // New Actor entry
                if (trimmed.rfind("- Name:", 0) == 0 || trimmed.rfind("- Actor:", 0) == 0 ||
                    trimmed.rfind("- Entity:", 0) == 0 || trimmed == "-") {
                    if (bParsingActor) {
                        actors.push_back(currentActor);
                    }
                    currentActor = FActorDeserializationData{};
                    bParsingActor = true;
                    currentSection = EParserSection::InActor;
                    currentCompSection = EActorComponentSection::None;

                    size_t colon = trimmed.find(':');
                    if (colon != std::string::npos) {
                        currentActor.Name = StripQuotes(trimmed.substr(colon + 1));
                    }
                    continue;
                }

                // Component section headers
                if (trimmed == "Transform:") {
                    currentCompSection = EActorComponentSection::Transform;
                    continue;
                } else if (trimmed == "Camera:" || trimmed == "CameraComponent:") {
                    currentCompSection = EActorComponentSection::Camera;
                    currentActor.bHasCamera = true;
                    continue;
                } else if (trimmed == "StaticMesh:" || trimmed == "StaticMeshComponent:") {
                    currentCompSection = EActorComponentSection::StaticMesh;
                    continue;
                } else if (trimmed == "MaterialOverrides:") {
                    currentCompSection = EActorComponentSection::MaterialOverrides;
                    continue;
                } else if (trimmed == "Mesh:" || trimmed == "MeshComponent:") {
                    currentCompSection = EActorComponentSection::Mesh;
                    currentActor.bHasMesh = true;
                    continue;
                } else if (trimmed == "Material:" || trimmed == "MaterialComponent:") {
                    currentCompSection = EActorComponentSection::Material;
                    currentActor.bHasMaterial = true;
                    continue;
                } else if (trimmed == "DirectionalLight:" || trimmed == "DirectionalLightComponent:") {
                    currentCompSection = EActorComponentSection::DirectionalLight;
                    currentActor.bHasDirLight = true;
                    continue;
                } else if (trimmed == "PointLight:" || trimmed == "PointLightComponent:") {
                    currentCompSection = EActorComponentSection::PointLight;
                    currentActor.bHasPointLight = true;
                    continue;
                } else if (trimmed == "SpotLight:" || trimmed == "SpotLightComponent:") {
                    currentCompSection = EActorComponentSection::SpotLight;
                    currentActor.bHasSpotLight = true;
                    continue;
                } else if (trimmed == "Text:" || trimmed == "TextComponent:") {
                    currentCompSection = EActorComponentSection::Text;
                    currentActor.bHasText = true;
                    continue;
                }

                // Material Overrides List items (- Slot: 0)
                if (currentCompSection == EActorComponentSection::MaterialOverrides ||
                    currentCompSection == EActorComponentSection::MaterialOverrideItem) {
                    if (trimmed.rfind("- Slot:", 0) == 0) {
                        currentCompSection = EActorComponentSection::MaterialOverrideItem;
                        size_t colon = trimmed.find(':');
                        currentActor.CurrentOverrideSlot = std::stoul(Trim(trimmed.substr(colon + 1)));
                        continue;
                    }
                }

                // Component Key-Value parsing
                size_t colon = trimmed.find(':');
                if (colon != std::string::npos) {
                    std::string key = Trim(trimmed.substr(0, colon));
                    std::string val = StripQuotes(trimmed.substr(colon + 1));

                    switch (currentCompSection) {
                    case EActorComponentSection::None:
                        if (key == "Class")
                            currentActor.ClassName = val;
                        else if (key == "GUID" || key == "Guid")
                            currentActor.Guid = FUUID::FromString(val);
                        else if (key == "PlayerStartTag")
                            currentActor.PlayerStartTag = val;
                        break;

                    case EActorComponentSection::Transform:
                        if (key == "Translation" || key == "Position") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                currentActor.Translation = {v[0], v[1], v[2]};
                        } else if (key == "Rotation") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                currentActor.Rotation = {v[0], v[1], v[2]};
                        } else if (key == "Scale") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                currentActor.Scale = {v[0], v[1], v[2]};
                        }
                        break;

                    case EActorComponentSection::Camera:
                        if (key == "Primary")
                            currentActor.bCameraPrimary = (val == "true");
                        else if (key == "FOV")
                            currentActor.CameraFOV = std::stof(val);
                        else if (key == "Near" || key == "NearClip")
                            currentActor.CameraNear = std::stof(val);
                        else if (key == "Far" || key == "FarClip")
                            currentActor.CameraFar = std::stof(val);
                        break;

                    case EActorComponentSection::StaticMesh:
                        if (key == "Asset" || key == "AssetPath") {
                            currentActor.bHasStaticMeshAsset = true;
                            currentActor.StaticMeshAssetPath = val;
                        } else if (key == "Type") {
                            currentActor.bHasMesh = true;
                            currentActor.MeshType = val;
                        } else if (key == "Size") {
                            currentActor.bHasMesh = true;
                            currentActor.MeshSize = std::stof(val);
                        } else if (key == "Width") {
                            currentActor.bHasMesh = true;
                            currentActor.MeshWidth = std::stof(val);
                        } else if (key == "Height") {
                            currentActor.bHasMesh = true;
                            currentActor.MeshHeight = std::stof(val);
                        } else if (key == "Depth") {
                            currentActor.bHasMesh = true;
                            currentActor.MeshDepth = std::stof(val);
                        } else if (key == "Radius") {
                            currentActor.bHasMesh = true;
                            currentActor.MeshRadius = std::stof(val);
                        } else if (key == "SubdivisionsX" || key == "SubdivX") {
                            currentActor.bHasMesh = true;
                            currentActor.MeshSubdivX = std::stoul(val);
                        } else if (key == "SubdivisionsZ" || key == "SubdivZ") {
                            currentActor.bHasMesh = true;
                            currentActor.MeshSubdivZ = std::stoul(val);
                        } else if (key == "Shader" || key == "ShaderPath") {
                            currentActor.bHasMesh = true;
                            currentActor.ShaderPath = val;
                        } else if (key == "CastShadows")
                            currentActor.bCastShadows = (val == "true");
                        else if (key == "ReceiveShadows")
                            currentActor.bReceiveShadows = (val == "true");
                        else if (key == "VisibleInReflection")
                            currentActor.bVisibleInReflection = (val == "true");
                        else if (key == "Mobility")
                            currentActor.Mobility = StringToComponentMobility(val);
                        else if (key == "LightmapResolution")
                            currentActor.LightmapResolution = static_cast<uint32_t>(std::stoul(val));
                        else if (key == "LightmapIndex")
                            currentActor.LightmapIndex = std::stoi(val);
                        else if (key == "LightmapScale") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 2)
                                currentActor.LightmapScale = {v[0], v[1]};
                        } else if (key == "LightmapBias") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 2)
                                currentActor.LightmapBias = {v[0], v[1]};
                        } else if (key == "LightmapAsset")
                            currentActor.LightmapAssetPath = val;
                        break;

                    case EActorComponentSection::MaterialOverrides:
                    case EActorComponentSection::MaterialOverrideItem:
                        if (key == "Asset" || key == "AssetPath") {
                            currentActor.MaterialOverrides.push_back({currentActor.CurrentOverrideSlot, val});
                        } else if (key.rfind("Slot_", 0) == 0) {
                            size_t slotIdx = std::stoul(key.substr(5));
                            currentActor.MaterialOverrides.push_back({slotIdx, val});
                        }
                        break;

                    case EActorComponentSection::Mesh:
                        if (key == "Type")
                            currentActor.MeshType = val;
                        else if (key == "Size")
                            currentActor.MeshSize = std::stof(val);
                        else if (key == "Width")
                            currentActor.MeshWidth = std::stof(val);
                        else if (key == "Height")
                            currentActor.MeshHeight = std::stof(val);
                        else if (key == "Depth")
                            currentActor.MeshDepth = std::stof(val);
                        else if (key == "Radius")
                            currentActor.MeshRadius = std::stof(val);
                        else if (key == "SubdivisionsX" || key == "SubdivX")
                            currentActor.MeshSubdivX = std::stoul(val);
                        else if (key == "SubdivisionsZ" || key == "SubdivZ")
                            currentActor.MeshSubdivZ = std::stoul(val);
                        else if (key == "Shader" || key == "ShaderPath")
                            currentActor.ShaderPath = val;
                        else if (key == "CastShadows")
                            currentActor.bCastShadows = (val == "true");
                        else if (key == "ReceiveShadows")
                            currentActor.bReceiveShadows = (val == "true");
                        else if (key == "VisibleInReflection")
                            currentActor.bVisibleInReflection = (val == "true");
                        else if (key == "Mobility")
                            currentActor.Mobility = StringToComponentMobility(val);
                        else if (key == "LightmapResolution")
                            currentActor.LightmapResolution = static_cast<uint32_t>(std::stoul(val));
                        else if (key == "LightmapIndex")
                            currentActor.LightmapIndex = std::stoi(val);
                        else if (key == "LightmapScale") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 2)
                                currentActor.LightmapScale = {v[0], v[1]};
                        } else if (key == "LightmapBias") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 2)
                                currentActor.LightmapBias = {v[0], v[1]};
                        } else if (key == "LightmapAsset")
                            currentActor.LightmapAssetPath = val;
                        break;

                    case EActorComponentSection::Material:
                        if (key == "Asset" || key == "AssetPath") {
                            currentActor.MaterialAssetPath = val;
                            if (!val.empty()) {
                                currentActor.MaterialInstance = UAssetManager::GetMaterialInstance(val);
                                if (!currentActor.MaterialInstance) {
                                    auto baseMat = UAssetManager::GetMaterial(val);
                                    if (baseMat) {
                                        currentActor.MaterialInstance = baseMat->CreateInstance();
                                    }
                                }
                            }
                        }
                        break;

                    case EActorComponentSection::DirectionalLight:
                        if (key == "Enabled")
                            currentActor.bDirLightEnabled = (val == "true");
                        else if (key == "Direction") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                currentActor.DirLight.Direction = {v[0], v[1], v[2]};
                        } else if (key == "Color" || key == "Radiance") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                currentActor.DirLight.Color = {v[0], v[1], v[2]};
                        } else if (key == "Intensity")
                            currentActor.DirLight.Intensity = std::stof(val);
                        else if (key == "Mobility")
                            currentActor.DirLightMobility = StringToLightMobility(val);
                        break;

                    case EActorComponentSection::PointLight:
                        if (key == "Enabled")
                            currentActor.bPointLightEnabled = (val == "true");
                        else if (key == "Color" || key == "Radiance") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                currentActor.PointLight.Color = {v[0], v[1], v[2]};
                        } else if (key == "Intensity")
                            currentActor.PointLight.Intensity = std::stof(val);
                        else if (key == "Radius")
                            currentActor.PointLight.Radius = std::stof(val);
                        else if (key == "Mobility")
                            currentActor.PointLightMobility = StringToLightMobility(val);
                        break;

                    case EActorComponentSection::SpotLight:
                        if (key == "Enabled")
                            currentActor.bSpotLightEnabled = (val == "true");
                        else if (key == "Direction") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                currentActor.SpotLight.Direction = {v[0], v[1], v[2]};
                        } else if (key == "Color" || key == "Radiance") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3)
                                currentActor.SpotLight.Color = {v[0], v[1], v[2]};
                        } else if (key == "Intensity")
                            currentActor.SpotLight.Intensity = std::stof(val);
                        else if (key == "Radius")
                            currentActor.SpotLight.Radius = std::stof(val);
                        else if (key == "CutOff" || key == "InnerConeAngle")
                            currentActor.SpotLight.CutOff = std::stof(val);
                        else if (key == "OuterCutOff" || key == "OuterConeAngle")
                            currentActor.SpotLight.OuterCutOff = std::stof(val);
                        else if (key == "Mobility")
                            currentActor.SpotLightMobility = StringToLightMobility(val);
                        break;

                    case EActorComponentSection::Text:
                        if (key == "Content" || key == "Text") {
                            currentActor.TextComp.Text = val;
                        } else if (key == "Color") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 4)
                                currentActor.TextComp.Color = {v[0], v[1], v[2], v[3]};
                        } else if (key == "Scale" || key == "Size")
                            currentActor.TextComp.Size = std::stof(val);
                        else if (key == "LineSpacing")
                            currentActor.TextComp.LineSpacing = std::stof(val);
                        else if (key == "Alignment") {
                            if (val == "Left" || val == "0")
                                currentActor.TextComp.Alignment = ETextAlignment::Left;
                            else if (val == "Center" || val == "1")
                                currentActor.TextComp.Alignment = ETextAlignment::Center;
                            else if (val == "Right" || val == "2")
                                currentActor.TextComp.Alignment = ETextAlignment::Right;
                        } else if (key == "DoubleSided")
                            currentActor.TextComp.bDoubleSided = (val == "true");
                        break;

                    default:
                        break;
                    }
                }
            }
        }

        if (bParsingActor) {
            actors.push_back(currentActor);
        }

        // 3. Populate World
        World->Clear();

        // Environment / Skybox / WorldSettings
        if (bLegacyBakeOnSkybox && !bHasWorldSettings)
            bHasWorldSettings = true;

        if (bHasSkybox || bHasWorldSettings) {
            AActor* envEntity = World->SpawnActor("Environment Skybox");
            if (bHasSkybox)
                envEntity->AddComponent<FSkyboxComponent>(skybox);
            if (bHasWorldSettings)
                envEntity->AddComponent<FWorldSettingsComponent>(worldSettings);
        }

        // Spawn actors
        for (const auto& actorData : actors) {
            AActor* entity = nullptr;
            std::string className = actorData.ClassName.empty() ? "AActor" : actorData.ClassName;
            if (UClassRegistry::Get().HasClass(className)) {
                entity = UClassRegistry::Get().CreateActorOfClass(className, World.get(), actorData.Name);
            }
            if (!entity)
                entity = World->SpawnActor(actorData.Name);
            if (!entity)
                continue;

            entity->SetClass(className);
            if (actorData.Guid.IsValid())
                entity->SetActorGuid(actorData.Guid);
            else
                entity->SetActorGuid(FUUID::FromPath(World->GetName() + "/" + actorData.Name));

            if (auto* start = dynamic_cast<APlayerStart*>(entity)) {
                if (!actorData.PlayerStartTag.empty())
                    start->SetPlayerStartTag(actorData.PlayerStartTag);
            }

            // Transform
            auto& transform = entity->GetComponent<FTransformComponent>();
            transform.Translation = actorData.Translation;
            transform.Rotation = actorData.Rotation;
            transform.Scale = actorData.Scale;

            // Camera Component
            if (actorData.bHasCamera) {
                FPerspectiveCamera camera(actorData.CameraFOV, 1280.0f / 720.0f, actorData.CameraNear,
                                          actorData.CameraFar);
                camera.SetPosition(actorData.Translation);
                camera.SetRotation(actorData.Rotation.x, actorData.Rotation.y);
                if (entity->HasComponent<UCameraComponent>()) {
                    auto& cam = entity->GetComponent<UCameraComponent>();
                    cam.Camera = camera;
                    cam.bPrimary = actorData.bCameraPrimary;
                } else {
                    auto& cam = entity->AddComponent<UCameraComponent>(camera);
                    cam.bPrimary = actorData.bCameraPrimary;
                }
            }

            // Static Mesh Component (Asset-based)
            if (actorData.bHasStaticMeshAsset) {
                auto mesh = UAssetManager::GetStaticMesh(actorData.StaticMeshAssetPath);
                if (mesh) {
                    auto& comp = entity->AddComponent<UStaticMeshComponent>(mesh, actorData.StaticMeshAssetPath);
                    comp.bCastShadows = actorData.bCastShadows;
                    comp.bReceiveShadows = actorData.bReceiveShadows;
                    comp.bVisibleInReflection = actorData.bVisibleInReflection;
                    comp.Mobility = actorData.Mobility;
                    comp.LightmapResolution = actorData.LightmapResolution;
                    comp.LightmapIndex = actorData.LightmapIndex;
                    comp.LightmapScale = actorData.LightmapScale;
                    comp.LightmapBias = actorData.LightmapBias;
                    comp.LightmapAssetPath = actorData.LightmapAssetPath;

                    // Apply Material Overrides per slot
                    for (const auto& [slotIdx, overridePath] : actorData.MaterialOverrides) {
                        auto matInst = UAssetManager::GetMaterialInstance(overridePath);
                        if (!matInst) {
                            auto baseMat = UAssetManager::GetMaterial(overridePath);
                            if (baseMat) {
                                matInst = baseMat->CreateInstance();
                            }
                        }
                        if (matInst) {
                            if (comp.MaterialOverrides.size() <= slotIdx) {
                                comp.MaterialOverrides.resize(slotIdx + 1, nullptr);
                                comp.MaterialOverridePaths.resize(slotIdx + 1, "");
                            }
                            comp.MaterialOverrides[slotIdx] = matInst;
                            comp.MaterialOverridePaths[slotIdx] = overridePath;
                        }
                    }
                } else {
                    LE_CORE_WARN("FMapSerializer: Failed to load static mesh '{0}' for actor '{1}'",
                                 actorData.StaticMeshAssetPath, actorData.Name);
                }
            }
            // Mesh Component (Procedural)
            else if (actorData.bHasMesh) {
                TRef<FVertexArray> va = nullptr;
                if (FRenderDriverRegistry::GetActiveDriver()) {
                    if (actorData.MeshType == "Cube") {
                        va = FMeshPrimitives::CreateCube(actorData.MeshSize);
                    } else if (actorData.MeshType == "Plane") {
                        va = FMeshPrimitives::CreatePlane(actorData.MeshWidth, actorData.MeshDepth,
                                                          actorData.MeshSubdivX, actorData.MeshSubdivZ);
                    } else if (actorData.MeshType == "Sphere") {
                        va = FMeshPrimitives::CreateSphere(actorData.MeshRadius, actorData.MeshSubdivX,
                                                           actorData.MeshSubdivZ);
                    } else if (actorData.MeshType == "Cylinder") {
                        va = FMeshPrimitives::CreateCylinder(actorData.MeshRadius, actorData.MeshRadius,
                                                             actorData.MeshHeight, actorData.MeshSubdivX, true);
                    } else if (actorData.MeshType == "Cone") {
                        va = FMeshPrimitives::CreateCylinder(actorData.MeshRadius, 0.0f, actorData.MeshHeight,
                                                             actorData.MeshSubdivX, true);
                    } else if (actorData.MeshType == "Ramp") {
                        va = FMeshPrimitives::CreateRamp(actorData.MeshWidth, actorData.MeshHeight,
                                                         actorData.MeshDepth);
                    } else if (actorData.MeshType == "Pyramid") {
                        va = FMeshPrimitives::CreatePyramid(actorData.MeshWidth, actorData.MeshHeight,
                                                          actorData.MeshDepth);
                    } else if (actorData.MeshType == "Quad") {
                        va = FMeshPrimitives::CreateQuad(actorData.MeshWidth, actorData.MeshHeight);
                    }
                }

                TRef<FShader> shader = nullptr;
                if (FRenderDriverRegistry::GetActiveDriver()) {
                    shader = UAssetManager::GetShader(actorData.ShaderPath);
                    if (!shader) {
                        shader = FShader::Create(actorData.ShaderPath);
                    }
                }

                auto& comp = entity->AddComponent<FMeshComponent>(va, shader);
                comp.MeshType = actorData.MeshType;
                comp.MeshSize = actorData.MeshSize;
                comp.MeshWidth = actorData.MeshWidth;
                comp.MeshHeight = actorData.MeshHeight;
                comp.MeshDepth = actorData.MeshDepth;
                comp.MeshRadius = actorData.MeshRadius;
                comp.MeshSubdivX = actorData.MeshSubdivX;
                comp.MeshSubdivZ = actorData.MeshSubdivZ;
                comp.ShaderPath = actorData.ShaderPath;
                comp.bCastShadows = actorData.bCastShadows;
                comp.bReceiveShadows = actorData.bReceiveShadows;
                comp.bVisibleInReflection = actorData.bVisibleInReflection;
                comp.Mobility = actorData.Mobility;
                comp.LightmapResolution = actorData.LightmapResolution;
                comp.LightmapIndex = actorData.LightmapIndex;
                comp.LightmapScale = actorData.LightmapScale;
                comp.LightmapBias = actorData.LightmapBias;
                comp.LightmapAssetPath = actorData.LightmapAssetPath;
            }

            // Material Component
            if (actorData.bHasMaterial && actorData.MaterialInstance) {
                entity->AddComponent<FMaterialComponent>(actorData.MaterialInstance, actorData.MaterialAssetPath);
            }

            // Directional Light
            if (actorData.bHasDirLight) {
                auto& comp = entity->AddComponent<UDirectionalLightComponent>(actorData.DirLight);
                comp.bEnabled = actorData.bDirLightEnabled;
                comp.Mobility = actorData.DirLightMobility;
            }

            // Point Light
            if (actorData.bHasPointLight) {
                auto& comp = entity->AddComponent<UPointLightComponent>(actorData.PointLight);
                comp.bEnabled = actorData.bPointLightEnabled;
                comp.Mobility = actorData.PointLightMobility;
                comp.Light.Position = actorData.Translation;
            }

            // Spot Light
            if (actorData.bHasSpotLight) {
                auto& comp = entity->AddComponent<USpotLightComponent>(actorData.SpotLight);
                comp.bEnabled = actorData.bSpotLightEnabled;
                comp.Mobility = actorData.SpotLightMobility;
                comp.Light.Position = actorData.Translation;
            }

            // Text Component
            if (actorData.bHasText) {
                entity->AddComponent<FTextComponent>(actorData.TextComp);
            }
        }

        LE_CORE_INFO("FMapSerializer: Instantiated {0} actors from map file", actors.size());
        return true;
    }

} // namespace Leon
