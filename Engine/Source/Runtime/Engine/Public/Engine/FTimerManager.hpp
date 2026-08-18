#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace Leon {

    using FTimerDelegate = std::function<void()>;

    /**
     * Opaque handle for a world timer. Index 0 is invalid.
     */
    struct FTimerHandle {
        uint32_t Index = 0;
        uint32_t Generation = 0;

        bool IsValid() const { return Index != 0; }
        void Invalidate() {
            Index = 0;
            Generation = 0;
        }

        bool operator==(const FTimerHandle& InOther) const {
            return Index == InOther.Index && Generation == InOther.Generation;
        }
    };

    /**
     * World-owned countdown list (Unreal FTimerManager lite).
     * Delegates fire during UWorld::Tick after incoming net consume.
     */
    class FTimerManager {
    public:
        void Tick(float InDeltaSeconds);
        void Clear();

        void SetTimer(FTimerHandle& InOutHandle, FTimerDelegate InDelegate, float InRate, bool bInLoop = false);
        void ClearTimer(FTimerHandle& InOutHandle);
        bool IsTimerActive(const FTimerHandle& InHandle) const;
        float GetTimerRemaining(const FTimerHandle& InHandle) const;

    private:
        struct FTimerEntry {
            FTimerDelegate Delegate;
            float Rate = 0.0f;
            float Remaining = 0.0f;
            uint32_t Generation = 0;
            bool bLoop = false;
            bool bActive = false;
        };

        std::vector<FTimerEntry> Entries;
        uint32_t AllocateSlot();
        bool IsHandleLive(const FTimerHandle& InHandle) const;
    };

} // namespace Leon
