#include "LeonEngine.hpp"
#include "OpenGLRenderDriver.hpp"
#include "renderer/AssetManager.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <filesystem>
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

        Leon::FAssetManager::SetContentRoot("Projects/Sandbox/Content");

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
        LE_INFO("  - F10: Cycle Material Forensic Debug Views (BaseColor -> Metallic -> Roughness -> Normal -> AO -> "
                "Emissive -> T -> B -> UV -> N.L)");
        LE_INFO("  - F11: Cycle Shadow Forensic Debug Views (Composite -> Shadow Factor -> Cascade False-Color -> "
                "Contact Shadows -> Depth 0..3)");
        LE_INFO("  - F12: Cycle Shadow Filter Modes (Hard -> PCF 3x3 -> PCF 5x5 -> Poisson Disk 16-Tap)");
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
        Leon::FLevelSerializer serializer(m_Scene);
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
            else if (tag.Tag == "PBR Mirror Chrome Sphere")
                m_ChromeSphereEntity = entity;
            else if (tag.Tag == "Orbiting Point Light")
                m_PointLightEntity = entity;
            else if (tag.Tag == "Accent Magenta Point Light")
                m_PointLight2Entity = entity;
            else if (tag.Tag == "Dramatic Spotlight")
                m_SpotLightEntity = entity;
            else if (tag.Tag == "Golden Raking Spotlight")
                m_SpotLight2Entity = entity;
            else if (tag.Tag == "Directional Sunlight")
                m_DirLightEntity = entity;
        }

        // Set initial camera position looking down at the stage or from level's primary camera
        bool bFoundPrimaryCamera = false;
        auto camView = m_Scene->GetRegistry().view<Leon::FCameraComponent, Leon::FTransformComponent>();
        for (auto entityHandle : camView) {
            const auto& [camComp, transform] =
                camView.get<Leon::FCameraComponent, Leon::FTransformComponent>(entityHandle);
            if (camComp.bPrimary) {
                m_CameraController.GetCamera().SetPosition(transform.Translation);
                m_CameraController.GetCamera().SetRotation(transform.Rotation.x, transform.Rotation.y);
                m_CameraController.GetCamera().SetFOV(camComp.Camera.GetFOV());
                bFoundPrimaryCamera = true;
                break;
            }
        }
        if (!bFoundPrimaryCamera) {
            m_CameraController.GetCamera().SetPosition({0.0f, 4.5f, 9.5f});
            m_CameraController.GetCamera().SetRotation(-18.0f, -90.0f);
        }

        size_t totalActors = m_Scene->GetRegistry().view<Leon::FTagComponent>().size();
        size_t meshActors = m_Scene->GetRegistry().view<Leon::FMeshComponent>().size() +
                            m_Scene->GetRegistry().view<Leon::FStaticMeshComponent>().size();
        size_t dirLights = m_Scene->GetRegistry().view<Leon::FDirectionalLightComponent>().size();
        size_t pointLights = m_Scene->GetRegistry().view<Leon::FPointLightComponent>().size();
        size_t spotLights = m_Scene->GetRegistry().view<Leon::FSpotLightComponent>().size();
        size_t cameras = m_Scene->GetRegistry().view<Leon::FCameraComponent>().size();

        std::filesystem::path lp(m_LevelPath);
        std::cout << "\n[Level] Loading: " << lp.filename().string() << "\n";
        std::cout << "[Level] Actors: " << totalActors << "\n";
        std::cout << "[Level] Mesh Actors: " << meshActors << "\n";
        std::cout << "[Level] Directional Lights: " << dirLights << "\n";
        std::cout << "[Level] Point Lights: " << pointLights << "\n";
        std::cout << "[Level] Spot Lights: " << spotLights << "\n";
        std::cout << "[Level] Cameras: " << cameras << "\n\n";

        std::cout << "[Renderer] Initializing...\n";
        std::cout << "[Renderer] Loading assets...\n";
        std::cout << "[Renderer] Level ready.\n\n";

        std::cout << "[Sandbox] Renderer Showcase loaded successfully.\n";
        std::cout << "[Sandbox] READY\n" << std::flush;

        auto endTime = std::chrono::high_resolution_clock::now();
        float levelLoadDurationMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        float totalFromWindowOpenMs = Leon::FApplication::Get().GetTimeSinceWindowOpenMs();

        LE_INFO("Level loaded complete. Loaded '{0}' in {1:.2f} ms ({2:.2f} ms since window opened).", m_LevelPath,
                levelLoadDurationMs, totalFromWindowOpenMs);
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
        if (m_ChromeSphereEntity) {
            auto& transform = m_ChromeSphereEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation.y = 0.7f + std::sin(m_TimeAccumulator * 2.5f + 1.0f) * 0.12f;
            transform.Rotation.y += 20.0f * InTs.GetSeconds();
        }

        // 2. Orbit Point Light 1 (Amber, clockwise)
        if (m_PointLightEntity) {
            float orbitRadius = 5.2f;
            glm::vec3 newPos = glm::vec3(std::cos(m_TimeAccumulator * 1.2f) * orbitRadius, 1.8f,
                                         std::sin(m_TimeAccumulator * 1.2f) * orbitRadius);

            auto& transform = m_PointLightEntity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = newPos;

            if (m_PointLightEntity.HasComponent<Leon::FPointLightComponent>()) {
                auto& pointLightComp = m_PointLightEntity.GetComponent<Leon::FPointLightComponent>();
                pointLightComp.Light.Position = newPos;
            }
        }

        // 3. Orbit Point Light 2 (Magenta, counter-clockwise)
        if (m_PointLight2Entity) {
            float orbitRadius = 4.8f;
            glm::vec3 newPos = glm::vec3(std::cos(-m_TimeAccumulator * 0.9f + 1.57f) * orbitRadius, 2.0f,
                                         std::sin(-m_TimeAccumulator * 0.9f + 1.57f) * orbitRadius);

            auto& transform = m_PointLight2Entity.GetComponent<Leon::FTransformComponent>();
            transform.Translation = newPos;

            if (m_PointLight2Entity.HasComponent<Leon::FPointLightComponent>()) {
                auto& pointLightComp = m_PointLight2Entity.GetComponent<Leon::FPointLightComponent>();
                pointLightComp.Light.Position = newPos;
            }
        }

        // ==========================================
        // Render Scene Pipeline (CSM + Spot + Reflection + PBR + Skybox + Post-Process)
        // ==========================================
        m_Scene->OnRender(m_CameraController.GetCamera());

        // Render 3D Light Debug Gizmos (F2 Toggle)
        // Render 3D Light Debug Gizmos (F2 Toggle)
        if (Leon::FApplication::Get().IsLightGizmosEnabled() && m_Scene) {
            Leon::FDebugRenderer::BeginScene(m_CameraController.GetCamera());

            auto& reg = m_Scene->GetRegistry();

            // 1. Directional Light Vector & Arrow
            auto dirView = reg.view<Leon::FDirectionalLightComponent, Leon::FTransformComponent>();
            for (auto entityHandle : dirView) {
                const auto& [dirComp, transform] =
                    dirView.get<Leon::FDirectionalLightComponent, Leon::FTransformComponent>(entityHandle);
                if (dirComp.bEnabled) {
                    Leon::FDebugRenderer::DrawDirectionalLightGizmo(dirComp.Light, transform.Translation, 2.5f);
                }
            }

            // 2. Point Light Origin & Attenuation Radii
            auto pointView = reg.view<Leon::FPointLightComponent, Leon::FTransformComponent>();
            for (auto entityHandle : pointView) {
                const auto& [pointComp, transform] =
                    pointView.get<Leon::FPointLightComponent, Leon::FTransformComponent>(entityHandle);
                if (pointComp.bEnabled) {
                    Leon::FDebugRenderer::DrawPointLightGizmo(pointComp.Light);
                }
            }

            // 3. Spot Light Origin, Axis & Cone Wireframe
            auto spotView = reg.view<Leon::FSpotLightComponent, Leon::FTransformComponent>();
            for (auto entityHandle : spotView) {
                const auto& [spotComp, transform] =
                    spotView.get<Leon::FSpotLightComponent, Leon::FTransformComponent>(entityHandle);
                if (spotComp.bEnabled) {
                    Leon::FDebugRenderer::DrawSpotLightGizmo(spotComp.Light);
                }
            }

            Leon::FDebugRenderer::EndScene();
        }
    }

    void OnEvent(Leon::FEvent& InEvent) override {
        m_CameraController.OnEvent(InEvent);

        Leon::FEventDispatcher dispatcher(InEvent);
        dispatcher.Dispatch<Leon::FKeyPressedEvent>([this](Leon::FKeyPressedEvent& e) {
            auto key = e.GetKeyCode();
            auto renderer = m_Scene ? m_Scene->GetSceneRenderer() : nullptr;

            // Shift + Function Keys: Diagnostic Views
            if (Leon::FInput::IsKeyPressed(Leon::Key::LeftShift) || Leon::FInput::IsKeyPressed(Leon::Key::RightShift)) {
                if (!renderer)
                    return false;

                if (key == Leon::Key::F1) {
                    renderer->SetDebugMode(0);
                    LE_INFO("[DEBUG VIEW] Full Composite PBR Lit");
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
            }

            if (renderer) {
                auto& postSettings = renderer->GetPostProcessSettings();

                if (key == Leon::Key::F3) {
                    postSettings.bEnabled = !postSettings.bEnabled;
                    LE_INFO("[POST-PROCESS] Pipeline {0}",
                            postSettings.bEnabled ? "ENABLED" : "DISABLED (Raw Linear Pass-through)");
                    return true;
                } else if (key == Leon::Key::F4) {
                    postSettings.bFXAAEnabled = !postSettings.bFXAAEnabled;
                    LE_INFO("[POST-PROCESS] FXAA Anti-Aliasing {0}",
                            postSettings.bFXAAEnabled ? "ENABLED" : "DISABLED");
                    return true;
                } else if (key == Leon::Key::F5) {
                    postSettings.DebugMode = (postSettings.DebugMode + 1) % 5;
                    static const char* s_ModeNames[] = {"Full Post-Processing", "Raw HDR (Linear Pre-ToneMap)",
                                                        "Bloom Composite Only", "Bright-Pass Isolation Only",
                                                        "Tone Mapping Only (No Bloom)"};
                    LE_INFO("[POST-PROCESS DEBUG] Mode: {0}", s_ModeNames[postSettings.DebugMode]);
                    return true;
                } else if (key == Leon::Key::F6) {
                    postSettings.Exposure = std::max(0.1f, postSettings.Exposure - 0.1f);
                    LE_INFO("[POST-PROCESS] Exposure: {0:.2f}", postSettings.Exposure);
                    return true;
                } else if (key == Leon::Key::F7) {
                    postSettings.Exposure = std::min(10.0f, postSettings.Exposure + 0.1f);
                    LE_INFO("[POST-PROCESS] Exposure: {0:.2f}", postSettings.Exposure);
                    return true;
                } else if (key == Leon::Key::F8) {
                    postSettings.BloomIntensity = std::max(0.0f, postSettings.BloomIntensity - 0.01f);
                    LE_INFO("[POST-PROCESS] Bloom Intensity: {0:.3f}", postSettings.BloomIntensity);
                    return true;
                } else if (key == Leon::Key::F9) {
                    postSettings.BloomIntensity = std::min(1.0f, postSettings.BloomIntensity + 0.01f);
                    LE_INFO("[POST-PROCESS] Bloom Intensity: {0:.3f}", postSettings.BloomIntensity);
                    return true;
                } else if (key == Leon::Key::F10) {
                    static int s_MatDebugIndex = 0;
                    static int s_MatModes[] = {0, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
                    static const char* s_MatNames[] = {"Full Lit (Composite)",
                                                       "Albedo / Base Color",
                                                       "Metallic",
                                                       "Roughness",
                                                       "World Normals (N)",
                                                       "Ambient Occlusion",
                                                       "Emissive",
                                                       "World Tangent (T)",
                                                       "World Bitangent (B)",
                                                       "Texture Coordinates (UV0)",
                                                       "Direct Lighting Cosine Term (N.L)"};
                    s_MatDebugIndex = (s_MatDebugIndex + 1) % 11;
                    renderer->SetDebugMode(s_MatModes[s_MatDebugIndex]);
                    LE_INFO("[MATERIAL DEBUG VIEW] Mode {0}: {1}", s_MatModes[s_MatDebugIndex],
                            s_MatNames[s_MatDebugIndex]);
                    return true;
                } else if (key == Leon::Key::F11) {
                    static int s_ShadowDebugIndex = 0;
                    static int s_ShadowModes[] = {0, 24, 25, 26, 27, 28, 29, 30};
                    static const char* s_ShadowNames[] = {
                        "Full Lit (Default Composite)",
                        "Directional Shadow Factor (White=Lit, Black=Shadow)",
                        "CSM Cascade False-Color Visualization (Red=C0, Green=C1, Blue=C2, Yellow=C3)",
                        "Screen-Space Contact Shadow Factor",
                        "Cascade 0 Depth Map Slice",
                        "Cascade 1 Depth Map Slice",
                        "Cascade 2 Depth Map Slice",
                        "Cascade 3 Depth Map Slice"};
                    s_ShadowDebugIndex = (s_ShadowDebugIndex + 1) % 8;
                    renderer->SetDebugMode(s_ShadowModes[s_ShadowDebugIndex]);
                    LE_INFO("[SHADOW DEBUG VIEW] Mode {0}: {1}", s_ShadowModes[s_ShadowDebugIndex],
                            s_ShadowNames[s_ShadowDebugIndex]);
                    return true;
                } else if (key == Leon::Key::F12) {
                    auto& shadowSettings = renderer->GetShadowSettings();
                    int nextMode = (static_cast<int>(shadowSettings.FilterMode) + 1) % 4;
                    shadowSettings.FilterMode = static_cast<Leon::EShadowFilterMode>(nextMode);
                    static const char* s_FilterNames[] = {
                        "Hard Shadow (1 Tap)", "PCF 3x3 (9 Taps Kernel)", "PCF 5x5 (25 Taps Kernel)",
                        "Poisson Disk (16 Taps Vogel Spiral with Interleaved Noise Jitter)"};
                    LE_INFO("[SHADOW FILTER MODE] Switched to: {0}", s_FilterNames[nextMode]);
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
    Leon::FEntity m_ChromeSphereEntity;
    Leon::FEntity m_PointLightEntity;
    Leon::FEntity m_PointLight2Entity;
    Leon::FEntity m_SpotLightEntity;
    Leon::FEntity m_SpotLight2Entity;
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

Leon::FApplication* Leon::CreateApplication(Leon::FApplicationCommandLineArgs InArgs) {
    Leon::FOpenGLRenderDriver::Register();

    std::string levelPath = "Projects/Sandbox/Content/Maps/MainShowcase.llevel";

    // Optional command-line level override
    for (int i = 1; i < InArgs.Count; ++i) {
        if (!InArgs[i])
            continue;
        std::string arg = InArgs[i];
        if (arg == "--level" && i + 1 < InArgs.Count && InArgs[i + 1]) {
            levelPath = InArgs[++i];
        } else if (arg.rfind("-", 0) != 0) {
            levelPath = arg;
        }
    }

    Leon::FApplicationProps appProps;
    appProps.Name = "LeonEngine2 - Renderer Showcase";
    appProps.WindowWidth = 1280;
    appProps.WindowHeight = 720;
    appProps.CommandLineArgs = InArgs;

    return new FSandboxApp(appProps, levelPath);
}
