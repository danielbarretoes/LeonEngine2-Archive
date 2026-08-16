#include "Gameplay/FOnScreenDebugMessage.hpp"
#include "UMG/FUIRenderer.hpp"

#include <algorithm>

namespace Leon {

    FOnScreenDebugMessageManager& FOnScreenDebugMessageManager::Get() {
        static FOnScreenDebugMessageManager instance;
        return instance;
    }

    void FOnScreenDebugMessageManager::AddMessage(const std::string& InText, float InDuration, const glm::vec4& InColor,
                                                  int64_t InKey, const glm::vec2& InPosition) {
        if (InKey >= 0) {
            auto it = std::find_if(Messages.begin(), Messages.end(),
                                   [InKey](const FOnScreenDebugMessage& msg) { return msg.Key == InKey; });
            if (it != Messages.end()) {
                it->Text = InText;
                it->TimeRemaining = InDuration;
                it->Color = InColor;
                it->Position = InPosition;
                return;
            }
        }

        FOnScreenDebugMessage msg;
        msg.Text = InText;
        msg.TimeRemaining = InDuration;
        msg.Color = InColor;
        msg.Position = InPosition;
        msg.Key = InKey;
        Messages.push_back(std::move(msg));
    }

    void FOnScreenDebugMessageManager::Tick(float InDeltaSeconds) {
        for (auto& msg : Messages) {
            msg.TimeRemaining -= InDeltaSeconds;
        }
        Messages.erase(std::remove_if(Messages.begin(), Messages.end(),
                                        [](const FOnScreenDebugMessage& msg) { return msg.TimeRemaining <= 0.0f; }),
                         Messages.end());
    }

    void FOnScreenDebugMessageManager::Draw(float InViewportWidth, float InViewportHeight) {
        (void)InViewportWidth;
        constexpr float StartX = 16.0f;
        constexpr float StartY = 16.0f;
        constexpr float LineStep = 22.0f;

        float autoY = StartY;
        for (const auto& msg : Messages) {
            float x = msg.Position.x >= 0.0f ? msg.Position.x : StartX;
            float y = msg.Position.y >= 0.0f ? msg.Position.y : autoY;

            // Fade out in the last 0.5s
            glm::vec4 color = msg.Color;
            if (msg.TimeRemaining < 0.5f) {
                color.a *= std::max(0.0f, msg.TimeRemaining / 0.5f);
            }

            FUIRenderer::DrawString(x, y, msg.Text, color, 1.0f);

            if (msg.Position.y < 0.0f) {
                autoY += LineStep;
            }
            if (autoY > InViewportHeight - 20.0f) {
                break;
            }
        }
    }

    void FOnScreenDebugMessageManager::Clear() {
        Messages.clear();
    }

} // namespace Leon
