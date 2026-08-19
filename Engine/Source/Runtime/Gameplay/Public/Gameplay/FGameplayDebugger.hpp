#pragma once

#include <cstdint>

namespace Leon {

    /**
     * Shift+F1 master overlay. Colliders + weapon traces by default; Shift+F5..F8 toggle AI/character/etc.
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
