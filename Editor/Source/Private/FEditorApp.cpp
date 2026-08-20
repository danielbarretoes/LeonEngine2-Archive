#include "Editor/FEditorApp.hpp"

#include "Core/FLog.hpp"
#include "Core/FWindow.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <filesystem>

namespace Leon::Editor {

    FEditorApp::FEditorApp()
        : FApplication([] {
              FApplicationProps Props;
              Props.Name = "Leon Editor";
              Props.WindowWidth = 1600;
              Props.WindowHeight = 900;
              return Props;
          }()) {}

    void FEditorApp::OnInit() {
        GLFWwindow* Native = GetWindow().GetNativeWindow();
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& IO = ImGui::GetIO();
        IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        const std::filesystem::path IniDir = std::filesystem::path("Editor") / "Saved";
        std::error_code Ec;
        std::filesystem::create_directories(IniDir, Ec);
        ImGuiIniPath = (IniDir / "imgui.ini").string();
        IO.IniFilename = ImGuiIniPath.c_str();

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(Native, true);
        ImGui_ImplOpenGL3_Init("#version 450");
        bImGuiReady = true;

        EditorWorld = UWorld::Create("EditorWorld");
        EditorWorld->InitWorld();

        LE_CORE_INFO("FEditorApp: ImGui docking host ready (skeleton)");
    }

    void FEditorApp::OnUpdate(FTimestep InTs) {
        (void)InTs;
        if (!bImGuiReady) {
            return;
        }

        BeginImGuiFrame();
        DrawDockspace();
        Viewport.Draw(EditorWorld.get());
        EndImGuiFrame();
    }

    void FEditorApp::OnShutdown() {
        if (EditorWorld) {
            EditorWorld->EndPlay();
            EditorWorld->Clear();
            EditorWorld.reset();
        }

        if (bImGuiReady) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            bImGuiReady = false;
        }
    }

    void FEditorApp::BeginImGuiFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void FEditorApp::EndImGuiFrame() {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void FEditorApp::DrawDockspace() {
        const ImGuiViewport* ViewportInfo = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ViewportInfo->WorkPos);
        ImGui::SetNextWindowSize(ViewportInfo->WorkSize);
        ImGui::SetNextWindowViewport(ViewportInfo->ID);

        ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                       ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("LeonEditorDockspace", nullptr, WindowFlags);
        ImGui::PopStyleVar(3);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) {
                    Close();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        const ImGuiID DockspaceId = ImGui::GetID("LeonEditorDockspaceId");
        ImGui::DockSpace(DockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();
    }

} // namespace Leon::Editor
