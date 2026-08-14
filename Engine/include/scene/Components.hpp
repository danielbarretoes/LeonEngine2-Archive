#pragma once

#include "core/Base.hpp"
#include "renderer/Buffer.hpp"
#include "renderer/Light.hpp"
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

    using TagComponent = FTagComponent;

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

    using TransformComponent = FTransformComponent;

    struct FMeshComponent {
        TRef<FVertexArray> VertexArray;
        TRef<FShader> Shader;
        TRef<FTexture2D> Texture;
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        float Tiling = 1.0f;
        bool bUseTexture = true;
        bool bCastShadows = true;
        bool bReceiveShadows = true;
        bool bVisibleInReflection = true;

        FMeshComponent() = default;
        FMeshComponent(const FMeshComponent&) = default;
        FMeshComponent(const TRef<FVertexArray>& InVertexArray, const TRef<FShader>& InShader)
            : VertexArray(InVertexArray), Shader(InShader) {}
    };

    using MeshComponent = FMeshComponent;

    struct FPBRMaterial {
        glm::vec3 AlbedoColor{1.0f, 1.0f, 1.0f};
        float Metallic = 0.0f;
        float Roughness = 0.5f;
        float AO = 1.0f;

        TRef<FTexture2D> AlbedoMap;
        TRef<FTexture2D> NormalMap;
        TRef<FTexture2D> MetallicMap;
        TRef<FTexture2D> RoughnessMap;
        TRef<FTexture2D> AOMap;

        bool bUseAlbedoMap = false;
        bool bUseNormalMap = false;
        bool bUseMetallicMap = false;
        bool bUseRoughnessMap = false;
        bool bUseAOMap = false;
        bool bUsePlanarReflection = false;
    };

    using PBRMaterial = FPBRMaterial;

    struct FPBRMaterialComponent {
        FPBRMaterial Material;

        FPBRMaterialComponent() = default;
        FPBRMaterialComponent(const FPBRMaterialComponent&) = default;
        FPBRMaterialComponent(const FPBRMaterial& InMaterial) : Material(InMaterial) {}
    };

    using PBRMaterialComponent = FPBRMaterialComponent;

    struct FDirectionalLightComponent {
        FDirectionalLight Light;
        bool bEnabled = true;

        FDirectionalLightComponent() = default;
        FDirectionalLightComponent(const FDirectionalLightComponent&) = default;
        FDirectionalLightComponent(const FDirectionalLight& InLight) : Light(InLight) {}
    };

    using DirectionalLightComponent = FDirectionalLightComponent;

    struct FPointLightComponent {
        FPointLight Light;
        bool bEnabled = true;

        FPointLightComponent() = default;
        FPointLightComponent(const FPointLightComponent&) = default;
        FPointLightComponent(const FPointLight& InLight) : Light(InLight) {}
    };

    using PointLightComponent = FPointLightComponent;

    struct FSpotLightComponent {
        FSpotLight Light;
        bool bEnabled = true;

        FSpotLightComponent() = default;
        FSpotLightComponent(const FSpotLightComponent&) = default;
        FSpotLightComponent(const FSpotLight& InLight) : Light(InLight) {}
    };

    using SpotLightComponent = FSpotLightComponent;

    struct FCameraComponent {
        FPerspectiveCamera Camera{45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f};
        bool bPrimary = true;

        FCameraComponent() = default;
        FCameraComponent(const FCameraComponent&) = default;
        FCameraComponent(const FPerspectiveCamera& InCamera) : Camera(InCamera) {}
    };

    using CameraComponent = FCameraComponent;

    struct FSkyboxComponent {
        bool bEnabled = true;
        float Exposure = 1.0f;
        float SunIntensity = 3.5f;
        float EnvironmentIntensity = 1.2f;

        TRef<FTexture2D> HDREnvironmentMap;
        bool bUseHDREnvironmentMap = false;

        glm::vec3 SkyZenithColor{0.18f, 0.44f, 0.88f}; // Deep Atmospheric Sky Blue
        glm::vec3 HorizonColor{0.78f, 0.84f, 0.95f};   // Soft Horizon Haze
        glm::vec3 GroundColor{0.22f, 0.24f, 0.28f};    // Muted Ground Reflectance
        glm::vec3 SunColor{1.0f, 0.98f, 0.92f};        // Solar Warm White

        FSkyboxComponent() = default;
        FSkyboxComponent(const FSkyboxComponent&) = default;
    };

    using SkyboxComponent = FSkyboxComponent;

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

    using TextComponent = FTextComponent;

} // namespace Leon
