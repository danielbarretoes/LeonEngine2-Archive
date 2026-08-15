#include "world/MapSerializer.hpp"
#include "core/Log.hpp"
#include "gameplay/AActor.hpp"
#include "renderer/AssetManager.hpp"
#include "renderer/MeshPrimitives.hpp"
#include "world/Components.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Leon {

    MapSerializer::MapSerializer(const TRef<UWorld>& InWorld) : m_World(InWorld) {}

    static void Indent(std::stringstream& ss, int level) {
        for (int i = 0; i < level; ++i)
            ss << "  ";
    }

    static std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
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

    bool MapSerializer::Serialize(const std::string& InFilePath) {
        std::string yaml;
        if (!SerializeText(yaml)) {
            return false;
        }

        std::ofstream fout(InFilePath);
        if (!fout.is_open()) {
            LE_CORE_ERROR("MapSerializer: Failed to open file for writing: {0}", InFilePath);
            return false;
        }

        fout << yaml;
        fout.close();
        LE_CORE_INFO("MapSerializer: Successfully saved Map to '{0}'", InFilePath);
        return true;
    }

    bool MapSerializer::SerializeText(std::string& OutYamlString) {
        if (!m_World) {
            LE_CORE_ERROR("MapSerializer: Attempted to serialize a null UWorld!");
            return false;
        }

        std::stringstream ss;
        ss << "# LeonEngine2 Map Asset File (.lmap)\n";
        ss << "# Human-Readable & Deterministic Serialization\n";
        ss << "Map:\n";
        Indent(ss, 1);
        ss << "Name: \"" << m_World->GetName() << "\"\n";
        Indent(ss, 1);
        ss << "Version: \"2.0\"\n\n";

        // 1. Environment / Global Settings
        ss << "Environment:\n";
        bool bSkyboxFound = false;
        for (const auto& actorRef : m_World->GetAllActors()) {
            if (!actorRef) continue;
            if (actorRef->HasComponent<FSkyboxComponent>()) {
                const auto& sky = actorRef->GetComponent<FSkyboxComponent>();
                bSkyboxFound = true;
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
                break;
            }
        }
        if (!bSkyboxFound) {
            Indent(ss, 1);
            ss << "Skybox:\n";
            Indent(ss, 2);
            ss << "Enabled: false\n";
        }

        // 2. Actors Collection
        ss << "\nActors:\n";

        for (const auto& actorRef : m_World->GetAllActors()) {
            if (!actorRef) continue;
            const AActor& entity = *actorRef;

            if (entity.HasComponent<FSkyboxComponent>()) {
                continue;
            }

            Indent(ss, 1);
            ss << "- Name: \"" << (entity.HasComponent<FTagComponent>() ? entity.GetComponent<FTagComponent>().Tag : entity.GetName()) << "\"\n";

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
                ss << "Color: [" << dlc.Light.Color.r << ", " << dlc.Light.Color.g << ", "
                   << dlc.Light.Color.b << "]\n";
                Indent(ss, 3);
                ss << "Intensity: " << dlc.Light.Intensity << "\n";
            }

            // Point Light Component
            if (entity.HasComponent<UPointLightComponent>()) {
                const auto& plc = entity.GetComponent<UPointLightComponent>();
                Indent(ss, 2);
                ss << "PointLight:\n";
                Indent(ss, 3);
                ss << "Enabled: " << (plc.bEnabled ? "true" : "false") << "\n";
                Indent(ss, 3);
                ss << "Color: [" << plc.Light.Color.r << ", " << plc.Light.Color.g << ", "
                   << plc.Light.Color.b << "]\n";
                Indent(ss, 3);
                ss << "Intensity: " << plc.Light.Intensity << "\n";
                Indent(ss, 3);
                ss << "Radius: " << plc.Light.Radius << "\n";
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
                ss << "Color: [" << slc.Light.Color.r << ", " << slc.Light.Color.g << ", "
                   << slc.Light.Color.b << "]\n";
                Indent(ss, 3);
                ss << "Intensity: " << slc.Light.Intensity << "\n";
                Indent(ss, 3);
                ss << "Radius: " << slc.Light.Radius << "\n";
                Indent(ss, 3);
                ss << "CutOff: " << slc.Light.CutOff << "\n";
                Indent(ss, 3);
                ss << "OuterCutOff: " << slc.Light.OuterCutOff << "\n";
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

    bool MapSerializer::Deserialize(const std::string& InFilePath) {
        std::ifstream fin(InFilePath);
        if (!fin.is_open()) {
            LE_CORE_ERROR("MapSerializer: Failed to open map file '{0}'", InFilePath);
            return false;
        }

        std::stringstream ss;
        ss << fin.rdbuf();
        fin.close();

        bool result = DeserializeText(ss.str());
        if (result) {
            LE_CORE_INFO("MapSerializer: Successfully loaded Map from '{0}'", InFilePath);
        }
        return result;
    }

    struct FActorDeserializationData {
        std::string Name = "Actor";
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

        bool bHasMaterial = false;
        std::string MaterialAssetPath;
        TRef<FMaterialInstance> MaterialInstance;

        bool bHasDirLight = false;
        bool bDirLightEnabled = true;
        FDirectionalLight DirLight;

        bool bHasPointLight = false;
        bool bPointLightEnabled = true;
        FPointLight PointLight;

        bool bHasSpotLight = false;
        bool bSpotLightEnabled = true;
        FSpotLight SpotLight;

        bool bHasText = false;
        FTextComponent TextComp;
    };

    bool MapSerializer::DeserializeText(const std::string& InYamlString) {
        if (!m_World) {
            LE_CORE_ERROR("MapSerializer: Attempted to deserialize into a null UWorld!");
            return false;
        }

        std::stringstream ss(InYamlString);
        std::string line;

        enum class EParserSection { None, Map, Environment, Skybox, Actors, InActor };
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
                    m_World->SetName(StripQuotes(val));
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
                        m_World->SetName(val);
                    }
                }
                continue;
            }

            // Parse Environment block
            if (currentSection == EParserSection::Environment || currentSection == EParserSection::Skybox) {
                if (trimmed == "Skybox:") {
                    currentSection = EParserSection::Skybox;
                    bHasSkybox = true;
                    continue;
                }

                if (currentSection == EParserSection::Skybox) {
                    size_t colon = trimmed.find(':');
                    if (colon != std::string::npos) {
                        std::string key = Trim(trimmed.substr(0, colon));
                        std::string val = StripQuotes(trimmed.substr(colon + 1));

                        if (key == "Enabled") skybox.bEnabled = (val == "true");
                        else if (key == "Exposure") skybox.Exposure = std::stof(val);
                        else if (key == "SunIntensity") skybox.SunIntensity = std::stof(val);
                        else if (key == "EnvironmentIntensity") skybox.EnvironmentIntensity = std::stof(val);
                        else if (key == "UseHDREnvironmentMap") skybox.bUseHDREnvironmentMap = (val == "true");
                        else if (key == "HDREnvironmentMap" || key == "HDREnvironmentMapPath" || key == "EnvironmentMap") {
                            skybox.HDREnvironmentMapPath = val;
                            skybox.bUseHDREnvironmentMap = true;
                            if (!val.empty()) {
                                skybox.HDREnvironmentMap = FAssetManager::GetTexture2D(val);
                            }
                        } else if (key == "ZenithColor" || key == "SkyZenithColor") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) skybox.SkyZenithColor = {v[0], v[1], v[2]};
                        } else if (key == "HorizonColor") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) skybox.HorizonColor = {v[0], v[1], v[2]};
                        } else if (key == "GroundColor") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) skybox.GroundColor = {v[0], v[1], v[2]};
                        } else if (key == "SunColor") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) skybox.SunColor = {v[0], v[1], v[2]};
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
                    case EActorComponentSection::Transform:
                        if (key == "Translation" || key == "Position") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) currentActor.Translation = {v[0], v[1], v[2]};
                        } else if (key == "Rotation") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) currentActor.Rotation = {v[0], v[1], v[2]};
                        } else if (key == "Scale") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) currentActor.Scale = {v[0], v[1], v[2]};
                        }
                        break;

                    case EActorComponentSection::Camera:
                        if (key == "Primary") currentActor.bCameraPrimary = (val == "true");
                        else if (key == "FOV") currentActor.CameraFOV = std::stof(val);
                        else if (key == "Near" || key == "NearClip") currentActor.CameraNear = std::stof(val);
                        else if (key == "Far" || key == "FarClip") currentActor.CameraFar = std::stof(val);
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
                        } else if (key == "CastShadows") currentActor.bCastShadows = (val == "true");
                        else if (key == "ReceiveShadows") currentActor.bReceiveShadows = (val == "true");
                        else if (key == "VisibleInReflection") currentActor.bVisibleInReflection = (val == "true");
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
                        if (key == "Type") currentActor.MeshType = val;
                        else if (key == "Size") currentActor.MeshSize = std::stof(val);
                        else if (key == "Width") currentActor.MeshWidth = std::stof(val);
                        else if (key == "Height") currentActor.MeshHeight = std::stof(val);
                        else if (key == "Depth") currentActor.MeshDepth = std::stof(val);
                        else if (key == "Radius") currentActor.MeshRadius = std::stof(val);
                        else if (key == "SubdivisionsX" || key == "SubdivX") currentActor.MeshSubdivX = std::stoul(val);
                        else if (key == "SubdivisionsZ" || key == "SubdivZ") currentActor.MeshSubdivZ = std::stoul(val);
                        else if (key == "Shader" || key == "ShaderPath") currentActor.ShaderPath = val;
                        else if (key == "CastShadows") currentActor.bCastShadows = (val == "true");
                        else if (key == "ReceiveShadows") currentActor.bReceiveShadows = (val == "true");
                        else if (key == "VisibleInReflection") currentActor.bVisibleInReflection = (val == "true");
                        break;

                    case EActorComponentSection::Material:
                        if (key == "Asset" || key == "AssetPath") {
                            currentActor.MaterialAssetPath = val;
                            if (!val.empty()) {
                                currentActor.MaterialInstance = FAssetManager::GetMaterialInstance(val);
                                if (!currentActor.MaterialInstance) {
                                    auto baseMat = FAssetManager::GetMaterial(val);
                                    if (baseMat) {
                                        currentActor.MaterialInstance = baseMat->CreateInstance();
                                    }
                                }
                            }
                        }
                        break;

                    case EActorComponentSection::DirectionalLight:
                        if (key == "Enabled") currentActor.bDirLightEnabled = (val == "true");
                        else if (key == "Direction") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) currentActor.DirLight.Direction = {v[0], v[1], v[2]};
                        } else if (key == "Color" || key == "Radiance") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) currentActor.DirLight.Color = {v[0], v[1], v[2]};
                        } else if (key == "Intensity") currentActor.DirLight.Intensity = std::stof(val);
                        break;

                    case EActorComponentSection::PointLight:
                        if (key == "Enabled") currentActor.bPointLightEnabled = (val == "true");
                        else if (key == "Color" || key == "Radiance") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) currentActor.PointLight.Color = {v[0], v[1], v[2]};
                        } else if (key == "Intensity") currentActor.PointLight.Intensity = std::stof(val);
                        else if (key == "Radius") currentActor.PointLight.Radius = std::stof(val);
                        break;

                    case EActorComponentSection::SpotLight:
                        if (key == "Enabled") currentActor.bSpotLightEnabled = (val == "true");
                        else if (key == "Direction") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) currentActor.SpotLight.Direction = {v[0], v[1], v[2]};
                        } else if (key == "Color" || key == "Radiance") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 3) currentActor.SpotLight.Color = {v[0], v[1], v[2]};
                        } else if (key == "Intensity") currentActor.SpotLight.Intensity = std::stof(val);
                        else if (key == "Radius") currentActor.SpotLight.Radius = std::stof(val);
                        else if (key == "CutOff" || key == "InnerConeAngle") currentActor.SpotLight.CutOff = std::stof(val);
                        else if (key == "OuterCutOff" || key == "OuterConeAngle") currentActor.SpotLight.OuterCutOff = std::stof(val);
                        break;

                    case EActorComponentSection::Text:
                        if (key == "Content" || key == "Text") {
                            currentActor.TextComp.Text = val;
                        } else if (key == "Color") {
                            auto v = ParseFloatArray(val);
                            if (v.size() >= 4) currentActor.TextComp.Color = {v[0], v[1], v[2], v[3]};
                        } else if (key == "Scale" || key == "Size") currentActor.TextComp.Size = std::stof(val);
                        else if (key == "LineSpacing") currentActor.TextComp.LineSpacing = std::stof(val);
                        else if (key == "Alignment") {
                            if (val == "Left" || val == "0") currentActor.TextComp.Alignment = ETextAlignment::Left;
                            else if (val == "Center" || val == "1") currentActor.TextComp.Alignment = ETextAlignment::Center;
                            else if (val == "Right" || val == "2") currentActor.TextComp.Alignment = ETextAlignment::Right;
                        } else if (key == "DoubleSided") currentActor.TextComp.bDoubleSided = (val == "true");
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
        m_World->Clear();

        // Environment / Skybox Entity
        if (bHasSkybox) {
            AActor* envEntity = m_World->SpawnActor("Environment Skybox");
            envEntity->AddComponent<FSkyboxComponent>(skybox);
        }

        // Spawn actors
        for (const auto& actorData : actors) {
            AActor* entity = m_World->SpawnActor(actorData.Name);

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
                auto& cam = entity->AddComponent<UCameraComponent>(camera);
                cam.bPrimary = actorData.bCameraPrimary;
            }

            // Static Mesh Component (Asset-based)
            if (actorData.bHasStaticMeshAsset) {
                auto mesh = FAssetManager::GetStaticMesh(actorData.StaticMeshAssetPath);
                if (mesh) {
                    auto& comp = entity->AddComponent<UStaticMeshComponent>(mesh, actorData.StaticMeshAssetPath);
                    comp.bCastShadows = actorData.bCastShadows;
                    comp.bReceiveShadows = actorData.bReceiveShadows;
                    comp.bVisibleInReflection = actorData.bVisibleInReflection;

                    // Apply Material Overrides per slot
                    for (const auto& [slotIdx, overridePath] : actorData.MaterialOverrides) {
                        auto matInst = FAssetManager::GetMaterialInstance(overridePath);
                        if (!matInst) {
                            auto baseMat = FAssetManager::GetMaterial(overridePath);
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
                    LE_CORE_WARN("MapSerializer: Failed to load static mesh '{0}' for actor '{1}'",
                                 actorData.StaticMeshAssetPath, actorData.Name);
                }
            }
            // Mesh Component (Procedural)
            else if (actorData.bHasMesh) {
                TRef<FVertexArray> va = nullptr;
                if (actorData.MeshType == "Cube") {
                    va = FMeshPrimitives::CreateCube(actorData.MeshSize);
                } else if (actorData.MeshType == "Plane") {
                    va = FMeshPrimitives::CreatePlane(actorData.MeshWidth, actorData.MeshDepth, actorData.MeshSubdivX,
                                                      actorData.MeshSubdivZ);
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
                    va = FMeshPrimitives::CreateRamp(actorData.MeshWidth, actorData.MeshHeight, actorData.MeshDepth);
                } else if (actorData.MeshType == "Pyramid") {
                    va = FMeshPrimitives::CreatePyramid(actorData.MeshWidth, actorData.MeshHeight, actorData.MeshDepth);
                } else if (actorData.MeshType == "Quad") {
                    va = FMeshPrimitives::CreateQuad(actorData.MeshWidth, actorData.MeshHeight);
                }

                auto shader = FAssetManager::GetShader(actorData.ShaderPath);
                if (!shader) {
                    shader = FShader::Create(actorData.ShaderPath);
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
            }

            // Material Component
            if (actorData.bHasMaterial && actorData.MaterialInstance) {
                entity->AddComponent<FMaterialComponent>(actorData.MaterialInstance, actorData.MaterialAssetPath);
            }

            // Directional Light
            if (actorData.bHasDirLight) {
                auto& comp = entity->AddComponent<UDirectionalLightComponent>(actorData.DirLight);
                comp.bEnabled = actorData.bDirLightEnabled;
            }

            // Point Light
            if (actorData.bHasPointLight) {
                auto& comp = entity->AddComponent<UPointLightComponent>(actorData.PointLight);
                comp.bEnabled = actorData.bPointLightEnabled;
                comp.Light.Position = actorData.Translation;
            }

            // Spot Light
            if (actorData.bHasSpotLight) {
                auto& comp = entity->AddComponent<USpotLightComponent>(actorData.SpotLight);
                comp.bEnabled = actorData.bSpotLightEnabled;
                comp.Light.Position = actorData.Translation;
            }

            // Text Component
            if (actorData.bHasText) {
                entity->AddComponent<FTextComponent>(actorData.TextComp);
            }
        }

        LE_CORE_INFO("MapSerializer: Instantiated {0} actors from map file", actors.size());
        return true;
    }

} // namespace Leon
