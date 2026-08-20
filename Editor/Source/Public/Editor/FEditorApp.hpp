#pragma once

#include "Core/FApplication.hpp"
#include "Editor/FViewportPanel.hpp"
#include "Engine/UWorld.hpp"

#include <string>

namespace Leon::Editor {

    /**
     * Dear ImGui editor host. Owns an empty UWorld for the viewport skeleton.
     * Does not run UEngine::Run (game boot); uses FApplication's window loop.
     */
    class FEditorApp : public FApplication {
    public:
        FEditorApp();

        void OnInit() override;
        void OnUpdate(FTimestep InTs) override;
        void OnShutdown() override;

    private:
        void BeginImGuiFrame();
        void EndImGuiFrame();
        void DrawDockspace();

        TRef<UWorld> EditorWorld;
        FViewportPanel Viewport;
        std::string ImGuiIniPath;
        bool bImGuiReady = false;
    };

} // namespace Leon::Editor
