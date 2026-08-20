#include "Gameplay/FGameplayTag.hpp"

#include <algorithm>
#include <unordered_map>

namespace Leon {

    namespace {
        std::unordered_map<std::string, uint32_t>& TagNameToId() {
            static std::unordered_map<std::string, uint32_t> map;
            return map;
        }

        std::vector<std::string>& TagIdToName() {
            static std::vector<std::string> names;
            return names;
        }
    } // namespace

    FGameplayTag FGameplayTagRegistry::RequestTag(const std::string& InName) {
        if (InName.empty())
            return {};

        auto& nameToId = TagNameToId();
        auto it = nameToId.find(InName);
        if (it != nameToId.end())
            return FGameplayTag{it->second};

        auto& idToName = TagIdToName();
        const uint32_t id = static_cast<uint32_t>(idToName.size() + 1);
        idToName.push_back(InName);
        nameToId.emplace(InName, id);
        return FGameplayTag{id};
    }

    const char* FGameplayTagRegistry::GetTagName(FGameplayTag InTag) {
        if (!InTag.IsValid())
            return "";
        auto& idToName = TagIdToName();
        const size_t idx = static_cast<size_t>(InTag.Id - 1);
        if (idx >= idToName.size())
            return "";
        return idToName[idx].c_str();
    }

    bool FGameplayTagContainer::HasTag(FGameplayTag InTag) const {
        if (!InTag.IsValid())
            return false;
        return std::find_if(Tags.begin(), Tags.end(), [&](const FGameplayTag& tag) { return tag == InTag; }) !=
               Tags.end();
    }

    bool FGameplayTagContainer::HasAny(const FGameplayTagContainer& InOther) const {
        for (const FGameplayTag& tag : InOther.Tags) {
            if (HasTag(tag))
                return true;
        }
        return false;
    }

    bool FGameplayTagContainer::HasAll(const FGameplayTagContainer& InOther) const {
        for (const FGameplayTag& tag : InOther.Tags) {
            if (!HasTag(tag))
                return false;
        }
        return true;
    }

    void FGameplayTagContainer::AddTag(FGameplayTag InTag) {
        if (!InTag.IsValid() || HasTag(InTag))
            return;
        Tags.push_back(InTag);
    }

    void FGameplayTagContainer::RemoveTag(FGameplayTag InTag) {
        Tags.erase(std::remove_if(Tags.begin(), Tags.end(), [&](const FGameplayTag& tag) { return tag == InTag; }),
                   Tags.end());
    }

} // namespace Leon
