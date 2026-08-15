#include "LeonEngine.hpp"
#include "OpenGLRenderDriver.hpp"

#include <chrono>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class FLightingShowcaseLayer : public Leon::FLayer {
public:
    explicit FLightingShowcaseLayer(const std::string& InLevelPath)
        : FLayer("LightingShowcaseLayer"), m_LevelPath(InLevelPath),
          m_CameraController(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f) {}

    void OnAttach() override {
        auto startTime = std::chrono::high_resolution_clock::now();

        LE_INFO("FLightingShowcaseLayer attached! Initializing Cook-Torrance PBR & Atmospheric Skybox Pipeline.");
        LE_INFO("Loading Level from Asset: {0}", m_LevelPath);
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
        LE_INFO("  - F3: Toggle Full Post-Processing Pipeline (ON / OFF)");
        LE_INFO("  - F4: Toggle FXAA Anti-Aliasing (ON / OFF)");
        LE_INFO("  - F5: Cycle Post-Process Debug Mode (Full -> Raw HDR -> Bloom -> Bright-Pass -> Tone Mapping Only)");
        LE_INFO("  - F6 / F7: Adjust Exposure Down / Up (-0.10 / +0.10)");
        LE_INFO("  - F8 / F9: Adjust Bloom Intensity Down / Up (-0.01 / +0.01)");
        LE_INFO("  - Shift + F1:  Full Composite PBR Lit (Default)");
        LE_INFO("  - Shift + F2:  Environment Cubemap (LOD 0)");
        LE_INFO("  - Shift + F3:  Prefilter Cubemap Mip 0 (Roughness 0.0)");
        LE_INFO("  - Shift + F4:  Prefilter Cubemap Mip 1 (Roughness 0.25)");
        LE_INFO("  - Shift + F5:  Prefilter Cubemap Mip 2 (Roughness 0.50)");
        LE_INFO("  - Shift + F6:  Prefilter Cubemap Mip 3 (Roughness 0.75)");
        LE_INFO("  - Shift + F7:  Prefilter Cubemap Mip 4 (Roughness 1.00)");
        LE_INFO("  - Shift + F8:  Diffuse Irradiance Cubemap");
        LE_INFO("  - Shift + F9:  BRDF Look-Up Table (2D LUT)");
        LE_INFO("  - Shift + F10: Specular IBL Only (No Direct Lights)");
        LE_INFO("  - Shift + F11: Direct Lighting Only (No IBL)");
        LE_INFO("  - Shift + F12: Planar Reflection Texture");
        LE_INFO("  - Shift + N:   World-Space Normals N");
        LE_INFO("  - Shift + R:   World-Space Reflection Vector R");

        // 1. Initialize Scene (ECS) and Deserialize Level from .llevel asset file
        m_Scene = Leon::FScene::Create();
        Leon::FSceneSerializer serializer(m_Scene);
        if (!serializer.Deserialize(m_LevelPath)) {
            LE_ERROR("Failed to load startup level: {0}", m_LevelPath);
        }

        // 2. Cache references to dynamic animated entities in the scene
        auto view = m_Scene->GetRegistry().view<Leon::FTagComponent>();
        for (auto entityHandle : view) {
            const auto& tag = view.get<Leon::FTagComponent>(entityHandle);
            Leon::FEntity entity = {entityHandle, m_Scene.get()};
            if (tag.Tag == "PBR Polished Gold Sphere")
                m_GoldSphereEntity = entity;
            else if (tag.Tag == "PBR Glossy Ruby Sphere")
                m_RedSphereEntity = entity;
            else if (tag.Tag == "Orbiting Point Light")
                m_PointLightEntity = entity;
            else if (tag.Tag == "Dramatic Spotlight")
                m_SpotLightEntity = entity;
            else if (tag.Tag == "Directional Sunlight")
                m_DirLightEntity = entity;
        }

        // Set initial camera position looking down at the stage
        m_CameraController.GetCamera().SetPosition({0.0f, 4.0f, 7.5f});
        m_CameraController.GetCamera().SetRotation(-22.0f, -90.0f);

        auto endTime = std::chrono::high_resolution_clock::now();
        float levelLoadDurationMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        float totalFromWindowOpenMs = Leon::FApplication::Get().GetTimeSinceWindowOpenMs();

        LE_INFO("Level loaded complete. Loaded '{0}' in {1:.2f} ms ({2:.2f} ms since window opened).",
                m_LevelPath, levelLoadDurationMs, totalFromWindowOpenMs);
    }

    void OnDetach() override { LE_INFO("FLightingShowcaseLayer detached."); }

    void OnUpdate(Leon::FTimestep InTs) override {
        // Synchronize viewport on window resize
        uint32_t winWidth = Leon::FApplication::Get().GetWindow().GetWidth();
        uint32_t winHeight = Leon::FApplication::Get().GetWindow().GetHeight();
        if (winWidth > 0 && winHeight > 0) {
            m_CameraController.GetCamera().SetViewportSize(winWidth, winHeight);
            m_Scene->OnViewportResize(winWidth, winHeight);
        }

        // Update Camera Controller with user input (WASD + Mouse + Gamepad)
        m_CameraController.OnUpdate(InTs);

        // Update Entity Animations & Transformations
        m_TimeAccumulator += InTs.GetSeconds();

        // 1. Float Spheres slightly up and down
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

        // 2. Orbit Point Light Entity
        if (m_PointLightEntity) {
            float orbitRadius = 3.4f;
            glm::vec3 newPos = glm::vec3(std::cos(m_TimeAccumulator * 1.4f) * orbitRadius, 1.5f,
                                         std::sin(m_TimeAccumulator * 1.4f) * orbitRadius);

            auto& transform = m_PointLightEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = newPos;

            if (m_PointLightEntity.HasComponent<Leon::FPointLightComponent>()) {
                auto& pointLightComp = m_PointLightEntity.GetComponent<Leon::FPointLightComponent>();
                pointLightComp.Light.Position = newPos;
            }
        }

        // ==========================================
        // Render Scene Pipeline (CSM + Spot + Reflection + PBR + Skybox + Post-Process)
        // ==========================================
        m_Scene->OnRender(m_CameraController.GetCamera());

        // Render 3D Light Debug Gizmos (F2 Toggle)
        if (Leon::FApplication::Get().IsLightGizmosEnabled() && m_Scene) {
            Leon::FDebugRenderer::BeginScene(m_CameraController.GetCamera());

            auto& reg = m_Scene->GetRegistry();

            auto pointView = reg.view<Leon::FPointLightComponent>();
            for (auto entity : pointView) {
                const auto& pointComp = pointView.get<Leon::FPointLightComponent>(entity);
                if (pointComp.bEnabled) {
                    Leon::FPointLight light = pointComp.Light;
                    if (reg.all_of<Leon::FTransformComponent>(entity)) {
                        light.Position = reg.get<Leon::FTransformComponent>(entity).Translation;
                    }
                    Leon::FDebugRenderer::DrawPointLightGizmo(light);
                }
            }

            auto spotView = reg.view<Leon::FSpotLightComponent>();
            for (auto entity : spotView) {
                const auto& spotComp = spotView.get<Leon::FSpotLightComponent>(entity);
                if (spotComp.bEnabled) {
                    Leon::FSpotLight light = spotComp.Light;
                    if (reg.all_of<Leon::FTransformComponent>(entity)) {
                        light.Position = reg.get<Leon::FTransformComponent>(entity).Translation;
                    }
                    Leon::FDebugRenderer::DrawSpotLightGizmo(light);
                }
            }

            auto dirView = reg.view<Leon::FDirectionalLightComponent>();
            for (auto entity : dirView) {
                const auto& dirComp = dirView.get<Leon::FDirectionalLightComponent>(entity);
                if (dirComp.bEnabled) {
                    glm::vec3 pos = glm::vec3(0.0f, 3.5f, 0.0f);
                    if (reg.all_of<Leon::FTransformComponent>(entity)) {
                        pos = reg.get<Leon::FTransformComponent>(entity).Translation;
                    }
                    Leon::FDebugRenderer::DrawDirectionalLightGizmo(dirComp.Light, pos);
                }
            }

            Leon::FDebugRenderer::EndScene();
        }
    }

    void OnEvent(Leon::FEvent& InEvent) override {
        m_CameraController.OnEvent(InEvent);

        Leon::FEventDispatcher dispatcher(InEvent);
        dispatcher.Dispatch<Leon::FKeyPressedEvent>([this](Leon::FKeyPressedEvent& e) {
            if (e.IsRepeat() || !m_Scene || !m_Scene->GetSceneRenderer()) return false;

            int key = e.GetKeyCode();
            bool bShift = Leon::FInput::IsKeyPressed(Leon::Key::LeftShift) ||
                          Leon::FInput::IsKeyPressed(Leon::Key::RightShift);
            auto* renderer = m_Scene->GetSceneRenderer();

            if (bShift) {
                if (key == Leon::Key::F1) {
                    renderer->SetDebugMode(0);
                    LE_INFO("[DEBUG VIEW] Full Composite PBR Lit (Default)");
                    return true;
                } else if (key == Leon::Key::F2) {
                    renderer->SetDebugMode(1);
                    LE_INFO("[DEBUG VIEW] Environment Cubemap (LOD 0)");
                    return true;
                } else if (key == Leon::Key::F3) {
                    renderer->SetDebugMode(2);
                    LE_INFO("[DEBUG VIEW] Prefilter Cubemap Mip 0 (Roughness 0.0)");
                    return true;
                } else if (key == Leon::Key::F4) {
                    renderer->SetDebugMode(3);
                    LE_INFO("[DEBUG VIEW] Prefilter Cubemap Mip 1 (Roughness 0.25)");
                    return true;
                } else if (key == Leon::Key::F5) {
                    renderer->SetDebugMode(4);
                    LE_INFO("[DEBUG VIEW] Prefilter Cubemap Mip 2 (Roughness 0.50)");
                    return true;
                } else if (key == Leon::Key::F6) {
                    renderer->SetDebugMode(5);
                    LE_INFO("[DEBUG VIEW] Prefilter Cubemap Mip 3 (Roughness 0.75)");
                    return true;
                } else if (key == Leon::Key::F7) {
                    renderer->SetDebugMode(6);
                    LE_INFO("[DEBUG VIEW] Prefilter Cubemap Mip 4 (Roughness 1.00)");
                    return true;
                } else if (key == Leon::Key::F8) {
                    renderer->SetDebugMode(7);
                    LE_INFO("[DEBUG VIEW] Diffuse Irradiance Cubemap");
                    return true;
                } else if (key == Leon::Key::F9) {
                    renderer->SetDebugMode(8);
                    LE_INFO("[DEBUG VIEW] BRDF Look-Up Table (2D LUT)");
                    return true;
                } else if (key == Leon::Key::F10) {
                    renderer->SetDebugMode(9);
                    LE_INFO("[DEBUG VIEW] Specular IBL Only (No Direct Lights)");
                    return true;
                } else if (key == Leon::Key::F11) {
                    renderer->SetDebugMode(10);
                    LE_INFO("[DEBUG VIEW] Direct Lighting Only (No IBL)");
                    return true;
                } else if (key == Leon::Key::F12) {
                    renderer->SetDebugMode(13);
                    LE_INFO("[DEBUG VIEW] Planar Reflection Texture");
                    return true;
                } else if (key == Leon::Key::N) {
                    renderer->SetDebugMode(11);
                    LE_INFO("[DEBUG VIEW] World-Space Normals N");
                    return true;
                } else if (key == Leon::Key::R) {
                    renderer->SetDebugMode(12);
                    LE_INFO("[DEBUG VIEW] World-Space Reflection Vector R");
                    return true;
                }
            } else {
                auto& ppSettings = renderer->GetPostProcessSettings();
                if (key == Leon::Key::F3) {
                    ppSettings.bEnabled = !ppSettings.bEnabled;
                    LE_INFO("[POST-PROCESS] Pipeline {0}", ppSettings.bEnabled ? "ENABLED" : "DISABLED (Raw Linear Pass-through)");
                    return true;
                } else if (key == Leon::Key::F4) {
                    ppSettings.bFXAAEnabled = !ppSettings.bFXAAEnabled;
                    LE_INFO("[POST-PROCESS] FXAA Anti-Aliasing {0}", ppSettings.bFXAAEnabled ? "ENABLED" : "DISABLED");
                    return true;
                } else if (key == Leon::Key::F5) {
                    ppSettings.DebugMode = (ppSettings.DebugMode + 1) % 5;
                    const char* modeNames[] = {
                        "Full Composite Post-Process (Bloom + ACES + FXAA)",
                        "Raw HDR Scene (Before Post-Processing)",
                        "Bloom Glow Output Only",
                        "Bright-Pass High-Luminance Extract Only",
                        "Tone Mapping Output Only (No FXAA)"
                    };
                    LE_INFO("[POST-PROCESS DEBUG] Mode {0}: {1}", ppSettings.DebugMode, modeNames[ppSettings.DebugMode]);
                    return true;
                } else if (key == Leon::Key::F6) {
                    ppSettings.Exposure = std::max(ppSettings.Exposure - 0.1f, 0.1f);
                    LE_INFO("[POST-PROCESS] Exposure: {0:.2f}", ppSettings.Exposure);
                    return true;
                } else if (key == Leon::Key::F7) {
                    ppSettings.Exposure += 0.1f;
                    LE_INFO("[POST-PROCESS] Exposure: {0:.2f}", ppSettings.Exposure);
                    return true;
                } else if (key == Leon::Key::F8) {
                    ppSettings.BloomIntensity = std::max(ppSettings.BloomIntensity - 0.01f, 0.0f);
                    LE_INFO("[POST-PROCESS] Bloom Intensity: {0:.3f}", ppSettings.BloomIntensity);
                    return true;
                } else if (key == Leon::Key::F9) {
                    ppSettings.BloomIntensity += 0.01f;
                    LE_INFO("[POST-PROCESS] Bloom Intensity: {0:.3f}", ppSettings.BloomIntensity);
                    return true;
                }
            }
            return false;
        });
    }

private:
    std::string m_LevelPath;
    Leon::TRef<Leon::FScene> m_Scene;

    // Entity References
    Leon::FEntity m_GoldSphereEntity;
    Leon::FEntity m_RedSphereEntity;
    Leon::FEntity m_PointLightEntity;
    Leon::FEntity m_SpotLightEntity;
    Leon::FEntity m_DirLightEntity;

    // Camera
    Leon::FPerspectiveCameraController m_CameraController;

    float m_TimeAccumulator = 0.0f;
};

class FSandboxApp : public Leon::FApplication {
public:
    explicit FSandboxApp(const Leon::FApplicationProps& InProps, const std::string& InStartupLevel)
        : Leon::FApplication(InProps) {
        PushLayer(new FLightingShowcaseLayer(InStartupLevel));
    }

    ~FSandboxApp() override = default;
};

Leon::FApplication* Leon::CreateApplication() {
    Leon::FOpenGLRenderDriver::Register();

    // 1. Load Project Configuration from INI file
    Leon::FConfigFile engineConfig("Projects/Sandbox/Config/DefaultEngine.ini");

    std::string windowTitle =
        engineConfig.GetString("/Script/Engine.DisplaySettings", "WindowTitle", "LeonEngine2 - Next-Gen Engine");
    int windowWidth = engineConfig.GetInt("/Script/Engine.DisplaySettings", "WindowWidth", 1280);
    int windowHeight = engineConfig.GetInt("/Script/Engine.DisplaySettings", "WindowHeight", 720);
    std::string startupLevel = engineConfig.GetString(
        "/Script/EngineSettings.GameMapsSettings", "GameDefaultMap", "Projects/Sandbox/Content/Maps/MainShowcase.llevel");

    Leon::FApplicationProps appProps;
    appProps.Name = windowTitle;
    appProps.WindowWidth = static_cast<unsigned int>(windowWidth);
    appProps.WindowHeight = static_cast<unsigned int>(windowHeight);

    return new FSandboxApp(appProps, startupLevel);
}
