#include "engine/LeonEngine.hpp"
#include "opengl/OpenGLRenderDriver.hpp"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class FLightingShowcaseLayer : public Leon::FLayer {
public:
    FLightingShowcaseLayer()
        : FLayer("LightingShowcaseLayer"), m_CameraController(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f) {}

    void OnAttach() override {
        LE_INFO("FLightingShowcaseLayer attached! Initializing multi-primitive scene and 3-light setup.");
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

        // 1. Load Multi-Stage Multi-Light Shader (DefaultLit)
        m_Shader = Leon::FShader::Create("Engine/Assets/Shaders/DefaultLit.glsl");

        // 2. Load 2D Texture from Project Assets
        m_Texture = Leon::FTexture2D::Create("Projects/Sandbox/Assets/Textures/T_Container_D.png");

        // 3. Create Geometric Mesh Primitives
        m_CubeVA = Leon::FMeshPrimitives::CreateCube(1.0f);
        m_CylinderVA = Leon::FMeshPrimitives::CreateCylinder(0.5f, 0.5f, 1.2f, 32, true);
        m_SphereVA = Leon::FMeshPrimitives::CreateSphere(0.6f, 32, 16);
        m_PlaneVA = Leon::FMeshPrimitives::CreatePlane(12.0f, 12.0f, 16, 16);

        // 4. Configure Light Sources
        // Directional Sunlight
        m_DirLight.Direction = glm::vec3(-0.4f, -1.0f, -0.3f);
        m_DirLight.Color = glm::vec3(0.9f, 0.95f, 1.0f);
        m_DirLight.AmbientIntensity = 0.12f;
        m_DirLight.DiffuseIntensity = 0.5f;
        m_DirLight.SpecularIntensity = 0.3f;

        // Orbiting Point Light (Warm Amber)
        m_PointLight.Color = glm::vec3(1.0f, 0.55f, 0.15f);
        m_PointLight.Constant = 1.0f;
        m_PointLight.Linear = 0.14f;
        m_PointLight.Quadratic = 0.07f;
        m_PointLight.AmbientIntensity = 0.05f;
        m_PointLight.DiffuseIntensity = 1.2f;
        m_PointLight.SpecularIntensity = 1.0f;

        // Dramatic Spot Light (Cyan)
        m_SpotLight.Position = glm::vec3(0.0f, 3.5f, 0.0f);
        m_SpotLight.Direction = glm::vec3(0.0f, -1.0f, 0.0f);
        m_SpotLight.Color = glm::vec3(0.2f, 0.85f, 1.0f);
        m_SpotLight.CutOff = 15.0f;
        m_SpotLight.OuterCutOff = 22.5f;
        m_SpotLight.Constant = 1.0f;
        m_SpotLight.Linear = 0.09f;
        m_SpotLight.Quadratic = 0.032f;
        m_SpotLight.AmbientIntensity = 0.0f;
        m_SpotLight.DiffuseIntensity = 2.0f;
        m_SpotLight.SpecularIntensity = 2.0f;

        // Set initial camera position looking down at the stage
        m_CameraController.GetCamera().SetPosition({0.0f, 3.2f, 5.0f});
        m_CameraController.GetCamera().SetRotation(-25.0f, -90.0f);
    }

    void OnDetach() override { LE_INFO("FLightingShowcaseLayer detached."); }

    void OnUpdate(Leon::FTimestep InTs) override {
        // Update Camera Controller with user input (WASD + Mouse + Gamepad)
        m_CameraController.OnUpdate(InTs);

        // Update Animations
        m_TimeAccumulator += InTs.GetSeconds();
        m_RotationAngle += InTs.GetSeconds() * 25.0f;

        // Orbit the point light in a circle
        float orbitRadius = 2.8f;
        m_PointLight.Position = glm::vec3(std::cos(m_TimeAccumulator * 1.5f) * orbitRadius, 1.2f,
                                          std::sin(m_TimeAccumulator * 1.5f) * orbitRadius);

        // Clear screen and depth buffer
        Leon::FRenderCommand::SetClearColor(0.05f, 0.06f, 0.09f, 1.0f);
        Leon::FRenderCommand::Clear();

        // Begin Scene with Camera View-Projection
        Leon::FRenderer::BeginScene(m_CameraController.GetCamera());

        m_Shader->Bind();

        // Upload Directional Light Uniforms
        m_Shader->SetInt("u_DirLight.enabled", 1);
        m_Shader->SetFloat3("u_DirLight.direction", m_DirLight.Direction.x, m_DirLight.Direction.y,
                            m_DirLight.Direction.z);
        m_Shader->SetFloat3("u_DirLight.color", m_DirLight.Color.x, m_DirLight.Color.y, m_DirLight.Color.z);
        m_Shader->SetFloat("u_DirLight.ambientIntensity", m_DirLight.AmbientIntensity);
        m_Shader->SetFloat("u_DirLight.diffuseIntensity", m_DirLight.DiffuseIntensity);
        m_Shader->SetFloat("u_DirLight.specularIntensity", m_DirLight.SpecularIntensity);

        // Upload Point Light Uniforms
        m_Shader->SetInt("u_PointLight.enabled", 1);
        m_Shader->SetFloat3("u_PointLight.position", m_PointLight.Position.x, m_PointLight.Position.y,
                            m_PointLight.Position.z);
        m_Shader->SetFloat3("u_PointLight.color", m_PointLight.Color.x, m_PointLight.Color.y, m_PointLight.Color.z);
        m_Shader->SetFloat("u_PointLight.constant", m_PointLight.Constant);
        m_Shader->SetFloat("u_PointLight.linear", m_PointLight.Linear);
        m_Shader->SetFloat("u_PointLight.quadratic", m_PointLight.Quadratic);
        m_Shader->SetFloat("u_PointLight.ambientIntensity", m_PointLight.AmbientIntensity);
        m_Shader->SetFloat("u_PointLight.diffuseIntensity", m_PointLight.DiffuseIntensity);
        m_Shader->SetFloat("u_PointLight.specularIntensity", m_PointLight.SpecularIntensity);

        // Upload Spot Light Uniforms
        m_Shader->SetInt("u_SpotLight.enabled", 1);
        m_Shader->SetFloat3("u_SpotLight.position", m_SpotLight.Position.x, m_SpotLight.Position.y,
                            m_SpotLight.Position.z);
        m_Shader->SetFloat3("u_SpotLight.direction", m_SpotLight.Direction.x, m_SpotLight.Direction.y,
                            m_SpotLight.Direction.z);
        m_Shader->SetFloat3("u_SpotLight.color", m_SpotLight.Color.x, m_SpotLight.Color.y, m_SpotLight.Color.z);
        m_Shader->SetFloat("u_SpotLight.cutOff", std::cos(glm::radians(m_SpotLight.CutOff)));
        m_Shader->SetFloat("u_SpotLight.outerCutOff", std::cos(glm::radians(m_SpotLight.OuterCutOff)));
        m_Shader->SetFloat("u_SpotLight.constant", m_SpotLight.Constant);
        m_Shader->SetFloat("u_SpotLight.linear", m_SpotLight.Linear);
        m_Shader->SetFloat("u_SpotLight.quadratic", m_SpotLight.Quadratic);
        m_Shader->SetFloat("u_SpotLight.ambientIntensity", m_SpotLight.AmbientIntensity);
        m_Shader->SetFloat("u_SpotLight.diffuseIntensity", m_SpotLight.DiffuseIntensity);
        m_Shader->SetFloat("u_SpotLight.specularIntensity", m_SpotLight.SpecularIntensity);

        // 1. Draw Textured Rotating 3D Cube (Left)
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(-2.2f, 0.5f, 0.0f));
            model = glm::rotate(model, glm::radians(m_RotationAngle), glm::vec3(0.5f, 1.0f, 0.2f));
            m_Shader->SetMat4("u_Model", glm::value_ptr(model));

            if (m_Texture && m_Texture->IsLoaded()) {
                m_Texture->Bind(0);
                m_Shader->SetInt("u_DiffuseMap", 0);
                m_Shader->SetInt("u_UseTexture", 1);
            }
            Leon::FRenderer::SubmitIndexed(m_Shader, m_CubeVA);
        }

        // 2. Draw Smooth 3D Cylinder (Center - under Spotlight)
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.6f, 0.0f));
            model = glm::rotate(model, glm::radians(m_RotationAngle * 0.7f), glm::vec3(0.0f, 1.0f, 0.0f));
            m_Shader->SetMat4("u_Model", glm::value_ptr(model));
            m_Shader->SetInt("u_UseTexture", 0);
            Leon::FRenderer::SubmitIndexed(m_Shader, m_CylinderVA);
        }

        // 3. Draw Smooth 3D Sphere (Right)
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(2.2f, 0.6f, 0.0f));
            model = glm::rotate(model, glm::radians(m_RotationAngle * 0.5f), glm::vec3(1.0f, 0.5f, 0.0f));
            m_Shader->SetMat4("u_Model", glm::value_ptr(model));
            m_Shader->SetInt("u_UseTexture", 0);
            Leon::FRenderer::SubmitIndexed(m_Shader, m_SphereVA);
        }

        // 4. Draw Ground Plane Grid
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
            m_Shader->SetMat4("u_Model", glm::value_ptr(model));
            m_Shader->SetInt("u_UseTexture", 0);
            Leon::FRenderer::SubmitIndexed(m_Shader, m_PlaneVA);
        }

        Leon::FRenderer::EndScene();

        // 5. Render 3D Light Debug Gizmos (F2 Toggle)
        if (Leon::FApplication::Get().IsLightGizmosEnabled()) {
            Leon::FDebugRenderer::BeginScene(m_CameraController.GetCamera());
            Leon::FDebugRenderer::DrawPointLightGizmo(m_PointLight);
            Leon::FDebugRenderer::DrawSpotLightGizmo(m_SpotLight);
            Leon::FDebugRenderer::DrawDirectionalLightGizmo(m_DirLight, glm::vec3(0.0f, 3.0f, 0.0f));
            Leon::FDebugRenderer::EndScene();
        }
    }

    void OnEvent(Leon::FEvent& InEvent) override { m_CameraController.OnEvent(InEvent); }

private:
    Leon::TRef<Leon::FShader> m_Shader;
    Leon::TRef<Leon::FTexture2D> m_Texture;

    // Primitives
    Leon::TRef<Leon::FVertexArray> m_CubeVA;
    Leon::TRef<Leon::FVertexArray> m_CylinderVA;
    Leon::TRef<Leon::FVertexArray> m_SphereVA;
    Leon::TRef<Leon::FVertexArray> m_PlaneVA;

    // Camera & Lights
    Leon::FPerspectiveCameraController m_CameraController;
    Leon::FDirectionalLight m_DirLight;
    Leon::FPointLight m_PointLight;
    Leon::FSpotLight m_SpotLight;

    float m_TimeAccumulator = 0.0f;
    float m_RotationAngle = 0.0f;
};

class FSandboxApp : public Leon::FApplication {
public:
    FSandboxApp() : Leon::FApplication(Leon::FApplicationProps{"LeonEngine2 - Multi-Light 3D Scene", 1280, 720}) {
        PushLayer(new FLightingShowcaseLayer());
    }

    ~FSandboxApp() override = default;
};

Leon::FApplication* Leon::CreateApplication() {
    Leon::FOpenGLRenderDriver::Register();
    return new FSandboxApp();
}
