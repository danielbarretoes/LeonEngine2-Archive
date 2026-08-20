#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    /** @brief Lightweight gameplay tag handle (name interned in FGameplayTagRegistry). */
    struct FGameplayTag {
        uint32_t Id = 0;

        bool IsValid() const { return Id != 0; }
        bool operator==(const FGameplayTag& InOther) const { return Id == InOther.Id; }
        bool operator!=(const FGameplayTag& InOther) const { return Id != InOther.Id; }
    };

    /** @brief Global tag name registry; RequestTag is idempotent per name. */
    class FGameplayTagRegistry {
    public:
        static FGameplayTag RequestTag(const std::string& InName);
        static const char* GetTagName(FGameplayTag InTag);
    };

    /** @brief Set of gameplay tags with HasTag / AddTag helpers. */
    struct FGameplayTagContainer {
        std::vector<FGameplayTag> Tags;

        bool IsEmpty() const { return Tags.empty(); }
        bool HasTag(FGameplayTag InTag) const;
        bool HasAny(const FGameplayTagContainer& InOther) const;
        bool HasAll(const FGameplayTagContainer& InOther) const;
        void AddTag(FGameplayTag InTag);
        void RemoveTag(FGameplayTag InTag);
        void Clear() { Tags.clear(); }
    };

} // namespace Leon
