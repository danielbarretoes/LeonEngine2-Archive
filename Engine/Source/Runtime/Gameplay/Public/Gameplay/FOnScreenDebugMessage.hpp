#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon {

    /**
     * @brief Single on-screen debug message entry (PrintString / GEngine-style).
     * Distinct from FLog — these are rendered inside the game viewport.
     */
    struct FOnScreenDebugMessage {
        std::string Text;
        float TimeRemaining = 2.0f;
        glm::vec4 Color{0.0f, 0.85f, 1.0f, 1.0f};
        glm::vec2 Position{-1.0f, -1.0f}; // (-1,-1) = auto-stacked
        int64_t Key = -1;
    };

    /**
     * @brief Engine-owned queue of viewport debug messages.
     * Updated each frame and painted by AHUD via FUIRenderer.
     */
    class FOnScreenDebugMessageManager {
    public:
        static FOnScreenDebugMessageManager& Get();

        void AddMessage(const std::string& InText, float InDuration = 2.0f,
                        const glm::vec4& InColor = glm::vec4(0.0f, 0.85f, 1.0f, 1.0f), int64_t InKey = -1,
                        const glm::vec2& InPosition = glm::vec2(-1.0f, -1.0f));

        void Tick(float InDeltaSeconds);
        void Draw(float InViewportWidth, float InViewportHeight);
        void Clear();

        size_t GetMessageCount() const { return Messages.size(); }
        const std::vector<FOnScreenDebugMessage>& GetMessages() const { return Messages; }

    private:
        std::vector<FOnScreenDebugMessage> Messages;
    };

} // namespace Leon
