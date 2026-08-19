#pragma once

#include <cstdint>

namespace Leon {

    /**
     * Compact match phase on AGameState. Game-specific lobby/menu states stay on the game GameState.
     */
    enum class EMatchState : uint8_t {
        WaitingToStart = 0,
        InProgress = 1,
        WaitingPostMatch = 2,
    };

} // namespace Leon
