#pragma once

#include "AI/UBlackboardData.hpp"
#include "Gameplay/UActorComponent.hpp"

#include <string>
#include <unordered_map>

namespace Leon {

    class UBlackboardComponent : public UActorComponent {
    public:
        UBlackboardComponent(const std::string& InName = "BlackboardComponent");

        void InitializeFrom(const TRef<UBlackboardData>& InAsset);
        TRef<UBlackboardData> GetBlackboardAsset() const { return Asset; }

        void SetValueAsBool(const std::string& InKey, bool bValue);
        bool GetValueAsBool(const std::string& InKey, bool bDefault = false) const;
        void SetValueAsInt(const std::string& InKey, int32_t InValue);
        int32_t GetValueAsInt(const std::string& InKey, int32_t InDefault = 0) const;
        void SetValueAsFloat(const std::string& InKey, float InValue);
        float GetValueAsFloat(const std::string& InKey, float InDefault = 0.0f) const;
        void SetValueAsVector(const std::string& InKey, const glm::vec3& InValue);
        glm::vec3 GetValueAsVector(const std::string& InKey, const glm::vec3& InDefault = glm::vec3(0.0f)) const;
        void SetValueAsObject(const std::string& InKey, void* InObject);
        void* GetValueAsObject(const std::string& InKey) const;
        void SetValueAsString(const std::string& InKey, const std::string& InValue);
        std::string GetValueAsString(const std::string& InKey) const;

        bool HasKey(const std::string& InKey) const;
        const std::unordered_map<std::string, FBlackboardValue>& GetValues() const { return Values; }

    private:
        TRef<UBlackboardData> Asset;
        std::unordered_map<std::string, FBlackboardValue> Values;
    };

} // namespace Leon
