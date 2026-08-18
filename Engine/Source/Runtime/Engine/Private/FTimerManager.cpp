#include "Engine/FTimerManager.hpp"

#include <algorithm>

namespace Leon {

    uint32_t FTimerManager::AllocateSlot() {
        for (uint32_t i = 0; i < static_cast<uint32_t>(Entries.size()); ++i) {
            if (!Entries[i].bActive)
                return i + 1;
        }
        Entries.push_back({});
        return static_cast<uint32_t>(Entries.size());
    }

    bool FTimerManager::IsHandleLive(const FTimerHandle& InHandle) const {
        if (!InHandle.IsValid() || InHandle.Index > Entries.size())
            return false;
        const FTimerEntry& entry = Entries[InHandle.Index - 1];
        return entry.bActive && entry.Generation == InHandle.Generation;
    }

    void FTimerManager::SetTimer(FTimerHandle& InOutHandle, FTimerDelegate InDelegate, float InRate, bool bInLoop) {
        ClearTimer(InOutHandle);
        if (!InDelegate)
            return;
        const uint32_t slot = AllocateSlot();
        FTimerEntry& entry = Entries[slot - 1];
        entry.Delegate = std::move(InDelegate);
        entry.Rate = std::max(InRate, 0.0f);
        entry.Remaining = entry.Rate;
        entry.bLoop = bInLoop;
        entry.bActive = true;
        ++entry.Generation;
        if (entry.Generation == 0)
            entry.Generation = 1;
        InOutHandle.Index = slot;
        InOutHandle.Generation = entry.Generation;
    }

    void FTimerManager::ClearTimer(FTimerHandle& InOutHandle) {
        if (IsHandleLive(InOutHandle)) {
            FTimerEntry& entry = Entries[InOutHandle.Index - 1];
            entry.bActive = false;
            entry.Delegate = {};
        }
        InOutHandle.Invalidate();
    }

    bool FTimerManager::IsTimerActive(const FTimerHandle& InHandle) const { return IsHandleLive(InHandle); }

    float FTimerManager::GetTimerRemaining(const FTimerHandle& InHandle) const {
        if (!IsHandleLive(InHandle))
            return -1.0f;
        return Entries[InHandle.Index - 1].Remaining;
    }

    void FTimerManager::Clear() {
        for (FTimerEntry& entry : Entries) {
            entry.bActive = false;
            entry.Delegate = {};
        }
    }

    void FTimerManager::Tick(float InDeltaSeconds) {
        std::vector<FTimerDelegate> fires;
        fires.reserve(4);
        for (FTimerEntry& entry : Entries) {
            if (!entry.bActive)
                continue;
            entry.Remaining -= InDeltaSeconds;
            if (entry.Remaining > 0.0f)
                continue;
            fires.push_back(entry.Delegate);
            if (entry.bLoop)
                entry.Remaining += std::max(entry.Rate, InDeltaSeconds);
            else {
                entry.bActive = false;
                entry.Delegate = {};
            }
        }
        for (FTimerDelegate& fn : fires) {
            if (fn)
                fn();
        }
    }

} // namespace Leon
