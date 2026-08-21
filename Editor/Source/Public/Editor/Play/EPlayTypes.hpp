#pragma once

#include <cstdint>

namespace Leon::Editor {

    enum class EPlayNetMode : uint8_t {
        Standalone = 0,
        ListenServer = 1,
        Client = 2,
        DedicatedServer = 3
    };

    enum class EPlayMode : uint8_t {
        SelectedViewport = 0,
        NewEditorWindow = 1
    };

} // namespace Leon::Editor
