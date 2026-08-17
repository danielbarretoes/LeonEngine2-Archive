#pragma once

#include <cstdint>

namespace Leon {

    /**
     * Shift+F1 master overlay. Categories can be toggled independently (Shift+F5..F8).
     */
    class FGameplayDebugger {
    public:
        static bool IsEnabled();
        static void SetEnabled(bool bEnabled);

        static bool ShowPhysics();
        static bool ShowCharacter();
        static bool ShowAI();
        static bool ShowNetwork();
        static void CycleSelectedAI(int32_t InDelta);
        static int32_t GetSelectedAIIndex();

        static void TogglePhysics();
        static void ToggleCharacter();
        static void ToggleAI();
        static void ToggleNetwork();

        static void ResetDefaults();
    };

} // namespace Leon
