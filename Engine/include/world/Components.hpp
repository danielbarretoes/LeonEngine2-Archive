#pragma once

#include "core/Base.hpp"
#include "renderer/Buffer.hpp"
#include "renderer/Light.hpp"
#include "renderer/Material.hpp"
#include "renderer/MaterialInstance.hpp"
#include "renderer/PerspectiveCamera.hpp"
#include "renderer/Shader.hpp"
#include "renderer/StaticMesh.hpp"
#include "renderer/Texture.hpp"
#include "renderer/VertexArray.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <vector>

namespace Leon {

    struct FTagComponent {
        std::string Tag;

        FTagComponent() = default;
        FTagComponent(const FTagComponent&) = default;
        FTagComponent(const std::string& InTag) : Tag(InTag) {}
    };

    struct FTransformComponent {
        glm::vec3 Translation{0.0f, 0.0f, 0.0f};
        glm::vec3 Rotation{0.0f, 0.0f, 0.0f}; // Euler angles in degrees (Pitch, Yaw, Roll)
        glm::vec3 Scale{1.0f, 1.0f, 1.0f};

        FTransformComponent() = default;
        FTransformComponent(const FTransformComponent&) = default;
        FTransformComponent(const glm::vec3& InTranslation) : Translation(InTranslation) {}
        FTransformComponent(const glm::vec3& InTranslation, const glm::vec3& InRotation, const glm::vec3& InScale)
            : Translation(InTranslation), Rotation(InRotation), Scale(InScale) {}

        glm::mat4 GetTransform() const {
            glm::mat4 rotation = glm::toMat4(glm::quat(glm::radians(Rotation)));
            return glm::translate(glm::mat4(1.0f), Translation) * rotation * glm::scale(glm::mat4(1.0f), Scale);
        }
    };

    struct FMeshComponent {
        TRef<FVertexArray> VertexArray;
        TRef<FShader> Shader;
        bool bCastShadows = true;
        bool bReceiveShadows = true;
        bool bVisibleInReflection = true;

        // Mesh geometry metadata for lossless map serialization
        std::string MeshType = "Cube";
        float MeshSize = 1.0f;
        float MeshWidth = 1.0f;
        float MeshHeight = 1.0f;
        float MeshDepth = 1.0f;
        float MeshRadius = 0.5f;
        unsigned int MeshSubdivX = 24;
        unsigned int MeshSubdivZ = 24;
        std::string ShaderPath = "Engine/Assets/Shaders/PBR_Lit.glsl";

        FMeshComponent() = default;
        FMeshComponent(const FMeshComponent&) = default;
        FMeshComponent(const TRef<FVertexArray>& InVertexArray, const TRef<FShader>& InShader)
            : VertexArray(InVertexArray), Shader(InShader) {}
    };

    /**
     * @brief Unreal Engine aligned StaticMesh component.
     * Holds an FStaticMesh asset reference with material slot overrides.
     */
    struct UStaticMeshComponent {
        TRef<FStaticMesh> StaticMesh = nullptr;
        std::vector<TRef<FMaterialInstance>> MaterialOverrides;
        std::vector<std::string> MaterialOverridePaths;
        std::string AssetPath; // Virtual path to .lmesh
        TRef<FShader> Shader = nullptr;
        bool bCastShadows = true;
        bool bReceiveShadows = true;
        bool bVisibleInReflection = true;

        UStaticMeshComponent() = default;
        UStaticMeshComponent(const UStaticMeshComponent&) = default;
        explicit UStaticMeshComponent(const TRef<FStaticMesh>& InMesh, const std::string& InAssetPath = "")
            : StaticMesh(InMesh), AssetPath(InAssetPath) {}
    };

    struct FMaterialComponent {
        TRef<FMaterialInstance> MaterialInstance;
        std::string AssetPath;

        FMaterialComponent() = default;
        FMaterialComponent(const FMaterialComponent&) = default;
        explicit FMaterialComponent(const TRef<FMaterialInstance>& InInstance, const std::string& InAssetPath = "")
            : MaterialInstance(InInstance), AssetPath(InAssetPath) {}
    };

    /**
     * @brief Unreal Engine aligned Light components.
     */
    struct UDirectionalLightComponent {
        FDirectionalLight Light;
        bool bEnabled = true;

        UDirectionalLightComponent() = default;
        UDirectionalLightComponent(const UDirectionalLightComponent&) = default;
        UDirectionalLightComponent(const FDirectionalLight& InLight) : Light(InLight) {}
    };

    struct UPointLightComponent {
        FPointLight Light;
        bool bEnabled = true;

        UPointLightComponent() = default;
        UPointLightComponent(const UPointLightComponent&) = default;
        UPointLightComponent(const FPointLight& InLight) : Light(InLight) {}
    };

    struct USpotLightComponent {
        FSpotLight Light;
        bool bEnabled = true;

        USpotLightComponent() = default;
        USpotLightComponent(const USpotLightComponent&) = default;
        USpotLightComponent(const FSpotLight& InLight) : Light(InLight) {}
    };

    /**
     * @brief Unreal Engine aligned Camera component.
     */
    struct UCameraComponent {
        FPerspectiveCamera Camera{45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f};
        bool bPrimary = true;

        UCameraComponent() = default;
        UCameraComponent(const UCameraComponent&) = default;
        UCameraComponent(const FPerspectiveCamera& InCamera) : Camera(InCamera) {}
    };

    struct FSkyboxComponent {
        bool bEnabled = true;
        float Exposure = 1.0f;
        float SunIntensity = 3.5f;
        float EnvironmentIntensity = 1.2f;

        TRef<FTexture2D> HDREnvironmentMap;
        std::string HDREnvironmentMapPath;
        bool bUseHDREnvironmentMap = false;

        glm::vec3 SkyZenithColor{0.18f, 0.44f, 0.88f};
        glm::vec3 HorizonColor{0.78f, 0.84f, 0.95f};
        glm::vec3 GroundColor{0.22f, 0.24f, 0.28f};
        glm::vec3 SunColor{1.0f, 0.98f, 0.92f};

        FSkyboxComponent() = default;
        FSkyboxComponent(const FSkyboxComponent&) = default;
    };

    enum class ETextAlignment : uint8_t { Left = 0, Center = 1, Right = 2 };

    struct FTextComponent {
        std::string Text = "Text";
        glm::vec4 Color{0.72f, 0.72f, 0.72f, 1.0f};
        float Size = 1.0f;
        float LineSpacing = 1.2f;
        ETextAlignment Alignment = ETextAlignment::Center;
        bool bDoubleSided = true;

        FTextComponent() = default;
        FTextComponent(const FTextComponent&) = default;
        FTextComponent(const std::string& InText, const glm::vec4& InColor = glm::vec4(0.72f, 0.72f, 0.72f, 1.0f),
                       float InSize = 1.0f, ETextAlignment InAlignment = ETextAlignment::Center)
            : Text(InText), Color(InColor), Size(InSize), Alignment(InAlignment) {}
    };

} // namespace Leon
