#include "scene/LevelSerializer.hpp"
#include "core/Log.hpp"
#include "renderer/AssetManager.hpp"
#include "renderer/MeshPrimitives.hpp"
#include "renderer/Shader.hpp"
#include "renderer/Texture.hpp"
#include "scene/Components.hpp"
#include "scene/Entity.hpp"
#include "scene/MaterialSerializer.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Leon {

    namespace Utils {

        static std::string Trim(const std::string& InStr) {
            size_t first = InStr.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return "";
            size_t last = InStr.find_last_not_of(" \t\r\n");
            return InStr.substr(first, (last - first + 1));
        }

        static std::string UnescapeString(const std::string& InStr) {
            std::string s = InStr;
            size_t pos = 0;
            while ((pos = s.find("\\n", pos)) != std::string::npos) {
                s.replace(pos, 2, "\n");
                pos += 1;
            }
            pos = 0;
            while ((pos = s.find("\\t", pos)) != std::string::npos) {
                s.replace(pos, 2, "\t");
                pos += 1;
            }
            pos = 0;
            while ((pos = s.find("\\r", pos)) != std::string::npos) {
                s.replace(pos, 2, "\r");
                pos += 1;
            }
            pos = 0;
            while ((pos = s.find("\\\"", pos)) != std::string::npos) {
                s.replace(pos, 2, "\"");
                pos += 1;
            }
            return s;
        }

        static std::string EscapeString(const std::string& InStr) {
            std::string result;
            for (char c : InStr) {
                if (c == '\n')
                    result += "\\n";
                else if (c == '\t')
                    result += "\\t";
                else if (c == '\r')
                    result += "\\r";
                else if (c == '\"')
                    result += "\\\"";
                else
                    result += c;
            }
            return result;
        }

        static std::string CleanValue(const std::string& InStr) {
            std::string s = Trim(InStr);
            if (s.length() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
                s = s.substr(1, s.length() - 2);
            }
            return UnescapeString(s);
        }

        static float ParseFloat(const std::string& InStr, float InDefault = 0.0f) {
            std::string s = CleanValue(InStr);
            if (s.empty())
                return InDefault;
            try {
                return std::stof(s);
            } catch (...) {
                return InDefault;
            }
        }

        static int ParseInt(const std::string& InStr, int InDefault = 0) {
            std::string s = CleanValue(InStr);
            if (s.empty())
                return InDefault;
            try {
                return std::stoi(s);
            } catch (...) {
                return InDefault;
            }
        }

        static bool ParseBool(const std::string& InStr, bool InDefault = false) {
            std::string s = CleanValue(InStr);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            if (s == "true" || s == "1" || s == "yes" || s == "on")
                return true;
            if (s == "false" || s == "0" || s == "no" || s == "off")
                return false;
            return InDefault;
        }

        static glm::vec2 ParseVec2(const std::string& InStr, const glm::vec2& InDefault = glm::vec2(0.0f)) {
            std::string s = CleanValue(InStr);
            if (s.front() == '[')
                s = s.substr(1);
            if (!s.empty() && s.back() == ']')
                s.pop_back();

            std::stringstream ss(s);
            std::string item;
            std::vector<float> values;
            while (std::getline(ss, item, ',')) {
                values.push_back(ParseFloat(item));
            }
            if (values.size() >= 2)
                return glm::vec2(values[0], values[1]);
            return InDefault;
        }

        static glm::vec3 ParseVec3(const std::string& InStr, const glm::vec3& InDefault = glm::vec3(0.0f)) {
            std::string s = CleanValue(InStr);
            if (s.front() == '[')
                s = s.substr(1);
            if (!s.empty() && s.back() == ']')
                s.pop_back();

            std::stringstream ss(s);
            std::string item;
            std::vector<float> values;
            while (std::getline(ss, item, ',')) {
                values.push_back(ParseFloat(item));
            }
            if (values.size() >= 3)
                return glm::vec3(values[0], values[1], values[2]);
            if (values.size() == 1)
                return glm::vec3(values[0]);
            return InDefault;
        }

        static glm::vec4 ParseVec4(const std::string& InStr, const glm::vec4& InDefault = glm::vec4(1.0f)) {
            std::string s = CleanValue(InStr);
            if (s.front() == '[')
                s = s.substr(1);
            if (!s.empty() && s.back() == ']')
                s.pop_back();

            std::stringstream ss(s);
            std::string item;
            std::vector<float> values;
            while (std::getline(ss, item, ',')) {
                values.push_back(ParseFloat(item));
            }
            if (values.size() >= 4)
                return glm::vec4(values[0], values[1], values[2], values[3]);
            if (values.size() >= 3)
                return glm::vec4(values[0], values[1], values[2], 1.0f);
            return InDefault;
        }

    } // namespace Utils

    FLevelSerializer::FLevelSerializer(const TRef<FScene>& InScene) : m_Scene(InScene) {}

    bool FLevelSerializer::Serialize(const std::string& InFilePath) {
        std::string text;
        if (!SerializeText(text))
            return false;

        std::ofstream file(InFilePath);
        if (!file.is_open()) {
            LE_CORE_ERROR("FLevelSerializer: Could not open '{0}' for writing!", InFilePath);
            return false;
        }

        file << text;
        LE_CORE_INFO("FLevelSerializer: Saved level to '{0}'", InFilePath);
        return true;
    }

    bool FLevelSerializer::SerializeText(std::string& OutText) {
        if (!m_Scene)
            return false;

        std::stringstream ss;
        ss << "# LeonEngine2 Level Asset File (.llevel)\n";
        ss << "Level:\n";
        ss << "  Name: \"Level\"\n";
        ss << "  Version: 1.0\n\n";

        // 1. Serialize Skybox & Environment
        auto skyboxView = m_Scene->GetRegistry().view<FSkyboxComponent>();
        for (auto entity : skyboxView) {
            const auto& skybox = skyboxView.get<FSkyboxComponent>(entity);
            ss << "Environment:\n";
            ss << "  Skybox:\n";
            ss << "    Enabled: " << (skybox.bEnabled ? "true" : "false") << "\n";
            ss << "    Exposure: " << skybox.Exposure << "\n";
            ss << "    SunIntensity: " << skybox.SunIntensity << "\n";
            ss << "    EnvironmentIntensity: " << skybox.EnvironmentIntensity << "\n";
            if (skybox.HDREnvironmentMap && !skybox.HDREnvironmentMap->GetPath().empty()) {
                ss << "    HDREnvironmentMap: \"" << skybox.HDREnvironmentMap->GetPath() << "\"\n";
                ss << "    UseHDREnvironmentMap: " << (skybox.bUseHDREnvironmentMap ? "true" : "false") << "\n";
            } else if (!skybox.HDREnvironmentMapPath.empty()) {
                ss << "    HDREnvironmentMap: \"" << skybox.HDREnvironmentMapPath << "\"\n";
                ss << "    UseHDREnvironmentMap: " << (skybox.bUseHDREnvironmentMap ? "true" : "false") << "\n";
            }
            ss << "    SkyZenithColor: [" << skybox.SkyZenithColor.r << ", " << skybox.SkyZenithColor.g << ", "
               << skybox.SkyZenithColor.b << "]\n";
            ss << "    HorizonColor: [" << skybox.HorizonColor.r << ", " << skybox.HorizonColor.g << ", "
               << skybox.HorizonColor.b << "]\n";
            ss << "    GroundColor: [" << skybox.GroundColor.r << ", " << skybox.GroundColor.g << ", "
               << skybox.GroundColor.b << "]\n";
            ss << "    SunColor: [" << skybox.SunColor.r << ", " << skybox.SunColor.g << ", " << skybox.SunColor.b
               << "]\n\n";
            break;
        }

        // 2. Serialize Actors
        ss << "Actors:\n";
        auto tagView = m_Scene->GetRegistry().view<FTagComponent>();
        for (auto entityHandle : tagView) {
            FEntity entity = {entityHandle, m_Scene.get()};

            // Skip Environment Skybox entity as it is serialized in the Environment block
            if (entity.HasComponent<FSkyboxComponent>()) {
                continue;
            }

            const auto& tag = entity.GetComponent<FTagComponent>();

            ss << "  - Name: \"" << tag.Tag << "\"\n";

            // Transform
            if (entity.HasComponent<FTransformComponent>()) {
                const auto& transform = entity.GetComponent<FTransformComponent>();
                ss << "    Transform:\n";
                ss << "      Translation: [" << transform.Translation.x << ", " << transform.Translation.y << ", "
                   << transform.Translation.z << "]\n";
                ss << "      Rotation: [" << transform.Rotation.x << ", " << transform.Rotation.y << ", "
                   << transform.Rotation.z << "]\n";
                ss << "      Scale: [" << transform.Scale.x << ", " << transform.Scale.y << ", " << transform.Scale.z
                   << "]\n";
            }

            // Static Mesh Component
            if (entity.HasComponent<FStaticMeshComponent>()) {
                const auto& meshComp = entity.GetComponent<FStaticMeshComponent>();
                ss << "    StaticMesh:\n";
                if (!meshComp.AssetPath.empty()) {
                    ss << "      Asset: \"" << meshComp.AssetPath << "\"\n";
                }
                ss << "      CastShadows: " << (meshComp.bCastShadows ? "true" : "false") << "\n";
                ss << "      ReceiveShadows: " << (meshComp.bReceiveShadows ? "true" : "false") << "\n";
                ss << "      VisibleInReflection: " << (meshComp.bVisibleInReflection ? "true" : "false") << "\n";
                if (!meshComp.MaterialOverridePaths.empty()) {
                    ss << "      MaterialOverrides:\n";
                    for (size_t sl = 0; sl < meshComp.MaterialOverridePaths.size(); ++sl) {
                        if (!meshComp.MaterialOverridePaths[sl].empty()) {
                            ss << "        - Slot: " << sl << "\n";
                            ss << "          Asset: \"" << meshComp.MaterialOverridePaths[sl] << "\"\n";
                        }
                    }
                }
            }

            // Camera Component
            if (entity.HasComponent<FCameraComponent>()) {
                const auto& camComp = entity.GetComponent<FCameraComponent>();
                ss << "    Camera:\n";
                ss << "      Primary: " << (camComp.bPrimary ? "true" : "false") << "\n";
                ss << "      FOV: " << camComp.Camera.GetFOV() << "\n";
                ss << "      NearPlane: " << camComp.Camera.GetNearClip() << "\n";
                ss << "      FarPlane: " << camComp.Camera.GetFarClip() << "\n";
            }

            // Mesh Component (Procedural)
            if (entity.HasComponent<FMeshComponent>()) {
                const auto& mesh = entity.GetComponent<FMeshComponent>();
                ss << "    Mesh:\n";
                ss << "      Type: " << mesh.MeshType << "\n";
                if (mesh.MeshType == "Cube")
                    ss << "      Size: " << mesh.MeshSize << "\n";
                else if (mesh.MeshType == "Plane") {
                    ss << "      Width: " << mesh.MeshWidth << "\n";
                    ss << "      Depth: " << mesh.MeshDepth << "\n";
                    ss << "      SubdivisionsX: " << mesh.MeshSubdivX << "\n";
                    ss << "      SubdivisionsZ: " << mesh.MeshSubdivZ << "\n";
                } else if (mesh.MeshType == "Sphere") {
                    ss << "      Radius: " << mesh.MeshRadius << "\n";
                    ss << "      SubdivisionsX: " << mesh.MeshSubdivX << "\n";
                    ss << "      SubdivisionsZ: " << mesh.MeshSubdivZ << "\n";
                } else if (mesh.MeshType == "Cylinder" || mesh.MeshType == "Cone") {
                    ss << "      Radius: " << mesh.MeshRadius << "\n";
                    ss << "      Height: " << mesh.MeshHeight << "\n";
                    ss << "      SubdivisionsX: " << mesh.MeshSubdivX << "\n";
                } else if (mesh.MeshType == "Ramp" || mesh.MeshType == "Pyramid") {
                    ss << "      Width: " << mesh.MeshWidth << "\n";
                    ss << "      Height: " << mesh.MeshHeight << "\n";
                    ss << "      Depth: " << mesh.MeshDepth << "\n";
                } else if (mesh.MeshType == "Quad") {
                    ss << "      Width: " << mesh.MeshWidth << "\n";
                    ss << "      Height: " << mesh.MeshHeight << "\n";
                }
                if (!mesh.ShaderPath.empty())
                    ss << "      Shader: \"" << mesh.ShaderPath << "\"\n";
                ss << "      CastShadows: " << (mesh.bCastShadows ? "true" : "false") << "\n";
                ss << "      ReceiveShadows: " << (mesh.bReceiveShadows ? "true" : "false") << "\n";
                ss << "      VisibleInReflection: " << (mesh.bVisibleInReflection ? "true" : "false") << "\n";
            }

            // Material Component
            if (entity.HasComponent<FMaterialComponent>()) {
                const auto& matComp = entity.GetComponent<FMaterialComponent>();
                ss << "    Material:\n";
                if (!matComp.AssetPath.empty()) {
                    ss << "      Asset: \"" << matComp.AssetPath << "\"\n";
                } else if (matComp.MaterialInstance) {
                    ss << "      BaseColor: [" << matComp.MaterialInstance->GetBaseColor().r << ", "
                       << matComp.MaterialInstance->GetBaseColor().g << ", "
                       << matComp.MaterialInstance->GetBaseColor().b << "]\n";
                    ss << "      Metallic: " << matComp.MaterialInstance->GetMetallic() << "\n";
                    ss << "      Roughness: " << matComp.MaterialInstance->GetRoughness() << "\n";
                    ss << "      AO: " << matComp.MaterialInstance->GetAO() << "\n";
                    ss << "      NormalScale: " << matComp.MaterialInstance->GetNormalScale() << "\n";
                    ss << "      OcclusionStrength: " << matComp.MaterialInstance->GetOcclusionStrength() << "\n";
                    ss << "      EmissiveColor: [" << matComp.MaterialInstance->GetEmissiveColor().r << ", "
                       << matComp.MaterialInstance->GetEmissiveColor().g << ", "
                       << matComp.MaterialInstance->GetEmissiveColor().b << "]\n";
                    ss << "      EmissiveIntensity: " << matComp.MaterialInstance->GetEmissiveIntensity() << "\n";
                    ss << "      AlphaMode: " << static_cast<int>(matComp.MaterialInstance->GetAlphaMode()) << "\n";
                    ss << "      AlphaCutoff: " << matComp.MaterialInstance->GetAlphaCutoff() << "\n";
                    ss << "      UVTiling: [" << matComp.MaterialInstance->GetUVTiling().x << ", "
                       << matComp.MaterialInstance->GetUVTiling().y << "]\n";
                    ss << "      UVOffset: [" << matComp.MaterialInstance->GetUVOffset().x << ", "
                       << matComp.MaterialInstance->GetUVOffset().y << "]\n";

                    if (matComp.MaterialInstance->GetTexture(0))
                        ss << "      AlbedoMap: \"" << matComp.MaterialInstance->GetTexture(0)->GetPath() << "\"\n";
                    if (matComp.MaterialInstance->GetTexture(1))
                        ss << "      NormalMap: \"" << matComp.MaterialInstance->GetTexture(1)->GetPath() << "\"\n";
                    if (matComp.MaterialInstance->GetTexture(2))
                        ss << "      MetallicMap: \"" << matComp.MaterialInstance->GetTexture(2)->GetPath() << "\"\n";
                    if (matComp.MaterialInstance->GetTexture(3))
                        ss << "      RoughnessMap: \"" << matComp.MaterialInstance->GetTexture(3)->GetPath() << "\"\n";
                    if (matComp.MaterialInstance->GetTexture(4))
                        ss << "      AOMap: \"" << matComp.MaterialInstance->GetTexture(4)->GetPath() << "\"\n";
                    if (matComp.MaterialInstance->GetTexture(5))
                        ss << "      EmissiveMap: \"" << matComp.MaterialInstance->GetTexture(5)->GetPath() << "\"\n";
                }
            }

            // Directional Light Component
            if (entity.HasComponent<FDirectionalLightComponent>()) {
                const auto& dir = entity.GetComponent<FDirectionalLightComponent>();
                ss << "    DirectionalLight:\n";
                ss << "      Enabled: " << (dir.bEnabled ? "true" : "false") << "\n";
                ss << "      Direction: [" << dir.Light.Direction.x << ", " << dir.Light.Direction.y << ", "
                   << dir.Light.Direction.z << "]\n";
                ss << "      Color: [" << dir.Light.Color.r << ", " << dir.Light.Color.g << ", " << dir.Light.Color.b
                   << "]\n";
                ss << "      Intensity: " << dir.Light.Intensity << "\n";
            }

            // Point Light Component
            if (entity.HasComponent<FPointLightComponent>()) {
                const auto& point = entity.GetComponent<FPointLightComponent>();
                ss << "    PointLight:\n";
                ss << "      Enabled: " << (point.bEnabled ? "true" : "false") << "\n";
                ss << "      Color: [" << point.Light.Color.r << ", " << point.Light.Color.g << ", "
                   << point.Light.Color.b << "]\n";
                ss << "      Intensity: " << point.Light.Intensity << "\n";
                ss << "      Radius: " << point.Light.Radius << "\n";
            }

            // Spot Light Component
            if (entity.HasComponent<FSpotLightComponent>()) {
                const auto& spot = entity.GetComponent<FSpotLightComponent>();
                ss << "    SpotLight:\n";
                ss << "      Enabled: " << (spot.bEnabled ? "true" : "false") << "\n";
                ss << "      Direction: [" << spot.Light.Direction.x << ", " << spot.Light.Direction.y << ", "
                   << spot.Light.Direction.z << "]\n";
                ss << "      Color: [" << spot.Light.Color.r << ", " << spot.Light.Color.g << ", " << spot.Light.Color.b
                   << "]\n";
                ss << "      Intensity: " << spot.Light.Intensity << "\n";
                ss << "      Radius: " << spot.Light.Radius << "\n";
                ss << "      CutOff: " << spot.Light.CutOff << "\n";
                ss << "      OuterCutOff: " << spot.Light.OuterCutOff << "\n";
            }

            // Text Component
            if (entity.HasComponent<FTextComponent>()) {
                const auto& textComp = entity.GetComponent<FTextComponent>();
                ss << "    Text:\n";
                ss << "      Content: \"" << Utils::EscapeString(textComp.Text) << "\"\n";
                ss << "      Color: [" << textComp.Color.r << ", " << textComp.Color.g << ", " << textComp.Color.b
                   << ", " << textComp.Color.a << "]\n";
                ss << "      Scale: " << textComp.Size << "\n";
            }

            ss << "\n";
        }

        OutText = ss.str();
        return true;
    }

    bool FLevelSerializer::Deserialize(const std::string& InFilePath) {
        auto startTime = std::chrono::high_resolution_clock::now();
        std::ifstream file(InFilePath);
        if (!file.is_open()) {
            LE_CORE_ERROR("FLevelSerializer: Could not open level file '{0}' for loading!", InFilePath);
            return false;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        bool success = DeserializeText(ss.str());
        if (success) {
            auto endTime = std::chrono::high_resolution_clock::now();
            float durationMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
            LE_CORE_INFO("FLevelSerializer: Level loaded complete: '{0}' in {1:.2f} ms", InFilePath, durationMs);
        }
        return success;
    }

    bool FLevelSerializer::DeserializeText(const std::string& InText) {
        if (!m_Scene)
            return false;

        std::stringstream ss(InText);
        std::string line;

        struct FActorData {
            std::string Name = "Entity";
            glm::vec3 Translation{0.0f};
            glm::vec3 Rotation{0.0f};
            glm::vec3 Scale{1.0f};

            bool bHasStaticMeshAsset = false;
            std::string StaticMeshAssetPath = "";
            std::vector<std::pair<uint32_t, std::string>> MaterialOverrides;
            uint32_t CurrentOverrideSlot = 0;

            bool bHasCamera = false;
            bool bCameraPrimary = true;
            float CameraFOV = 45.0f;
            float CameraNear = 0.1f;
            float CameraFar = 1000.0f;

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
            std::string MaterialAssetPath = "";
            TRef<FMaterialInstance> MaterialInstance = nullptr;

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

        std::vector<FActorData> actors;
        FActorData currentActor;
        bool bParsingActor = false;

        std::string currentBlock = "";
        std::string currentSubBlock = "";

        FSkyboxComponent skybox;
        bool bHasSkybox = false;

        while (std::getline(ss, line)) {
            size_t firstChar = line.find_first_not_of(" \t\r\n");
            if (firstChar == std::string::npos || line[firstChar] == '#' || line[firstChar] == ';')
                continue;

            std::string trimmed = Utils::Trim(line);

            // Calculate indentation
            size_t indent = line.find_first_not_of(' ');
            if (indent == std::string::npos)
                indent = 0;

            // New Actor entry: "  - Name: ..." or "- Name: ..."
            if (trimmed.rfind("- Name:", 0) == 0 || trimmed.rfind("- Actor:", 0) == 0) {
                if (bParsingActor) {
                    actors.push_back(currentActor);
                }
                currentActor = FActorData();
                bParsingActor = true;
                currentBlock = "Actor";
                currentSubBlock = "";

                size_t colon = trimmed.find(':');
                if (colon != std::string::npos) {
                    currentActor.Name = Utils::CleanValue(trimmed.substr(colon + 1));
                }
                continue;
            }

            size_t colonPos = trimmed.find(':');
            if (colonPos == std::string::npos)
                continue;

            std::string key = Utils::Trim(trimmed.substr(0, colonPos));
            std::string value = Utils::Trim(trimmed.substr(colonPos + 1));

            // Section Block Headers
            if (value.empty()) {
                if (indent == 0) {
                    currentBlock = key;
                    currentSubBlock = "";
                } else if (indent == 2) {
                    if (currentBlock == "Environment") {
                        currentSubBlock = key;
                    } else if (bParsingActor) {
                        currentSubBlock = key;
                    }
                } else if (indent >= 4 && bParsingActor) {
                    currentSubBlock = key;
                }
                continue;
            }

            // Check if key is a list item like "- Slot"
            if (key == "- Slot" || key == "-Slot") {
                key = "Slot";
                currentSubBlock = "MaterialOverrides";
            }

            // Environment / Skybox Block
            if (currentBlock == "Environment" && currentSubBlock == "Skybox") {
                bHasSkybox = true;
                if (key == "Enabled")
                    skybox.bEnabled = Utils::ParseBool(value, true);
                else if (key == "Exposure")
                    skybox.Exposure = Utils::ParseFloat(value, 1.0f);
                else if (key == "SunIntensity")
                    skybox.SunIntensity = Utils::ParseFloat(value, 3.5f);
                else if (key == "EnvironmentIntensity")
                    skybox.EnvironmentIntensity = Utils::ParseFloat(value, 1.2f);
                else if (key == "HDREnvironmentMap" || key == "HDRMap" || key == "HDR") {
                    skybox.HDREnvironmentMapPath = Utils::CleanValue(value);
                    skybox.HDREnvironmentMap = FAssetManager::GetTexture2D(skybox.HDREnvironmentMapPath);
                    skybox.bUseHDREnvironmentMap = (skybox.HDREnvironmentMap != nullptr);
                } else if (key == "UseHDREnvironmentMap")
                    skybox.bUseHDREnvironmentMap = Utils::ParseBool(value, false);
                else if (key == "SkyZenithColor")
                    skybox.SkyZenithColor = Utils::ParseVec3(value, skybox.SkyZenithColor);
                else if (key == "HorizonColor")
                    skybox.HorizonColor = Utils::ParseVec3(value, skybox.HorizonColor);
                else if (key == "GroundColor")
                    skybox.GroundColor = Utils::ParseVec3(value, skybox.GroundColor);
                else if (key == "SunColor")
                    skybox.SunColor = Utils::ParseVec3(value, skybox.SunColor);
                continue;
            }

            // Actor Components
            if (bParsingActor) {
                if (currentSubBlock == "Transform") {
                    if (key == "Translation")
                        currentActor.Translation = Utils::ParseVec3(value, currentActor.Translation);
                    else if (key == "Rotation")
                        currentActor.Rotation = Utils::ParseVec3(value, currentActor.Rotation);
                    else if (key == "Scale")
                        currentActor.Scale = Utils::ParseVec3(value, currentActor.Scale);
                } else if (currentSubBlock == "StaticMesh") {
                    if (key == "Asset" || key == "Path") {
                        currentActor.bHasStaticMeshAsset = true;
                        currentActor.StaticMeshAssetPath = Utils::CleanValue(value);
                    } else if (key == "Type") {
                        currentActor.MeshType = Utils::CleanValue(value);
                        if (currentActor.MeshType.find(".lmesh") != std::string::npos) {
                            currentActor.bHasStaticMeshAsset = true;
                            currentActor.StaticMeshAssetPath = currentActor.MeshType;
                        } else {
                            currentActor.bHasMesh = true;
                        }
                    } else if (key == "Size")
                        currentActor.MeshSize = Utils::ParseFloat(value, 1.0f);
                    else if (key == "Width")
                        currentActor.MeshWidth = Utils::ParseFloat(value, 1.0f);
                    else if (key == "Height")
                        currentActor.MeshHeight = Utils::ParseFloat(value, 1.0f);
                    else if (key == "Depth")
                        currentActor.MeshDepth = Utils::ParseFloat(value, 1.0f);
                    else if (key == "Radius")
                        currentActor.MeshRadius = Utils::ParseFloat(value, 0.5f);
                    else if (key == "SubdivisionsX")
                        currentActor.MeshSubdivX = static_cast<unsigned int>(Utils::ParseInt(value, 24));
                    else if (key == "SubdivisionsZ")
                        currentActor.MeshSubdivZ = static_cast<unsigned int>(Utils::ParseInt(value, 24));
                    else if (key == "Shader")
                        currentActor.ShaderPath = Utils::CleanValue(value);
                    else if (key == "CastShadows")
                        currentActor.bCastShadows = Utils::ParseBool(value, true);
                    else if (key == "ReceiveShadows")
                        currentActor.bReceiveShadows = Utils::ParseBool(value, true);
                    else if (key == "VisibleInReflection")
                        currentActor.bVisibleInReflection = Utils::ParseBool(value, true);
                } else if (currentSubBlock == "MaterialOverrides") {
                    if (key == "Slot")
                        currentActor.CurrentOverrideSlot = static_cast<uint32_t>(Utils::ParseInt(value, 0));
                    else if (key == "Asset" || key == "Path")
                        currentActor.MaterialOverrides.push_back(
                            {currentActor.CurrentOverrideSlot, Utils::CleanValue(value)});
                } else if (currentSubBlock == "Camera") {
                    currentActor.bHasCamera = true;
                    if (key == "Primary")
                        currentActor.bCameraPrimary = Utils::ParseBool(value, true);
                    else if (key == "FOV")
                        currentActor.CameraFOV = Utils::ParseFloat(value, 45.0f);
                    else if (key == "NearPlane" || key == "NearClip")
                        currentActor.CameraNear = Utils::ParseFloat(value, 0.1f);
                    else if (key == "FarPlane" || key == "FarClip")
                        currentActor.CameraFar = Utils::ParseFloat(value, 1000.0f);
                } else if (currentSubBlock == "Material") {
                    currentActor.bHasMaterial = true;
                    if (key == "Asset" || key == "Path" || key == "File") {
                        std::string matPath = Utils::CleanValue(value);
                        currentActor.MaterialAssetPath = matPath;
                        currentActor.MaterialInstance = FAssetManager::CreateMaterialInstance(matPath);
                    } else {
                        if (!currentActor.MaterialInstance) {
                            currentActor.MaterialInstance = FAssetManager::GetDefaultMaterial()->CreateInstance();
                        }
                        if (key == "BaseColor" || key == "AlbedoColor")
                            currentActor.MaterialInstance->SetBaseColor(
                                Utils::ParseVec3(value, currentActor.MaterialInstance->GetBaseColor()));
                        else if (key == "Metallic")
                            currentActor.MaterialInstance->SetMetallic(
                                Utils::ParseFloat(value, currentActor.MaterialInstance->GetMetallic()));
                        else if (key == "Roughness")
                            currentActor.MaterialInstance->SetRoughness(
                                Utils::ParseFloat(value, currentActor.MaterialInstance->GetRoughness()));
                        else if (key == "AO" || key == "AmbientOcclusion")
                            currentActor.MaterialInstance->SetAO(
                                Utils::ParseFloat(value, currentActor.MaterialInstance->GetAO()));
                        else if (key == "NormalScale")
                            currentActor.MaterialInstance->SetNormalScale(
                                Utils::ParseFloat(value, currentActor.MaterialInstance->GetNormalScale()));
                        else if (key == "OcclusionStrength")
                            currentActor.MaterialInstance->SetOcclusionStrength(
                                Utils::ParseFloat(value, currentActor.MaterialInstance->GetOcclusionStrength()));
                        else if (key == "EmissiveColor")
                            currentActor.MaterialInstance->SetEmissiveColor(
                                Utils::ParseVec3(value, currentActor.MaterialInstance->GetEmissiveColor()));
                        else if (key == "EmissiveIntensity")
                            currentActor.MaterialInstance->SetEmissiveIntensity(
                                Utils::ParseFloat(value, currentActor.MaterialInstance->GetEmissiveIntensity()));
                        else if (key == "AlphaMode")
                            currentActor.MaterialInstance->SetAlphaMode(
                                static_cast<EAlphaMode>(Utils::ParseInt(value, 0)));
                        else if (key == "AlphaCutoff")
                            currentActor.MaterialInstance->SetAlphaCutoff(
                                Utils::ParseFloat(value, currentActor.MaterialInstance->GetAlphaCutoff()));
                        else if (key == "UVTiling")
                            currentActor.MaterialInstance->SetUVTiling(
                                Utils::ParseVec2(value, currentActor.MaterialInstance->GetUVTiling()));
                        else if (key == "UVOffset")
                            currentActor.MaterialInstance->SetUVOffset(
                                Utils::ParseVec2(value, currentActor.MaterialInstance->GetUVOffset()));
                        else if (key == "AlbedoMap" || key == "DiffuseMap" || key == "BaseColorMap") {
                            std::string path = Utils::CleanValue(value);
                            currentActor.MaterialInstance->SetTexture(0, FAssetManager::GetTexture2D(path));
                        } else if (key == "NormalMap") {
                            std::string path = Utils::CleanValue(value);
                            currentActor.MaterialInstance->SetTexture(1, FAssetManager::GetTexture2D(path));
                        } else if (key == "MetallicMap") {
                            std::string path = Utils::CleanValue(value);
                            currentActor.MaterialInstance->SetTexture(2, FAssetManager::GetTexture2D(path));
                        } else if (key == "RoughnessMap") {
                            std::string path = Utils::CleanValue(value);
                            currentActor.MaterialInstance->SetTexture(3, FAssetManager::GetTexture2D(path));
                        } else if (key == "AOMap" || key == "OcclusionMap") {
                            std::string path = Utils::CleanValue(value);
                            currentActor.MaterialInstance->SetTexture(4, FAssetManager::GetTexture2D(path));
                        } else if (key == "EmissiveMap") {
                            std::string path = Utils::CleanValue(value);
                            currentActor.MaterialInstance->SetTexture(5, FAssetManager::GetTexture2D(path));
                        }
                    }
                } else if (currentSubBlock == "DirectionalLight") {
                    currentActor.bHasDirLight = true;
                    if (key == "Enabled")
                        currentActor.bDirLightEnabled = Utils::ParseBool(value, true);
                    else if (key == "Direction")
                        currentActor.DirLight.Direction = Utils::ParseVec3(value, currentActor.DirLight.Direction);
                    else if (key == "Color")
                        currentActor.DirLight.Color = Utils::ParseVec3(value, currentActor.DirLight.Color);
                    else if (key == "Intensity")
                        currentActor.DirLight.Intensity = Utils::ParseFloat(value, 3.5f);
                } else if (currentSubBlock == "PointLight") {
                    currentActor.bHasPointLight = true;
                    if (key == "Enabled")
                        currentActor.bPointLightEnabled = Utils::ParseBool(value, true);
                    else if (key == "Color")
                        currentActor.PointLight.Color = Utils::ParseVec3(value, currentActor.PointLight.Color);
                    else if (key == "Intensity")
                        currentActor.PointLight.Intensity = Utils::ParseFloat(value, 8.0f);
                    else if (key == "Radius")
                        currentActor.PointLight.Radius = Utils::ParseFloat(value, 10.0f);
                } else if (currentSubBlock == "SpotLight") {
                    currentActor.bHasSpotLight = true;
                    if (key == "Enabled")
                        currentActor.bSpotLightEnabled = Utils::ParseBool(value, true);
                    else if (key == "Direction")
                        currentActor.SpotLight.Direction = Utils::ParseVec3(value, currentActor.SpotLight.Direction);
                    else if (key == "Color")
                        currentActor.SpotLight.Color = Utils::ParseVec3(value, currentActor.SpotLight.Color);
                    else if (key == "Intensity")
                        currentActor.SpotLight.Intensity = Utils::ParseFloat(value, 10.0f);
                    else if (key == "Radius")
                        currentActor.SpotLight.Radius = Utils::ParseFloat(value, 10.0f);
                    else if (key == "CutOff")
                        currentActor.SpotLight.CutOff = Utils::ParseFloat(value, 12.5f);
                    else if (key == "OuterCutOff")
                        currentActor.SpotLight.OuterCutOff = Utils::ParseFloat(value, 17.5f);
                } else if (currentSubBlock == "Text") {
                    currentActor.bHasText = true;
                    if (key == "Content" || key == "Text")
                        currentActor.TextComp.Text = Utils::CleanValue(value);
                    else if (key == "Color")
                        currentActor.TextComp.Color = Utils::ParseVec4(value, currentActor.TextComp.Color);
                    else if (key == "Scale" || key == "Size")
                        currentActor.TextComp.Size = Utils::ParseFloat(value, 1.0f);
                }
            }
        }

        if (bParsingActor) {
            actors.push_back(currentActor);
        }

        // 3. Populate Scene ECS
        m_Scene->GetRegistry().clear();

        // Environment / Skybox Entity
        if (bHasSkybox) {
            FEntity envEntity = m_Scene->CreateEntity("Environment Skybox");
            envEntity.AddComponent<FSkyboxComponent>(skybox);
        }

        // Spawn actors
        for (const auto& actorData : actors) {
            FEntity entity = m_Scene->CreateEntity(actorData.Name);

            // Transform
            auto& transform = entity.GetComponent<FTransformComponent>();
            transform.Translation = actorData.Translation;
            transform.Rotation = actorData.Rotation;
            transform.Scale = actorData.Scale;

            // Camera Component
            if (actorData.bHasCamera) {
                FPerspectiveCamera camera(actorData.CameraFOV, 1280.0f / 720.0f, actorData.CameraNear,
                                          actorData.CameraFar);
                auto& cam = entity.AddComponent<FCameraComponent>(camera);
                cam.bPrimary = actorData.bCameraPrimary;
            }

            // Static Mesh Component
            if (actorData.bHasStaticMeshAsset) {
                auto mesh = FAssetManager::GetStaticMesh(actorData.StaticMeshAssetPath);
                if (mesh) {
                    auto& comp = entity.AddComponent<FStaticMeshComponent>(mesh, actorData.StaticMeshAssetPath);
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
                    LE_CORE_WARN("FLevelSerializer: Failed to load static mesh '{0}' for actor '{1}'",
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

                if (va) {
                    auto& comp = entity.AddComponent<FMeshComponent>(va, shader);
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
            }

            // Material Component
            if (actorData.bHasMaterial && actorData.MaterialInstance) {
                entity.AddComponent<FMaterialComponent>(actorData.MaterialInstance, actorData.MaterialAssetPath);
            }

            // Directional Light
            if (actorData.bHasDirLight) {
                auto& comp = entity.AddComponent<FDirectionalLightComponent>(actorData.DirLight);
                comp.bEnabled = actorData.bDirLightEnabled;
            }

            // Point Light
            if (actorData.bHasPointLight) {
                auto& comp = entity.AddComponent<FPointLightComponent>(actorData.PointLight);
                comp.bEnabled = actorData.bPointLightEnabled;
                comp.Light.Position = actorData.Translation;
            }

            // Spot Light
            if (actorData.bHasSpotLight) {
                auto& comp = entity.AddComponent<FSpotLightComponent>(actorData.SpotLight);
                comp.bEnabled = actorData.bSpotLightEnabled;
                comp.Light.Position = actorData.Translation;
            }

            // Text Component
            if (actorData.bHasText) {
                entity.AddComponent<FTextComponent>(actorData.TextComp);
            }
        }

        LE_CORE_INFO("FLevelSerializer: Instantiated {0} actors from level file", actors.size());
        return true;
    }

} // namespace Leon
