#include "Gameplay/UInventoryComponent.hpp"
#include "Gameplay/AActor.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    UInventoryComponent::UInventoryComponent(const std::string& InName) : UActorComponent(InName) {
        Slots.assign(8, nullptr);
        SetComponentTickEnabled(false);
    }

    void UInventoryComponent::EndPlay() {
        Clear(bDestroyItemsOnEndPlay);
    }

    void UInventoryComponent::SetSlotCount(uint32_t InCount) {
        Slots.resize(InCount, nullptr);
        if (ActiveSlot >= GetSlotCount())
            ActiveSlot = 0;
    }

    bool UInventoryComponent::GiveItem(uint32_t InSlotId, AActor* InItem) {
        if (!InItem || InSlotId >= Slots.size() || Slots[InSlotId])
            return false;
        Slots[InSlotId] = InItem;
        return true;
    }

    bool UInventoryComponent::RemoveItem(uint32_t InSlotId, bool bDestroyItem) {
        if (InSlotId >= Slots.size())
            return false;
        AActor* item = Slots[InSlotId];
        Slots[InSlotId] = nullptr;
        if (bDestroyItem && item && item->GetWorld())
            item->GetWorld()->DestroyActor(item);
        return true;
    }

    void UInventoryComponent::Clear(bool bDestroyItems) {
        UWorld* world = Owner ? Owner->GetWorld() : nullptr;
        for (AActor*& item : Slots) {
            if (bDestroyItems && item && world)
                world->DestroyActor(item);
            item = nullptr;
        }
        ActiveSlot = 0;
    }

    AActor* UInventoryComponent::GetItem(uint32_t InSlotId) const {
        return InSlotId < Slots.size() ? Slots[InSlotId] : nullptr;
    }

    bool UInventoryComponent::HasItem(uint32_t InSlotId) const {
        return GetItem(InSlotId) != nullptr;
    }

    std::vector<AActor*> UInventoryComponent::GetItems() const {
        std::vector<AActor*> items;
        items.reserve(Slots.size());
        for (AActor* item : Slots) {
            if (item)
                items.push_back(item);
        }
        return items;
    }

    AActor* UInventoryComponent::GetActiveItem() const {
        return GetItem(ActiveSlot);
    }

    bool UInventoryComponent::SetActiveSlot(uint32_t InSlotId) {
        if (InSlotId >= Slots.size() || !Slots[InSlotId])
            return false;
        ActiveSlot = InSlotId;
        return true;
    }

    uint32_t UInventoryComponent::Cycle(int InDirection) {
        if (InDirection == 0 || Slots.empty())
            return ActiveSlot;
        const int count = static_cast<int>(Slots.size());
        int cur = static_cast<int>(ActiveSlot);
        for (int step = 0; step < count; ++step) {
            cur = (cur + (InDirection > 0 ? 1 : -1) + count) % count;
            if (Slots[static_cast<size_t>(cur)]) {
                ActiveSlot = static_cast<uint32_t>(cur);
                return ActiveSlot;
            }
        }
        return ActiveSlot;
    }

} // namespace Leon
