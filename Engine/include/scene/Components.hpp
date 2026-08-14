#pragma once

#include "core/Base.hpp"
#include "renderer/Buffer.hpp"
#include "renderer/Light.hpp"
#include "renderer/Material.hpp"
#include "renderer/MaterialInstance.hpp"
#include "renderer/PerspectiveCamera.hpp"
#include "renderer/Shader.hpp"
#include "renderer/Texture.hpp"
#include "renderer/VertexArray.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <string>

namespace Leon {

    struct FTagComponent {
        std::string Tag;

        FTagComponent() = default;
        FTagComponent(const FTagComponent&) = default;
        FTagComponent(const std::string& InTag) : Tag(InTag) {}
    };

    struct FTransformComponent {
        glm::vec3 Translation{0.0f, 0.0f, 0.0f};
        glm::vec3 Rotation{0.0f, 0.0f, 0.0f}; // Euler angles in degrees
        glm::vec3 Scale{1.0f, 1.0f, 1.0f};

        FTransformComponent() = default;
        FTransformComponent(const FTransformComponent&) = default;
        FTransformComponent(const glm::vec3& InTranslation) : Translation(InTranslation) {}

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

        // Mesh geometry metadata for lossless scene serialization
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
     * @brief Render Material Component.
     *
     * Holds a shared reference to an FMaterialInstance. The entity does not copy
     * material data; all properties and textures are resolved through the instance hierarchy.
     */
    struct FMaterialComponent {
        TRef<FMaterialInstance> MaterialInstance;
        std::string AssetPath; // Track source .lmat file if loaded from asset

        FMaterialComponent() = default;
        FMaterialComponent(const FMaterialComponent&) = default;
        explicit FMaterialComponent(const TRef<FMaterialInstance>& InInstance, const std::string& InAssetPath = "")
            : MaterialInstance(InInstance), AssetPath(InAssetPath) {}
    };

    // Backwards-compatible alias for existing layers/serializers
    using FPBRMaterialComponent = FMaterialComponent;

    struct FDirectionalLightComponent {
        FDirectionalLight Light;
        bool bEnabled = true;

        FDirectionalLightComponent() = default;
        FDirectionalLightComponent(const FDirectionalLightComponent&) = default;
        FDirectionalLightComponent(const FDirectionalLight& InLight) : Light(InLight) {}
    };

    struct FPointLightComponent {
        FPointLight Light;
        bool bEnabled = true;

        FPointLightComponent() = default;
        FPointLightComponent(const FPointLightComponent&) = default;
        FPointLightComponent(const FPointLight& InLight) : Light(InLight) {}
    };

    struct FSpotLightComponent {
        FSpotLight Light;
        bool bEnabled = true;

        FSpotLightComponent() = default;
        FSpotLightComponent(const FSpotLightComponent&) = default;
        FSpotLightComponent(const FSpotLight& InLight) : Light(InLight) {}
    };

    struct FCameraComponent {
        FPerspectiveCamera Camera{45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f};
        bool bPrimary = true;

        FCameraComponent() = default;
        FCameraComponent(const FCameraComponent&) = default;
        FCameraComponent(const FPerspectiveCamera& InCamera) : Camera(InCamera) {}
    };

    struct FSkyboxComponent {
        bool bEnabled = true;
        float Exposure = 1.0f;
        float SunIntensity = 3.5f;
        float EnvironmentIntensity = 1.2f;

        TRef<FTexture2D> HDREnvironmentMap;
        std::string HDREnvironmentMapPath;
        bool bUseHDREnvironmentMap = false;

        glm::vec3 SkyZenithColor{0.18f, 0.44f, 0.88f}; // Deep Atmospheric Sky Blue
        glm::vec3 HorizonColor{0.78f, 0.84f, 0.95f};   // Soft Horizon Haze
        glm::vec3 GroundColor{0.22f, 0.24f, 0.28f};    // Muted Ground Reflectance
        glm::vec3 SunColor{1.0f, 0.98f, 0.92f};        // Solar Warm White

        FSkyboxComponent() = default;
        FSkyboxComponent(const FSkyboxComponent&) = default;
    };

    /**
     * @brief Horizontal text justification for 3D In-World Text Actors.
     */
    enum class ETextAlignment : uint8_t { Left = 0, Center = 1, Right = 2 };

    /**
     * @brief 3D In-World Text Mesh Component (analogous to Unreal Engine UTextRenderComponent).
     * Placed with an FTransformComponent to live in 3D world space at fixed coordinates.
     */
    struct FTextComponent {
        std::string Text = "Text";
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        float Size = 1.0f;        // Height in world space units
        float LineSpacing = 1.2f; // Line height multiplier
        ETextAlignment Alignment = ETextAlignment::Center;
        bool bDoubleSided = true; // Whether text is readable from both sides

        FTextComponent() = default;
        FTextComponent(const FTextComponent&) = default;
        FTextComponent(const std::string& InText, const glm::vec4& InColor = glm::vec4(1.0f), float InSize = 1.0f,
                       ETextAlignment InAlignment = ETextAlignment::Center)
            : Text(InText), Color(InColor), Size(InSize), Alignment(InAlignment) {}
    };

} // namespace Leon
