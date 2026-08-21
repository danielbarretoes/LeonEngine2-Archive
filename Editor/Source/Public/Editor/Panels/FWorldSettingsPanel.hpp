#pragma once

#include "Core/Base.hpp"
#include "Editor/Context/FEditorContext.hpp"
#include "Engine/UWorld.hpp"
#include <string>

namespace Leon::Editor {

    /**
     * @brief World Settings panel for configuring level-wide rules and defaults.
     */
    class FWorldSettingsPanel {
    public:
        FWorldSettingsPanel() = default;

        void SetEditorContext(FEditorContext* InContext) { Context = InContext; }
        void Draw(UWorld* InWorld, const std::string& InProjectDefaultGameMode = {}, bool* bInOutOpen = nullptr);

    private:
        FEditorContext* Context = nullptr;
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::FWorldSettingsPanel;
}
