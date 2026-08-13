#include "engine/LeonEngine.hpp"
#include "opengl/OpenGLRenderDriver.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class FCubeLayer : public Leon::FLayer {
public:
    FCubeLayer() : FLayer("Cube3DLayer"), m_CameraController(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f) {}

    void OnAttach() override {
        LE_INFO("FCubeLayer attached! Initializing 3D textured mesh and directional lighting shader.");
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
        LE_INFO("  - Left Thumb (L3) / RB: 2.5x Speed Boost");

        // 1. Load Multi-Stage Shader from Asset File
        m_Shader = Leon::FShader::Create("Assets/Shaders/DirectionalLit.glsl");

        // 2. Load 2D Texture from Disk
        m_Texture = Leon::FTexture2D::Create("Assets/Textures/Container_Diffuse.png");

        // 3. 3D Cube Vertices: 36 vertices (6 faces x 2 triangles x 3 vertices)
        // Format: Position (x,y,z), Normal (nx,ny,nz), TexCoords (u,v), Color (r,g,b)
        float cubeVertices[] = {
            // Front Face (Normal: 0, 0, 1)
            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f,
            0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
            1.0f, 1.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,

            // Back Face (Normal: 0, 0, -1)
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, -0.5f,
            0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f,

            // Top Face (Normal: 0, 1, 0)
            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -0.5f,
            0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f,

            // Bottom Face (Normal: 0, -1, 0)
            -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
            1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f,
            -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f, -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,

            // Left Face (Normal: -1, 0, 0)
            -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -0.5f,
            0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f, -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,

            // Right Face (Normal: 1, 0, 0)
            0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f,
            -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f};

        // 4. Create Vertex Buffer and Layout
        m_VertexArray = Leon::FVertexArray::Create();

        Leon::TRef<Leon::FVertexBuffer> vertexBuffer = Leon::FVertexBuffer::Create(cubeVertices, sizeof(cubeVertices));
        vertexBuffer->SetLayout({{Leon::EShaderDataType::Float3, "aPos"},
                                 {Leon::EShaderDataType::Float3, "aNormal"},
                                 {Leon::EShaderDataType::Float2, "aTexCoord"},
                                 {Leon::EShaderDataType::Float3, "aColor"}});

        m_VertexArray->AddVertexBuffer(vertexBuffer);

        // Set initial camera position looking slightly from above
        m_CameraController.GetCamera().SetPosition({2.2f, 1.8f, 3.2f});
        m_CameraController.GetCamera().SetRotation(-22.0f, -125.0f);
    }

    void OnDetach() override { LE_INFO("FCubeLayer detached."); }

    void OnUpdate(Leon::FTimestep InTs) override {
        // Update Camera Controller with user input (WASD + Mouse + Gamepad)
        m_CameraController.OnUpdate(InTs);

        // Accumulate cube rotation angle
        m_RotationAngle += InTs.GetSeconds() * 20.0f;

        // Clear screen and depth buffer
        Leon::FRenderCommand::SetClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        Leon::FRenderCommand::Clear();

        // Begin Scene with Camera View-Projection
        Leon::FRenderer::BeginScene(m_CameraController.GetCamera());

        m_Shader->Bind();

        // Model transformation: smooth continuous 3D rotation
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians(m_RotationAngle), glm::vec3(0.5f, 1.0f, 0.2f));

        // Upload Model Matrix
        m_Shader->SetMat4("u_Model", glm::value_ptr(model));

        // Directional Light Uniforms (Sunlight direction & warm ambient/diffuse)
        glm::vec3 lightDir(-0.6f, -1.0f, -0.4f);
        m_Shader->SetFloat3("u_LightDirection", lightDir.x, lightDir.y, lightDir.z);
        m_Shader->SetFloat3("u_LightColor", 1.0f, 0.98f, 0.92f);
        m_Shader->SetFloat("u_AmbientIntensity", 0.3f);

        // Bind Texture to Texture Unit 0
        if (m_Texture && m_Texture->IsLoaded()) {
            m_Texture->Bind(0);
            m_Shader->SetInt("u_DiffuseMap", 0);
            m_Shader->SetInt("u_UseTexture", 1);
        } else {
            m_Shader->SetInt("u_UseTexture", 0);
        }

        // Render 3D Cube (36 vertices)
        Leon::FRenderer::Submit(m_Shader, m_VertexArray, 36);

        Leon::FRenderer::EndScene();
    }

    void OnEvent(Leon::FEvent& InEvent) override { m_CameraController.OnEvent(InEvent); }

private:
    Leon::TRef<Leon::FShader> m_Shader;
    Leon::TRef<Leon::FTexture2D> m_Texture;
    Leon::TRef<Leon::FVertexArray> m_VertexArray;
    Leon::FPerspectiveCameraController m_CameraController;
    float m_RotationAngle = 0.0f;
};

class FSandboxApp : public Leon::FApplication {
public:
    FSandboxApp() : Leon::FApplication(Leon::FApplicationProps{"LeonEngine2 - 3D Textured Lighting Scene", 1280, 720}) {
        PushLayer(new FCubeLayer());
    }

    ~FSandboxApp() override = default;
};

Leon::FApplication* Leon::CreateApplication() {
    Leon::FOpenGLRenderDriver::Register();
    return new FSandboxApp();
}
