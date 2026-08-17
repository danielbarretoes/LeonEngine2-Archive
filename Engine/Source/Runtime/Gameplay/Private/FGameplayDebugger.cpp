#include "Gameplay/FGameplayDebugger.hpp"
#include "Core/FApplication.hpp"

#include <algorithm>

namespace Leon {

    namespace {
        int32_t GSelectedAIIndex = 0;
    }

    bool FGameplayDebugger::IsEnabled() {
        return FApplication::HasInstance() && FApplication::Get().IsGameplayDebugEnabled();
    }

    void FGameplayDebugger::SetEnabled(bool bEnabled) {
        if (FApplication::HasInstance())
            FApplication::Get().SetGameplayDebugEnabled(bEnabled);
    }

    bool FGameplayDebugger::ShowPhysics() {
        return IsEnabled() && FApplication::HasInstance() && FApplication::Get().IsDebugPhysicsEnabled();
    }
    bool FGameplayDebugger::ShowCharacter() {
        return IsEnabled() && FApplication::HasInstance() && FApplication::Get().IsDebugCharacterEnabled();
    }
    bool FGameplayDebugger::ShowAI() {
        return FApplication::HasInstance() && FApplication::Get().IsDebugAIEnabled();
    }
    bool FGameplayDebugger::ShowNetwork() {
        return IsEnabled() && FApplication::HasInstance() && FApplication::Get().IsDebugNetworkEnabled();
    }

    void FGameplayDebugger::CycleSelectedAI(int32_t InDelta) { GSelectedAIIndex += InDelta; }

    int32_t FGameplayDebugger::GetSelectedAIIndex() { return GSelectedAIIndex; }

    void FGameplayDebugger::TogglePhysics() {
        if (FApplication::HasInstance())
            FApplication::Get().ToggleDebugPhysics();
    }
    void FGameplayDebugger::ToggleCharacter() {
        if (FApplication::HasInstance())
            FApplication::Get().ToggleDebugCharacter();
    }
    void FGameplayDebugger::ToggleAI() {
        if (FApplication::HasInstance())
            FApplication::Get().ToggleDebugAI();
    }
    void FGameplayDebugger::ToggleNetwork() {
        if (FApplication::HasInstance())
            FApplication::Get().ToggleDebugNetwork();
    }

    void FGameplayDebugger::ResetDefaults() {}

} // namespace Leon
