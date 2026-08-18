#pragma once

#include "Gameplay/UActorComponent.hpp"

#include <cstdint>
#include <vector>

namespace Leon {

    class AActor;

    /**
     * Generic actor-slot bag. Knows class/slot ids and pointers, not weapon rules or teams.
     * Does not spawn items; the owner grants and (optionally) destroys them.
     */
    class UInventoryComponent : public UActorComponent {
    public:
        UInventoryComponent(const std::string& InName = "InventoryComponent");

        void EndPlay() override;

        void SetSlotCount(uint32_t InCount);
        uint32_t GetSlotCount() const { return static_cast<uint32_t>(Slots.size()); }

        bool GiveItem(uint32_t InSlotId, AActor* InItem);
        bool RemoveItem(uint32_t InSlotId, bool bDestroyItem = false);
        void Clear(bool bDestroyItems = false);

        AActor* GetItem(uint32_t InSlotId) const;
        bool HasItem(uint32_t InSlotId) const;
        std::vector<AActor*> GetItems() const;

        AActor* GetActiveItem() const;
        uint32_t GetActiveSlot() const { return ActiveSlot; }
        bool SetActiveSlot(uint32_t InSlotId);

        /** Advance to the next occupied slot. Empty inventories keep the current slot. */
        uint32_t Cycle(int InDirection);

        void SetDestroyItemsOnEndPlay(bool bDestroy) { bDestroyItemsOnEndPlay = bDestroy; }
        bool DestroysItemsOnEndPlay() const { return bDestroyItemsOnEndPlay; }

    private:
        std::vector<AActor*> Slots;
        uint32_t ActiveSlot = 0;
        bool bDestroyItemsOnEndPlay = true;
    };

} // namespace Leon
