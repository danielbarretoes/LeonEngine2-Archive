#pragma once

#include "Core/Base.hpp"
#include "Engine/UWorld.hpp"

namespace Leon::Editor {

    /**
     * @brief World Settings panel for configuring level-wide rules and defaults.
     */
    class FWorldSettingsPanel {
    public:
        FWorldSettingsPanel() = default;

        void Draw(UWorld* InWorld, bool* bInOutOpen = nullptr);

    private:
        char GameModeOverride[128] = "";
        bool bEnableStaticLighting = true;
        int LightmapResolution = 512;
        float Gravity = -9.81f;
    };

} // namespace Leon::Editor
