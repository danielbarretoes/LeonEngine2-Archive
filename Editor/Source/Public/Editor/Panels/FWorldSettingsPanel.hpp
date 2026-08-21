#pragma once

#include "Core/Base.hpp"
#include "Engine/UWorld.hpp"
#include <string>

namespace Leon::Editor {

    /**
     * @brief World Settings panel for configuring level-wide rules and defaults.
     */
    class FWorldSettingsPanel {
    public:
        FWorldSettingsPanel() = default;

        void Draw(UWorld* InWorld, const std::string& InProjectDefaultGameMode = {}, bool* bInOutOpen = nullptr);
    };

} // namespace Leon::Editor
