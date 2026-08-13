#include "engine/LeonEngine.hpp"
#include "plugin_opengl/OpenGLRenderDriver.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class FCubeLayer : public Leon::FLayer {
public:
    FCubeLayer() : FLayer("Cube3DLayer"), m_CameraController(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f) {}

    void OnAttach() override {
        LE_INFO("FCubeLayer attached! Initializing 3D cube mesh and interactive camera controller.");
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

        // 1. Shaders (3D Blinn-Phong with Directional Lighting & ViewProjection matrix)
        const std::string vertexSrc = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec3 aNormal;
            layout (location = 2) in vec3 aColor;

            uniform mat4 u_ViewProjection;
            uniform mat4 u_Model;

            out vec3 v_FragPos;
            out vec3 v_Normal;
            out vec3 v_Color;

            void main() {
                v_FragPos = vec3(u_Model * vec4(aPos, 1.0));
                v_Normal = mat3(transpose(inverse(u_Model))) * aNormal;
                v_Color = aColor;

                gl_Position = u_ViewProjection * vec4(v_FragPos, 1.0);
            }
        )";

        const std::string fragmentSrc = R"(
            #version 330 core
            in vec3 v_FragPos;
            in vec3 v_Normal;
            in vec3 v_Color;

            out vec4 FragColor;

            uniform vec3 u_ViewPos;

            // Directional Light Uniforms
            uniform vec3 u_LightDirection;
            uniform vec3 u_LightColor;
            uniform float u_AmbientIntensity;

            void main() {
                // Ambient Component
                vec3 ambient = u_AmbientIntensity * u_LightColor;

                // Diffuse Component
                vec3 norm = normalize(v_Normal);
                vec3 lightDir = normalize(-u_LightDirection);
                float diff = max(dot(norm, lightDir), 0.0);
                vec3 diffuse = diff * u_LightColor;

                // Specular Component (Blinn-Phong)
                vec3 viewDir = normalize(u_ViewPos - v_FragPos);
                vec3 halfwayDir = normalize(lightDir + viewDir);
                float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
                vec3 specular = 0.5 * spec * u_LightColor;

                // Final Lit Color
                vec3 result = (ambient + diffuse + specular) * v_Color;
                FragColor = vec4(result, 1.0);
            }
        )";

        m_Shader = Leon::FShader::Create("DirectionalLightShader", vertexSrc, fragmentSrc);

        // 2. 3D Cube Vertices: 36 vertices (6 faces x 2 triangles x 3 vertices)
        // Format: Position (x,y,z), Normal (nx,ny,nz), Color (r,g,b)
        float cubeVertices[] = {// Front Face (Normal: 0, 0, 1) - Cyan/Teal
                                -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.2f, 0.7f, 0.9f, 0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
                                1.0f, 0.2f, 0.7f, 0.9f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.2f, 0.7f, 0.9f, 0.5f,
                                0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.2f, 0.7f, 0.9f, -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
                                0.2f, 0.7f, 0.9f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.2f, 0.7f, 0.9f,

                                // Back Face (Normal: 0, 0, -1) - Blue
                                -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.1f, 0.4f, 0.8f, 0.5f, 0.5f, -0.5f, 0.0f, 0.0f,
                                -1.0f, 0.1f, 0.4f, 0.8f, 0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.1f, 0.4f, 0.8f, -0.5f,
                                0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.1f, 0.4f, 0.8f, 0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
                                0.1f, 0.4f, 0.8f, -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.1f, 0.4f, 0.8f,

                                // Top Face (Normal: 0, 1, 0) - Orange
                                -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.9f, 0.6f, 0.2f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
                                0.0f, 0.9f, 0.6f, 0.2f, 0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.9f, 0.6f, 0.2f, -0.5f,
                                0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.9f, 0.6f, 0.2f, -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
                                0.9f, 0.6f, 0.2f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.9f, 0.6f, 0.2f,

                                // Bottom Face (Normal: 0, -1, 0) - Purple
                                -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.6f, 0.2f, 0.8f, 0.5f, -0.5f, -0.5f, 0.0f,
                                -1.0f, 0.0f, 0.6f, 0.2f, 0.8f, 0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.6f, 0.2f, 0.8f,
                                0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.6f, 0.2f, 0.8f, -0.5f, -0.5f, 0.5f, 0.0f, -1.0f,
                                0.0f, 0.6f, 0.2f, 0.8f, -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.6f, 0.2f, 0.8f,

                                // Left Face (Normal: -1, 0, 0) - Green
                                -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.2f, 0.8f, 0.4f, -0.5f, -0.5f, 0.5f, -1.0f,
                                0.0f, 0.0f, 0.2f, 0.8f, 0.4f, -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.2f, 0.8f, 0.4f,
                                -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.2f, 0.8f, 0.4f, -0.5f, 0.5f, -0.5f, -1.0f, 0.0f,
                                0.0f, 0.2f, 0.8f, 0.4f, -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.2f, 0.8f, 0.4f,

                                // Right Face (Normal: 1, 0, 0) - Coral/Red
                                0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.9f, 0.3f, 0.3f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
                                0.0f, 0.9f, 0.3f, 0.3f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.9f, 0.3f, 0.3f, 0.5f,
                                0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.9f, 0.3f, 0.3f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
                                0.9f, 0.3f, 0.3f, 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.9f, 0.3f, 0.3f};

        // 3. Create Vertex Buffer and Layout
        m_VertexArray = Leon::FVertexArray::Create();

        Leon::TRef<Leon::FVertexBuffer> vertexBuffer = Leon::FVertexBuffer::Create(cubeVertices, sizeof(cubeVertices));
        vertexBuffer->SetLayout({{Leon::EShaderDataType::Float3, "aPos"},
                                 {Leon::EShaderDataType::Float3, "aNormal"},
                                 {Leon::EShaderDataType::Float3, "aColor"}});

        m_VertexArray->AddVertexBuffer(vertexBuffer);

        // Set initial camera position slightly elevated looking at the cube
        m_CameraController.GetCamera().SetPosition({2.2f, 1.8f, 3.2f});
        m_CameraController.GetCamera().SetRotation(-22.0f, -125.0f);
    }

    void OnDetach() override { LE_INFO("FCubeLayer detached."); }

    void OnUpdate(Leon::FTimestep InTs) override {
        // Update Camera Controller with user input (WASD + Mouse)
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
        m_Shader->SetFloat("u_AmbientIntensity", 0.25f);

        // Render 3D Cube (36 vertices)
        Leon::FRenderer::Submit(m_Shader, m_VertexArray, 36);

        Leon::FRenderer::EndScene();
    }

    void OnEvent(Leon::FEvent& InEvent) override { m_CameraController.OnEvent(InEvent); }

private:
    Leon::TRef<Leon::FShader> m_Shader;
    Leon::TRef<Leon::FVertexArray> m_VertexArray;
    Leon::FPerspectiveCameraController m_CameraController;
    float m_RotationAngle = 0.0f;
};

class FSandboxApp : public Leon::FApplication {
public:
    FSandboxApp()
        : Leon::FApplication(Leon::FApplicationProps{"LeonEngine2 - Interactive 3D Camera Scene", 1280, 720}) {
        PushLayer(new FCubeLayer());
    }

    ~FSandboxApp() override = default;
};

Leon::FApplication* Leon::CreateApplication() {
    Leon::FOpenGLRenderDriver::Register();
    return new FSandboxApp();
}
