#pragma once

#include "Core/Base.hpp"
#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class UWorld;
    class APlayerController;
    class AGameModeBase;
    class AGameStateBase;
    class APawn;

    /**
     * @brief Unreal Engine aligned static library for gameplay functions.
     */
    class UGameplayStatics {
    public:
        /**
         * @brief Requests loading of a new level map (e.g. "/Game/Maps/NightScene").
         */
        static void OpenLevel(UWorld* InWorldContext, const std::string& InLevelName);

        /**
         * @brief Displays an on-screen debug message in the viewport (not FLog).
         * @param InKey When >= 0, replaces any existing message with the same key.
         * @param InPosition Screen-space pixels; (-1,-1) auto-stacks from the top-left.
         */
        static void PrintString(const std::string& InMessage, float InDuration = 2.0f,
                                const glm::vec4& InColor = glm::vec4(0.0f, 0.85f, 1.0f, 1.0f), int64_t InKey = -1,
                                const glm::vec2& InPosition = glm::vec2(-1.0f, -1.0f));

        static APlayerController* GetPlayerController(UWorld* InWorldContext, int32_t InPlayerIndex = 0);
        static AGameModeBase* GetGameMode(UWorld* InWorldContext);
        static AGameStateBase* GetGameState(UWorld* InWorldContext);
        static APawn* GetPlayerPawn(UWorld* InWorldContext, int32_t InPlayerIndex = 0);
    };

    /**
     * @brief Global helper for visual on-screen debug printing (Unreal PrintString equivalent).
     */
    inline void PrintString(const std::string& InMessage, float InDuration = 2.0f,
                            const glm::vec4& InColor = glm::vec4(0.0f, 0.85f, 1.0f, 1.0f), int64_t InKey = -1,
                            const glm::vec2& InPosition = glm::vec2(-1.0f, -1.0f)) {
        UGameplayStatics::PrintString(InMessage, InDuration, InColor, InKey, InPosition);
    }

} // namespace Leon
