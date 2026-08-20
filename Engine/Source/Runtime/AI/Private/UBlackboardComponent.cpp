#include "AI/UBlackboardComponent.hpp"

namespace Leon {

    UBlackboardComponent::UBlackboardComponent(const std::string& InName) : UActorComponent(InName) {}

    void UBlackboardComponent::InitializeFrom(const TRef<UBlackboardData>& InAsset) {
        Asset = InAsset;
        Values.clear();
        if (!Asset)
            return;
        for (const auto& key : Asset->GetKeys())
            Values[key.Name] = key.DefaultValue;
    }

    void UBlackboardComponent::SetValueAsBool(const std::string& InKey, bool bValue) {
        Values[InKey] = bValue;
    }

    bool UBlackboardComponent::GetValueAsBool(const std::string& InKey, bool bDefault) const {
        auto it = Values.find(InKey);
        if (it == Values.end() || !std::holds_alternative<bool>(it->second))
            return bDefault;
        return std::get<bool>(it->second);
    }

    void UBlackboardComponent::SetValueAsInt(const std::string& InKey, int32_t InValue) {
        Values[InKey] = InValue;
    }

    int32_t UBlackboardComponent::GetValueAsInt(const std::string& InKey, int32_t InDefault) const {
        auto it = Values.find(InKey);
        if (it == Values.end() || !std::holds_alternative<int32_t>(it->second))
            return InDefault;
        return std::get<int32_t>(it->second);
    }

    void UBlackboardComponent::SetValueAsFloat(const std::string& InKey, float InValue) {
        Values[InKey] = InValue;
    }

    float UBlackboardComponent::GetValueAsFloat(const std::string& InKey, float InDefault) const {
        auto it = Values.find(InKey);
        if (it == Values.end() || !std::holds_alternative<float>(it->second))
            return InDefault;
        return std::get<float>(it->second);
    }

    void UBlackboardComponent::SetValueAsVector(const std::string& InKey, const glm::vec3& InValue) {
        Values[InKey] = InValue;
    }

    glm::vec3 UBlackboardComponent::GetValueAsVector(const std::string& InKey, const glm::vec3& InDefault) const {
        auto it = Values.find(InKey);
        if (it == Values.end() || !std::holds_alternative<glm::vec3>(it->second))
            return InDefault;
        return std::get<glm::vec3>(it->second);
    }

    void UBlackboardComponent::SetValueAsObject(const std::string& InKey, void* InObject) {
        Values[InKey] = InObject;
    }

    void* UBlackboardComponent::GetValueAsObject(const std::string& InKey) const {
        auto it = Values.find(InKey);
        if (it == Values.end() || !std::holds_alternative<void*>(it->second))
            return nullptr;
        return std::get<void*>(it->second);
    }

    void UBlackboardComponent::SetValueAsString(const std::string& InKey, const std::string& InValue) {
        Values[InKey] = InValue;
    }

    std::string UBlackboardComponent::GetValueAsString(const std::string& InKey) const {
        auto it = Values.find(InKey);
        if (it == Values.end() || !std::holds_alternative<std::string>(it->second))
            return {};
        return std::get<std::string>(it->second);
    }

    bool UBlackboardComponent::HasKey(const std::string& InKey) const {
        return Values.find(InKey) != Values.end();
    }

} // namespace Leon
