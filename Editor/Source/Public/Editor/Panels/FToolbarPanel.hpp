#pragma once

#include "Core/Base.hpp"
#include <functional>
#include <string>

namespace Leon::Editor {

    /**
     * @brief Editor Toolbar panel for top-level actions (Save Map, Bake, Project Hub, Run).
     */
    class FToolbarPanel {
    public:
        using FActionCallback = std::function<void()>;

        FToolbarPanel() = default;

        void SetOnSaveMap(FActionCallback InCb) { OnSaveMap = std::move(InCb); }
        void SetOnBakeDraft(FActionCallback InCb) { OnBakeDraft = std::move(InCb); }
        void SetOnBakeProduction(FActionCallback InCb) { OnBakeProduction = std::move(InCb); }
        void SetOnOpenHub(FActionCallback InCb) { OnOpenHub = std::move(InCb); }
        void SetOnRunGame(FActionCallback InCb) { OnRunGame = std::move(InCb); }

        void Draw(const std::string& InProjectName, const std::string& InMapName);

    private:
        FActionCallback OnSaveMap;
        FActionCallback OnBakeDraft;
        FActionCallback OnBakeProduction;
        FActionCallback OnOpenHub;
        FActionCallback OnRunGame;
    };

} // namespace Leon::Editor
