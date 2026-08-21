#pragma once

#include "Core/Base.hpp"
#include "Editor/Play/FPlaySettings.hpp"
#include <functional>
#include <string>

namespace Leon::Editor {

    /**
     * @brief Editor Toolbar panel for top-level actions (Save, Play/Stop, Bake).
     */
    class FToolbarPanel {
    public:
        using FActionCallback = std::function<void()>;

        FToolbarPanel() = default;

        void SetOnSaveMap(FActionCallback InCb) { OnSaveMap = std::move(InCb); }
        void SetOnBakeDraft(FActionCallback InCb) { OnBakeDraft = std::move(InCb); }
        void SetOnBakeProduction(FActionCallback InCb) { OnBakeProduction = std::move(InCb); }
        void SetOnPlay(FActionCallback InCb) { OnPlay = std::move(InCb); }
        void SetOnStop(FActionCallback InCb) { OnStop = std::move(InCb); }
        void SetPlaySettings(FPlaySettings* InSettings) { PlaySettings = InSettings; }
        void SetPlaying(bool bInPlaying) { bPlaying = bInPlaying; }

        /** @deprecated Use SetOnPlay — kept for transitional wiring. */
        void SetOnRunGame(FActionCallback InCb) { OnPlay = std::move(InCb); }

        void Draw(const std::string& InProjectName, const std::string& InMapName,
                  const std::string& InStatusMessage = "");

    private:
        void DrawPlaySettingsPopup();

        FActionCallback OnSaveMap;
        FActionCallback OnBakeDraft;
        FActionCallback OnBakeProduction;
        FActionCallback OnPlay;
        FActionCallback OnStop;
        FPlaySettings* PlaySettings = nullptr;
        bool bPlaying = false;
    };

} // namespace Leon::Editor
