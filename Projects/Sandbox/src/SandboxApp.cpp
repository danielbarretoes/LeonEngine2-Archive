#include "LeonEngine.hpp"
#include "OpenGLRenderDriver.hpp"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class FLightingShowcaseLayer : public Leon::FLayer {
public:
    FLightingShowcaseLayer()
        : FLayer("LightingShowcaseLayer"), m_CameraController(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f) {}

    void OnAttach() override {
        LE_INFO("FLightingShowcaseLayer attached! Initializing Cook-Torrance PBR & Atmospheric Skybox Pipeline.");
        LE_INFO("Controls (Keyboard & Mouse):");
        LE_INFO("  - W, A, S, D: Move Forward / Left / Backward / Right");
        LE_INFO("  - Space / LeftControl (or E / Q): Move Up / Down");
        LE_INFO("  - LeftShift (Hold): 2.5x Speed Boost");
        LE_INFO("  - Right Click (Hold & Drag) or Left Click: Rotate View (Pitch / Yaw)");
        LE_INFO("  - Mouse Scroll: Adjust Field of View (Zoom)");
        LE_INFO("Controls (Xbox / Gamepad):");
        LE_INFO("  - Left Stick: 3D Movement");
        LE_INFO("  - Right Stick: Camera Look (Pitch / Yaw)");
        LE_INFO("  - Right Trigger (RT) / A: Fly Up");
        LE_INFO("  - Left Trigger (LT) / B: Fly Down");
        LE_INFO("  - F1: Toggle Real-time Performance HUD Stats (FPS, RAM, GPU, Tris, Draw Calls)");
        LE_INFO("  - F2: Toggle 3D Light Debug Gizmos (Spot Cones, Point Attenuation Sphere, Sun Vector)");

        // 1. Initialize Offscreen Render Target (FFramebuffer)
        Leon::FFramebufferSpecification fbSpec;
        fbSpec.Width = 1280;
        fbSpec.Height = 720;
        fbSpec.Attachments = {Leon::EFramebufferTextureFormat::RGBA8, Leon::EFramebufferTextureFormat::Depth};
        m_Framebuffer = Leon::FFramebuffer::Create(fbSpec);

        // 2. Initialize Scene (ECS)
        m_Scene = Leon::FScene::Create();

        // 3. Load Shaders & PBR Textures
        m_PBRShader = Leon::FShader::Create("Engine/Assets/Shaders/PBR_Lit.glsl");
        m_DefaultLitShader = Leon::FShader::Create("Engine/Assets/Shaders/DefaultLit.glsl");

        m_ContainerTexture = Leon::FTexture2D::Create("Projects/Sandbox/Assets/Textures/T_Container_D.png");
        m_MetalPlatesNormalMap = Leon::FTexture2D::Create("Projects/Sandbox/Assets/Textures/T_MetalPlates_N.png");
        m_TilesNormalMap = Leon::FTexture2D::Create("Projects/Sandbox/Assets/Textures/T_Tiles_N.png");

        // 4. Create Geometric Mesh Primitives (Using Default Standard 1.0 Unit Modular Dimensions)
        m_CubeVA = Leon::FMeshPrimitives::CreateCube();
        m_CylinderVA = Leon::FMeshPrimitives::CreateCylinder();
        m_SphereVA = Leon::FMeshPrimitives::CreateSphere();
        m_PlaneVA = Leon::FMeshPrimitives::CreatePlane(24.0f, 24.0f, 24, 24);
        m_RampVA = Leon::FMeshPrimitives::CreateRamp();
        m_PyramidVA = Leon::FMeshPrimitives::CreatePyramid();

        // 5. Populate PBR & Environment Entities
        // 5.0 Atmospheric HDR Skybox Environment
        {
            m_SkyboxEntity = m_Scene->CreateEntity("Atmospheric Skybox");
            Leon::FSkyboxComponent skybox;
            skybox.bEnabled = true;
            skybox.Exposure = 1.0f;
            skybox.SunIntensity = 3.5f;
            skybox.EnvironmentIntensity = 1.2f;
            skybox.SkyZenithColor = glm::vec3(0.18f, 0.44f, 0.88f);
            skybox.HorizonColor = glm::vec3(0.78f, 0.84f, 0.95f);
            skybox.GroundColor = glm::vec3(0.22f, 0.24f, 0.28f);
            skybox.SunColor = glm::vec3(1.0f, 0.98f, 0.92f);
            m_SkyboxEntity.AddComponent<Leon::FSkyboxComponent>(skybox);
        }

        // 5.1 Glossy Ground Plane with Tile Normal Map (Reflective Shadow Receiver)
        {
            m_PlaneEntity = m_Scene->CreateEntity("PBR Ground Plane");
            auto& transform = m_PlaneEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = glm::vec3(0.0f, 0.0f, 0.0f);

            m_PlaneEntity.AddComponent<Leon::FMeshComponent>(m_PlaneVA, m_PBRShader);

            Leon::FPBRMaterial mat;
            mat.AlbedoColor = glm::vec3(0.75f, 0.78f, 0.82f);
            mat.NormalMap = m_TilesNormalMap;
            mat.bUseNormalMap = true;
            mat.Metallic = 0.08f;
            mat.Roughness = 0.18f; // Glossy reflective floor
            m_PlaneEntity.AddComponent<Leon::FPBRMaterialComponent>(mat);
        }

        // 5.2 Emerald PBR Ramp with Metal Panel Normal Map (Far Left)
        {
            m_RampEntity = m_Scene->CreateEntity("PBR Emerald Ramp");
            auto& transform = m_RampEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = glm::vec3(-4.0f, 0.5f, 0.0f);
            transform.Rotation = glm::vec3(0.0f, -25.0f, 0.0f);

            m_RampEntity.AddComponent<Leon::FMeshComponent>(m_RampVA, m_PBRShader);

            Leon::FPBRMaterial mat;
            mat.AlbedoColor = glm::vec3(0.05f, 0.85f, 0.45f);
            mat.NormalMap = m_MetalPlatesNormalMap;
            mat.bUseNormalMap = true;
            mat.Metallic = 0.25f;
            mat.Roughness = 0.15f; // Crisp reflections perturbed by normal grooves
            m_RampEntity.AddComponent<Leon::FPBRMaterialComponent>(mat);
        }

        // 5.3 Textured PBR Rotating Cube with Diffuse + Normal Map (Left)
        {
            m_CubeEntity = m_Scene->CreateEntity("PBR Textured Cube");
            auto& transform = m_CubeEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = glm::vec3(-2.4f, 0.5f, 0.0f);

            m_CubeEntity.AddComponent<Leon::FMeshComponent>(m_CubeVA, m_PBRShader);

            Leon::FPBRMaterial mat;
            mat.AlbedoColor = glm::vec3(1.0f);
            mat.AlbedoMap = m_ContainerTexture;
            mat.bUseAlbedoMap = true;
            mat.NormalMap = m_MetalPlatesNormalMap;
            mat.bUseNormalMap = true;
            mat.Metallic = 0.65f;
            mat.Roughness = 0.18f;
            m_CubeEntity.AddComponent<Leon::FPBRMaterialComponent>(mat);
        }

        // 5.4 Polished Mirror Gold PBR Sphere (Center-Left)
        {
            m_GoldSphereEntity = m_Scene->CreateEntity("PBR Polished Gold Sphere");
            auto& transform = m_GoldSphereEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = glm::vec3(-0.8f, 0.5f, 0.0f);

            m_GoldSphereEntity.AddComponent<Leon::FMeshComponent>(m_SphereVA, m_PBRShader);

            Leon::FPBRMaterial mat;
            mat.AlbedoColor = glm::vec3(1.00f, 0.78f, 0.34f); // Pure Gold Base Reflectance
            mat.Metallic = 1.0f;
            mat.Roughness = 0.05f; // Mirror-like sky & horizon reflections
            m_GoldSphereEntity.AddComponent<Leon::FPBRMaterialComponent>(mat);
        }

        // 5.5 Glossy Ruby Dielectric PBR Sphere (Center-Right)
        {
            m_RedSphereEntity = m_Scene->CreateEntity("PBR Glossy Ruby Sphere");
            auto& transform = m_RedSphereEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = glm::vec3(0.8f, 0.5f, 0.0f);

            m_RedSphereEntity.AddComponent<Leon::FMeshComponent>(m_SphereVA, m_PBRShader);

            Leon::FPBRMaterial mat;
            mat.AlbedoColor = glm::vec3(0.92f, 0.08f, 0.12f);
            mat.Metallic = 0.0f;
            mat.Roughness = 0.12f; // Glossy dielectric reflection
            m_RedSphereEntity.AddComponent<Leon::FPBRMaterialComponent>(mat);
        }

        // 5.6 Rough Brushed Iron Cylinder (Right)
        {
            m_CylinderEntity = m_Scene->CreateEntity("PBR Brushed Iron Cylinder");
            auto& transform = m_CylinderEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = glm::vec3(2.4f, 0.5f, 0.0f);

            m_CylinderEntity.AddComponent<Leon::FMeshComponent>(m_CylinderVA, m_PBRShader);

            Leon::FPBRMaterial mat;
            mat.AlbedoColor = glm::vec3(0.56f, 0.57f, 0.58f); // Iron Base Reflectance
            mat.NormalMap = m_MetalPlatesNormalMap;
            mat.bUseNormalMap = true;
            mat.Metallic = 0.95f;
            mat.Roughness = 0.22f;
            m_CylinderEntity.AddComponent<Leon::FPBRMaterialComponent>(mat);
        }

        // 5.7 Cobalt Mirror Metallic PBR Square Pyramid (Far Right)
        {
            m_PyramidEntity = m_Scene->CreateEntity("PBR Cobalt Pyramid");
            auto& transform = m_PyramidEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = glm::vec3(4.0f, 0.5f, 0.0f);
            transform.Rotation = glm::vec3(0.0f, 35.0f, 0.0f);

            m_PyramidEntity.AddComponent<Leon::FMeshComponent>(m_PyramidVA, m_PBRShader);

            Leon::FPBRMaterial mat;
            mat.AlbedoColor = glm::vec3(0.18f, 0.45f, 0.95f);
            mat.NormalMap = m_TilesNormalMap;
            mat.bUseNormalMap = true;
            mat.Metallic = 0.90f;
            mat.Roughness = 0.08f; // Crisp geometric reflections with beveled normal tile seams
            m_PyramidEntity.AddComponent<Leon::FPBRMaterialComponent>(mat);
        }

        // 6. Configure Light Source Entities
        // 6.1 Directional Sunlight (Shadow Caster)
        {
            Leon::FDirectionalLight dirLight;
            dirLight.Direction = glm::vec3(-0.45f, -1.0f, -0.35f);
            dirLight.Color = glm::vec3(1.0f, 0.98f, 0.92f);
            dirLight.AmbientIntensity = 0.10f;
            dirLight.DiffuseIntensity = 3.5f;
            dirLight.SpecularIntensity = 1.0f;

            m_DirLightEntity = m_Scene->CreateEntity("Directional Sunlight");
            m_DirLightEntity.AddComponent<Leon::FDirectionalLightComponent>(dirLight);
        }

        // 6.2 Orbiting Point Light (Warm Amber)
        {
            Leon::FPointLight pointLight;
            pointLight.Color = glm::vec3(1.0f, 0.55f, 0.15f);
            pointLight.Constant = 1.0f;
            pointLight.Linear = 0.14f;
            pointLight.Quadratic = 0.07f;
            pointLight.AmbientIntensity = 0.05f;
            pointLight.DiffuseIntensity = 4.0f;
            pointLight.SpecularIntensity = 1.0f;

            m_PointLightEntity = m_Scene->CreateEntity("Orbiting Point Light");
            m_PointLightEntity.GetComponent<Leon::FTransformComponent>().Translation = glm::vec3(3.2f, 1.4f, 0.0f);
            m_PointLightEntity.AddComponent<Leon::FPointLightComponent>(pointLight);
        }

        // 6.3 Dramatic Spotlight (Cyan Stage Light)
        {
            Leon::FSpotLight spotLight;
            spotLight.Position = glm::vec3(0.0f, 4.2f, 0.0f);
            spotLight.Direction = glm::vec3(0.0f, -1.0f, 0.0f);
            spotLight.Color = glm::vec3(0.2f, 0.85f, 1.0f);
            spotLight.CutOff = 18.0f;
            spotLight.OuterCutOff = 26.0f;
            spotLight.Constant = 1.0f;
            spotLight.Linear = 0.09f;
            spotLight.Quadratic = 0.032f;
            spotLight.AmbientIntensity = 0.0f;
            spotLight.DiffuseIntensity = 4.5f;
            spotLight.SpecularIntensity = 1.0f;

            m_SpotLightEntity = m_Scene->CreateEntity("Dramatic Spotlight");
            m_SpotLightEntity.GetComponent<Leon::FTransformComponent>().Translation = spotLight.Position;
            m_SpotLightEntity.AddComponent<Leon::FSpotLightComponent>(spotLight);
        }

        // ==========================================
        // 7. 3D In-World Text Actors (Unreal Engine ATextRenderActor style)
        // ==========================================
        // 7.1 Floating Scene Banner Title
        {
            auto titleText = m_Scene->CreateEntity("Text Actor - Scene Title");
            auto& transform = titleText.GetComponent<Leon::FTransformComponent>();
            transform.Translation = glm::vec3(0.0f, 2.8f, -1.8f);

            Leon::FTextComponent textComp;
            textComp.Text = "LEON ENGINE 2 - PHYSICAL PBR";
            textComp.Color = glm::vec4(0.35f, 0.88f, 1.0f, 1.0f);
            textComp.Size = 0.32f;
            textComp.Alignment = Leon::ETextAlignment::Center;
            titleText.AddComponent<Leon::FTextComponent>(textComp);
        }

        // 7.2 Sub-Header Concept Description
        {
            auto subtitleText = m_Scene->CreateEntity("Text Actor - Subtitle");
            auto& transform = subtitleText.GetComponent<Leon::FTransformComponent>();
            transform.Translation = glm::vec3(0.0f, 2.35f, -1.8f);

            Leon::FTextComponent textComp;
            textComp.Text = "Cook-Torrance Specular | Atmospheric IBL | Dynamic Shadows | Tangent Normal Maps";
            textComp.Color = glm::vec4(0.85f, 0.88f, 0.92f, 0.9f);
            textComp.Size = 0.13f;
            textComp.Alignment = Leon::ETextAlignment::Center;
            subtitleText.AddComponent<Leon::FTextComponent>(textComp);
        }

        // 7.3 3D Spatial Primitive Labels floating directly above each actor in world space
        struct FPrimitiveLabel {
            glm::vec3 Pos;
            std::string Name;
            glm::vec4 Color;
        };

        std::vector<FPrimitiveLabel> labels = {
            {glm::vec3(-4.0f, 1.25f, 0.0f), "EMERALD RAMP\n(Modular Wedge)", glm::vec4(0.15f, 1.0f, 0.55f, 1.0f)},
            {glm::vec3(-2.4f, 1.25f, 0.0f), "TEXTURED CUBE\n(Metal Panel NMap)", glm::vec4(1.0f, 0.85f, 0.30f, 1.0f)},
            {glm::vec3(-0.8f, 1.25f, 0.0f), "POLISHED GOLD\n(Metallic 1.0)", glm::vec4(1.0f, 0.92f, 0.45f, 1.0f)},
            {glm::vec3(0.8f, 1.25f, 0.0f), "RUBY DIELECTRIC\n(Dielectric F0)", glm::vec4(1.0f, 0.35f, 0.45f, 1.0f)},
            {glm::vec3(2.4f, 1.25f, 0.0f), "BRUSHED IRON\n(Brushed Roughness)", glm::vec4(0.78f, 0.85f, 0.95f, 1.0f)},
            {glm::vec3(4.0f, 1.25f, 0.0f), "COBALT PYRAMID\n(Square Base)", glm::vec4(0.40f, 0.75f, 1.0f, 1.0f)}};

        for (const auto& lbl : labels) {
            auto labelEntity = m_Scene->CreateEntity("Label - " + lbl.Name);
            auto& transform = labelEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = lbl.Pos;

            Leon::FTextComponent textComp;
            textComp.Text = lbl.Name;
            textComp.Color = lbl.Color;
            textComp.Size = 0.11f;
            textComp.Alignment = Leon::ETextAlignment::Center;
            labelEntity.AddComponent<Leon::FTextComponent>(textComp);
        }

        // Set initial camera position looking down at the stage
        m_CameraController.GetCamera().SetPosition({0.0f, 4.0f, 7.5f});
        m_CameraController.GetCamera().SetRotation(-22.0f, -90.0f);
    }

    void OnDetach() override { LE_INFO("FLightingShowcaseLayer detached."); }

    void OnUpdate(Leon::FTimestep InTs) override {
        // Synchronize viewport and framebuffer on window resize
        uint32_t winWidth = Leon::FApplication::Get().GetWindow().GetWidth();
        uint32_t winHeight = Leon::FApplication::Get().GetWindow().GetHeight();
        if (winWidth > 0 && winHeight > 0 &&
            (m_Framebuffer->GetSpecification().Width != winWidth ||
             m_Framebuffer->GetSpecification().Height != winHeight)) {
            m_Framebuffer->Resize(winWidth, winHeight);
            m_CameraController.GetCamera().SetViewportSize(winWidth, winHeight);
            m_Scene->OnViewportResize(winWidth, winHeight);
        }

        // Update Camera Controller with user input (WASD + Mouse + Gamepad)
        m_CameraController.OnUpdate(InTs);

        // Update Entity Animations & Transformations
        m_TimeAccumulator += InTs.GetSeconds();

        // 1. Rotate Cube Entity
        if (m_CubeEntity) {
            auto& transform = m_CubeEntity.GetComponent<Leon::FTransformComponent>();
            transform.Rotation += glm::vec3(18.0f, 30.0f, 12.0f) * InTs.GetSeconds();
        }

        // 2. Rotate Cylinder Entity
        if (m_CylinderEntity) {
            auto& transform = m_CylinderEntity.GetComponent<Leon::FTransformComponent>();
            transform.Rotation.y += 22.0f * InTs.GetSeconds();
        }

        // 3. Float Spheres slightly up and down
        if (m_GoldSphereEntity) {
            auto& transform = m_GoldSphereEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation.y = 0.7f + std::sin(m_TimeAccumulator * 2.0f) * 0.15f;
            transform.Rotation.y += 15.0f * InTs.GetSeconds();
        }
        if (m_RedSphereEntity) {
            auto& transform = m_RedSphereEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation.y = 0.7f + std::cos(m_TimeAccumulator * 2.0f) * 0.15f;
            transform.Rotation.y -= 15.0f * InTs.GetSeconds();
        }

        // 4. Slowly rotate Pyramid
        if (m_PyramidEntity) {
            auto& transform = m_PyramidEntity.GetComponent<Leon::FTransformComponent>();
            transform.Rotation.y += 18.0f * InTs.GetSeconds();
        }

        // 5. Orbit Point Light Entity
        if (m_PointLightEntity) {
            float orbitRadius = 3.4f;
            glm::vec3 newPos = glm::vec3(std::cos(m_TimeAccumulator * 1.4f) * orbitRadius, 1.5f,
                                         std::sin(m_TimeAccumulator * 1.4f) * orbitRadius);

            auto& transform = m_PointLightEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = newPos;

            auto& pointLightComp = m_PointLightEntity.GetComponent<Leon::FPointLightComponent>();
            pointLightComp.Light.Position = newPos;
        }

        // ==========================================
        // Offscreen Framebuffer Render Pass
        // ==========================================
        m_Framebuffer->Bind();

        Leon::FRenderCommand::SetClearColor(0.04f, 0.05f, 0.07f, 1.0f);
        Leon::FRenderCommand::Clear();

        // Render entire ECS Scene (Depth Shadow Pre-Pass + PBR Main Pass + Atmospheric Skybox)
        m_Scene->OnRender(m_CameraController.GetCamera());

        // Render 3D Light Debug Gizmos (F2 Toggle) inside the Framebuffer
        if (Leon::FApplication::Get().IsLightGizmosEnabled()) {
            Leon::FDebugRenderer::BeginScene(m_CameraController.GetCamera());

            if (m_PointLightEntity) {
                const auto& pointComp = m_PointLightEntity.GetComponent<Leon::FPointLightComponent>();
                Leon::FDebugRenderer::DrawPointLightGizmo(pointComp.Light);
            }

            if (m_SpotLightEntity) {
                const auto& spotComp = m_SpotLightEntity.GetComponent<Leon::FSpotLightComponent>();
                Leon::FDebugRenderer::DrawSpotLightGizmo(spotComp.Light);
            }

            if (m_DirLightEntity) {
                const auto& dirComp = m_DirLightEntity.GetComponent<Leon::FDirectionalLightComponent>();
                Leon::FDebugRenderer::DrawDirectionalLightGizmo(dirComp.Light, glm::vec3(0.0f, 3.5f, 0.0f));
            }

            Leon::FDebugRenderer::EndScene();
        }

        m_Framebuffer->Unbind();

        // ==========================================
        // Present Offscreen Framebuffer to Window
        // ==========================================
        m_Framebuffer->BlitToDefault(winWidth, winHeight);
    }

    void OnEvent(Leon::FEvent& InEvent) override { m_CameraController.OnEvent(InEvent); }

private:
    Leon::TRef<Leon::FFramebuffer> m_Framebuffer;
    Leon::TRef<Leon::FScene> m_Scene;

    Leon::TRef<Leon::FShader> m_PBRShader;
    Leon::TRef<Leon::FShader> m_DefaultLitShader;

    // Textures & Normal Maps
    Leon::TRef<Leon::FTexture2D> m_ContainerTexture;
    Leon::TRef<Leon::FTexture2D> m_MetalPlatesNormalMap;
    Leon::TRef<Leon::FTexture2D> m_TilesNormalMap;

    // Primitives
    Leon::TRef<Leon::FVertexArray> m_CubeVA;
    Leon::TRef<Leon::FVertexArray> m_CylinderVA;
    Leon::TRef<Leon::FVertexArray> m_SphereVA;
    Leon::TRef<Leon::FVertexArray> m_PlaneVA;
    Leon::TRef<Leon::FVertexArray> m_RampVA;
    Leon::TRef<Leon::FVertexArray> m_PyramidVA;

    // Entities
    Leon::FEntity m_SkyboxEntity;
    Leon::FEntity m_RampEntity;
    Leon::FEntity m_CubeEntity;
    Leon::FEntity m_GoldSphereEntity;
    Leon::FEntity m_RedSphereEntity;
    Leon::FEntity m_CylinderEntity;
    Leon::FEntity m_PyramidEntity;
    Leon::FEntity m_PlaneEntity;
    Leon::FEntity m_DirLightEntity;
    Leon::FEntity m_PointLightEntity;
    Leon::FEntity m_SpotLightEntity;

    // Camera
    Leon::FPerspectiveCameraController m_CameraController;

    float m_TimeAccumulator = 0.0f;
};

class FSandboxApp : public Leon::FApplication {
public:
    FSandboxApp()
        : Leon::FApplication(Leon::FApplicationProps{"LeonEngine2 - PBR, Normal Maps & Reflections", 1280, 720}) {
        PushLayer(new FLightingShowcaseLayer());
    }

    ~FSandboxApp() override = default;
};

Leon::FApplication* Leon::CreateApplication() {
    Leon::FOpenGLRenderDriver::Register();
    return new FSandboxApp();
}
