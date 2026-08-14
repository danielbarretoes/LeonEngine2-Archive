#include "scene/SceneSerializer.hpp"
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

    FSceneSerializer::FSceneSerializer(const TRef<FScene>& InScene) : m_Scene(InScene) {}

    bool FSceneSerializer::Serialize(const std::string& InFilePath) {
        std::string text;
        if (!SerializeText(text))
            return false;

        std::ofstream file(InFilePath);
        if (!file.is_open()) {
            LE_CORE_ERROR("FSceneSerializer: Could not open '{0}' for writing!", InFilePath);
            return false;
        }

        file << text;
        LE_CORE_INFO("FSceneSerializer: Saved level to '{0}'", InFilePath);
        return true;
    }

    bool FSceneSerializer::SerializeText(std::string& OutText) {
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

        ss << "Actors:\n";

        // 2. Serialize All Actors
        auto view = m_Scene->GetRegistry().view<FTagComponent, FTransformComponent>();
        for (auto entity : view) {
            auto [tag, transform] = view.get<FTagComponent, FTransformComponent>(entity);

            // Skip Skybox Entity since it's already serialized in Environment
            if (m_Scene->GetRegistry().all_of<FSkyboxComponent>(entity))
                continue;

            ss << "  - Name: \"" << tag.Tag << "\"\n";
            ss << "    Transform:\n";
            ss << "      Translation: [" << transform.Translation.x << ", " << transform.Translation.y << ", "
               << transform.Translation.z << "]\n";
            ss << "      Rotation: [" << transform.Rotation.x << ", " << transform.Rotation.y << ", "
               << transform.Rotation.z << "]\n";
            ss << "      Scale: [" << transform.Scale.x << ", " << transform.Scale.y << ", " << transform.Scale.z
               << "]\n";

            // StaticMesh Component
            if (m_Scene->GetRegistry().all_of<FMeshComponent>(entity)) {
                const auto& mesh = m_Scene->GetRegistry().get<FMeshComponent>(entity);
                ss << "    StaticMesh:\n";
                ss << "      Type: \"" << mesh.MeshType << "\"\n";
                if (mesh.MeshType == "Plane") {
                    ss << "      Width: " << mesh.MeshWidth << "\n";
                    ss << "      Depth: " << mesh.MeshDepth << "\n";
                    ss << "      SubdivisionsX: " << mesh.MeshSubdivX << "\n";
                    ss << "      SubdivisionsZ: " << mesh.MeshSubdivZ << "\n";
                } else if (mesh.MeshType == "Sphere") {
                    ss << "      Radius: " << mesh.MeshRadius << "\n";
                } else if (mesh.MeshType == "Cylinder") {
                    ss << "      Radius: " << mesh.MeshRadius << "\n";
                    ss << "      Height: " << mesh.MeshHeight << "\n";
                } else if (mesh.MeshType == "Ramp" || mesh.MeshType == "Pyramid") {
                    ss << "      Width: " << mesh.MeshWidth << "\n";
                    ss << "      Height: " << mesh.MeshHeight << "\n";
                    ss << "      Depth: " << mesh.MeshDepth << "\n";
                } else {
                    ss << "      Size: " << mesh.MeshSize << "\n";
                }
                if (!mesh.ShaderPath.empty()) {
                    ss << "      Shader: \"" << mesh.ShaderPath << "\"\n";
                }
                ss << "      CastShadows: " << (mesh.bCastShadows ? "true" : "false") << "\n";
                ss << "      ReceiveShadows: " << (mesh.bReceiveShadows ? "true" : "false") << "\n";
                ss << "      VisibleInReflection: " << (mesh.bVisibleInReflection ? "true" : "false") << "\n";
            }

            // Material Component
            if (m_Scene->GetRegistry().all_of<FMaterialComponent>(entity)) {
                const auto& matComp = m_Scene->GetRegistry().get<FMaterialComponent>(entity);
                ss << "    Material:\n";
                if (!matComp.AssetPath.empty()) {
                    ss << "      Asset: \"" << matComp.AssetPath << "\"\n";
                } else if (matComp.MaterialInstance) {
                    const auto& inst = matComp.MaterialInstance;
                    glm::vec3 col = inst->GetAlbedoColor();
                    ss << "      AlbedoColor: [" << col.r << ", " << col.g << ", " << col.b << "]\n";
                    ss << "      Metallic: " << inst->GetMetallic() << "\n";
                    ss << "      Roughness: " << inst->GetRoughness() << "\n";
                    ss << "      AO: " << inst->GetAO() << "\n";
                    if (inst->GetEmissiveIntensity() > 0.0f) {
                        glm::vec3 em = inst->GetEmissiveColor();
                        ss << "      EmissiveColor: [" << em.r << ", " << em.g << ", " << em.b << "]\n";
                        ss << "      EmissiveIntensity: " << inst->GetEmissiveIntensity() << "\n";
                    }
                    if (inst->GetTexture(0))
                        ss << "      AlbedoMap: \"" << inst->GetTexture(0)->GetPath() << "\"\n";
                    if (inst->GetTexture(1))
                        ss << "      NormalMap: \"" << inst->GetTexture(1)->GetPath() << "\"\n";
                    if (inst->GetTexture(2))
                        ss << "      MetallicMap: \"" << inst->GetTexture(2)->GetPath() << "\"\n";
                    if (inst->GetTexture(3))
                        ss << "      AOMap: \"" << inst->GetTexture(3)->GetPath() << "\"\n";
                    if (inst->GetTexture(4))
                        ss << "      RoughnessMap: \"" << inst->GetTexture(4)->GetPath() << "\"\n";
                    if (inst->GetTexture(5))
                        ss << "      EmissiveMap: \"" << inst->GetTexture(5)->GetPath() << "\"\n";
                    ss << "      UsePlanarReflection: " << (inst->GetUsePlanarReflection() ? "true" : "false") << "\n";
                }
            }

            // Directional Light Component
            if (m_Scene->GetRegistry().all_of<FDirectionalLightComponent>(entity)) {
                const auto& comp = m_Scene->GetRegistry().get<FDirectionalLightComponent>(entity);
                ss << "    DirectionalLight:\n";
                ss << "      Enabled: " << (comp.bEnabled ? "true" : "false") << "\n";
                ss << "      Direction: [" << comp.Light.Direction.x << ", " << comp.Light.Direction.y << ", "
                   << comp.Light.Direction.z << "]\n";
                ss << "      Color: [" << comp.Light.Color.r << ", " << comp.Light.Color.g << ", " << comp.Light.Color.b
                   << "]\n";
                ss << "      Intensity: " << comp.Light.Intensity << "\n";
            }

            // Point Light Component
            if (m_Scene->GetRegistry().all_of<FPointLightComponent>(entity)) {
                const auto& comp = m_Scene->GetRegistry().get<FPointLightComponent>(entity);
                ss << "    PointLight:\n";
                ss << "      Enabled: " << (comp.bEnabled ? "true" : "false") << "\n";
                ss << "      Color: [" << comp.Light.Color.r << ", " << comp.Light.Color.g << ", " << comp.Light.Color.b
                   << "]\n";
                ss << "      Intensity: " << comp.Light.Intensity << "\n";
                ss << "      Radius: " << comp.Light.Radius << "\n";
            }

            // Spot Light Component
            if (m_Scene->GetRegistry().all_of<FSpotLightComponent>(entity)) {
                const auto& comp = m_Scene->GetRegistry().get<FSpotLightComponent>(entity);
                ss << "    SpotLight:\n";
                ss << "      Enabled: " << (comp.bEnabled ? "true" : "false") << "\n";
                ss << "      Direction: [" << comp.Light.Direction.x << ", " << comp.Light.Direction.y << ", "
                   << comp.Light.Direction.z << "]\n";
                ss << "      Color: [" << comp.Light.Color.r << ", " << comp.Light.Color.g << ", " << comp.Light.Color.b
                   << "]\n";
                ss << "      Intensity: " << comp.Light.Intensity << "\n";
                ss << "      Radius: " << comp.Light.Radius << "\n";
                ss << "      CutOff: " << comp.Light.CutOff << "\n";
                ss << "      OuterCutOff: " << comp.Light.OuterCutOff << "\n";
            }

            // Text Component
            if (m_Scene->GetRegistry().all_of<FTextComponent>(entity)) {
                const auto& textComp = m_Scene->GetRegistry().get<FTextComponent>(entity);
                ss << "    Text:\n";
                ss << "      Content: \"" << Utils::EscapeString(textComp.Text) << "\"\n";
                ss << "      Color: [" << textComp.Color.r << ", " << textComp.Color.g << ", " << textComp.Color.b
                   << ", " << textComp.Color.a << "]\n";
                ss << "      Size: " << textComp.Size << "\n";
                ss << "      Alignment: "
                   << (textComp.Alignment == ETextAlignment::Left
                           ? "Left"
                           : (textComp.Alignment == ETextAlignment::Right ? "Right" : "Center"))
                   << "\n";
                ss << "      DoubleSided: " << (textComp.bDoubleSided ? "true" : "false") << "\n";
            }

            ss << "\n";
        }

        OutText = ss.str();
        return true;
    }

    bool FSceneSerializer::Deserialize(const std::string& InFilePath) {
        auto startTime = std::chrono::high_resolution_clock::now();
        std::ifstream file(InFilePath);
        if (!file.is_open()) {
            LE_CORE_ERROR("FSceneSerializer: Could not open level file '{0}' for loading!", InFilePath);
            return false;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        bool success = DeserializeText(ss.str());
        if (success) {
            auto endTime = std::chrono::high_resolution_clock::now();
            float durationMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
            LE_CORE_INFO("FSceneSerializer: Level loaded complete: '{0}' in {1:.2f} ms", InFilePath, durationMs);
        }
        return success;
    }

    bool FSceneSerializer::DeserializeText(const std::string& InText) {
        if (!m_Scene)
            return false;

        std::stringstream ss(InText);
        std::string line;

        struct FActorData {
            std::string Name = "Entity";
            glm::vec3 Translation{0.0f};
            glm::vec3 Rotation{0.0f};
            glm::vec3 Scale{1.0f};

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
            TRef<FMaterialInstance> MaterialInstance;
            std::string MaterialAssetPath;

            bool bHasDirLight = false;
            FDirectionalLight DirLight;
            bool bDirLightEnabled = true;

            bool bHasPointLight = false;
            FPointLight PointLight;
            bool bPointLightEnabled = true;

            bool bHasSpotLight = false;
            FSpotLight SpotLight;
            bool bSpotLightEnabled = true;

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
            // Remove comments
            size_t commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }

            std::string trimmed = Utils::Trim(line);
            if (trimmed.empty())
                continue;

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
                } else if (indent == 4 && bParsingActor) {
                    currentSubBlock = key;
                }
                continue;
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
                    currentActor.bHasMesh = true;
                    if (key == "Type")
                        currentActor.MeshType = Utils::CleanValue(value);
                    else if (key == "Size")
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
                        if (key == "Shader") {
                            currentActor.ShaderPath = Utils::CleanValue(value);
                        } else if (key == "AlbedoColor") {
                            currentActor.MaterialInstance->SetAlbedoColor(Utils::ParseVec3(value, currentActor.MaterialInstance->GetAlbedoColor()));
                        } else if (key == "Metallic") {
                            currentActor.MaterialInstance->SetMetallic(Utils::ParseFloat(value, 0.0f));
                        } else if (key == "Roughness") {
                            currentActor.MaterialInstance->SetRoughness(Utils::ParseFloat(value, 0.5f));
                        } else if (key == "AO") {
                            currentActor.MaterialInstance->SetAO(Utils::ParseFloat(value, 1.0f));
                        } else if (key == "EmissiveColor") {
                            currentActor.MaterialInstance->SetEmissiveColor(Utils::ParseVec3(value, currentActor.MaterialInstance->GetEmissiveColor()));
                        } else if (key == "EmissiveIntensity") {
                            currentActor.MaterialInstance->SetEmissiveIntensity(Utils::ParseFloat(value, 0.0f));
                        } else if (key == "AlbedoMap") {
                            currentActor.MaterialInstance->SetTexture(0, FAssetManager::GetTexture2D(Utils::CleanValue(value)));
                        } else if (key == "NormalMap") {
                            currentActor.MaterialInstance->SetTexture(1, FAssetManager::GetTexture2D(Utils::CleanValue(value)));
                        } else if (key == "MetallicMap") {
                            currentActor.MaterialInstance->SetTexture(2, FAssetManager::GetTexture2D(Utils::CleanValue(value)));
                        } else if (key == "AOMap") {
                            currentActor.MaterialInstance->SetTexture(3, FAssetManager::GetTexture2D(Utils::CleanValue(value)));
                        } else if (key == "RoughnessMap") {
                            currentActor.MaterialInstance->SetTexture(4, FAssetManager::GetTexture2D(Utils::CleanValue(value)));
                        } else if (key == "EmissiveMap") {
                            currentActor.MaterialInstance->SetTexture(5, FAssetManager::GetTexture2D(Utils::CleanValue(value)));
                        } else if (key == "UsePlanarReflection") {
                            currentActor.MaterialInstance->SetUsePlanarReflection(Utils::ParseBool(value, false));
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
                    else if (key == "Intensity" || key == "DiffuseIntensity")
                        currentActor.DirLight.Intensity = Utils::ParseFloat(value, 3.5f);
                } else if (currentSubBlock == "PointLight") {
                    currentActor.bHasPointLight = true;
                    if (key == "Enabled")
                        currentActor.bPointLightEnabled = Utils::ParseBool(value, true);
                    else if (key == "Color")
                        currentActor.PointLight.Color = Utils::ParseVec3(value, currentActor.PointLight.Color);
                    else if (key == "Intensity" || key == "DiffuseIntensity")
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
                    else if (key == "Intensity" || key == "DiffuseIntensity")
                        currentActor.SpotLight.Intensity = Utils::ParseFloat(value, 10.0f);
                    else if (key == "Radius")
                        currentActor.SpotLight.Radius = Utils::ParseFloat(value, 10.0f);
                    else if (key == "CutOff")
                        currentActor.SpotLight.CutOff = Utils::ParseFloat(value, 18.0f);
                    else if (key == "OuterCutOff")
                        currentActor.SpotLight.OuterCutOff = Utils::ParseFloat(value, 26.0f);
                } else if (currentSubBlock == "Text") {
                    currentActor.bHasText = true;
                    if (key == "Content" || key == "Text")
                        currentActor.TextComp.Text = Utils::CleanValue(value);
                    else if (key == "Color")
                        currentActor.TextComp.Color = Utils::ParseVec4(value, currentActor.TextComp.Color);
                    else if (key == "Size")
                        currentActor.TextComp.Size = Utils::ParseFloat(value, 1.0f);
                    else if (key == "Alignment") {
                        std::string align = Utils::CleanValue(value);
                        if (align == "Left")
                            currentActor.TextComp.Alignment = ETextAlignment::Left;
                        else if (align == "Right")
                            currentActor.TextComp.Alignment = ETextAlignment::Right;
                        else
                            currentActor.TextComp.Alignment = ETextAlignment::Center;
                    } else if (key == "DoubleSided")
                        currentActor.TextComp.bDoubleSided = Utils::ParseBool(value, true);
                }
            }
        }

        if (bParsingActor) {
            actors.push_back(currentActor);
        }

        // ==========================================
        // Instantiate Deserialized Scene Hierarchy
        // ==========================================

        // 1. Create Skybox Actor if found in environment
        if (bHasSkybox) {
            auto skyboxEntity = m_Scene->CreateEntity("Atmospheric Skybox");
            skyboxEntity.AddComponent<FSkyboxComponent>(skybox);
        }

        // 2. Instantiate Actors
        for (const auto& actorData : actors) {
            auto entity = m_Scene->CreateEntity(actorData.Name);
            auto& transform = entity.GetComponent<FTransformComponent>();
            transform.Translation = actorData.Translation;
            transform.Rotation = actorData.Rotation;
            transform.Scale = actorData.Scale;

            // Mesh Primitive Creation
            if (actorData.bHasMesh) {
                TRef<FVertexArray> va;
                if (actorData.MeshType == "Plane") {
                    va = FMeshPrimitives::CreatePlane(actorData.MeshWidth > 0 ? actorData.MeshWidth : 24.0f,
                                                      actorData.MeshDepth > 0 ? actorData.MeshDepth : 24.0f,
                                                      actorData.MeshSubdivX, actorData.MeshSubdivZ);
                } else if (actorData.MeshType == "Sphere") {
                    va = FMeshPrimitives::CreateSphere(actorData.MeshRadius > 0 ? actorData.MeshRadius : 0.5f);
                } else if (actorData.MeshType == "Cylinder") {
                    va = FMeshPrimitives::CreateCylinder(actorData.MeshRadius > 0 ? actorData.MeshRadius : 0.5f,
                                                         actorData.MeshRadius > 0 ? actorData.MeshRadius : 0.5f,
                                                         actorData.MeshHeight > 0 ? actorData.MeshHeight : 1.0f);
                } else if (actorData.MeshType == "Ramp") {
                    va = FMeshPrimitives::CreateRamp(actorData.MeshWidth, actorData.MeshHeight, actorData.MeshDepth);
                } else if (actorData.MeshType == "Pyramid") {
                    va =
                        FMeshPrimitives::CreatePyramid(actorData.MeshWidth, actorData.MeshHeight, actorData.MeshDepth);
                } else {
                    // Default Cube
                    va = FMeshPrimitives::CreateCube(actorData.MeshSize > 0 ? actorData.MeshSize : 1.0f);
                }

                TRef<FShader> shader = actorData.ShaderPath.empty()
                                           ? FAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl")
                                           : FAssetManager::GetShader(actorData.ShaderPath);
                auto& meshComp = entity.AddComponent<FMeshComponent>(va, shader);
                meshComp.MeshType = actorData.MeshType;
                meshComp.MeshSize = actorData.MeshSize;
                meshComp.MeshWidth = actorData.MeshWidth;
                meshComp.MeshHeight = actorData.MeshHeight;
                meshComp.MeshDepth = actorData.MeshDepth;
                meshComp.MeshRadius = actorData.MeshRadius;
                meshComp.MeshSubdivX = actorData.MeshSubdivX;
                meshComp.MeshSubdivZ = actorData.MeshSubdivZ;
                meshComp.ShaderPath = actorData.ShaderPath;
                meshComp.bCastShadows = actorData.bCastShadows;
                meshComp.bReceiveShadows = actorData.bReceiveShadows;
                meshComp.bVisibleInReflection = actorData.bVisibleInReflection;
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

        LE_CORE_INFO("FSceneSerializer: Instantiated {0} actors from level file", actors.size());
        return true;
    }

} // namespace Leon
