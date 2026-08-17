#pragma once

#include "Core/Base.hpp"
#include "Gameplay/UActorComponent.hpp"
#include "Gameplay/UObject.hpp"

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Leon {

    enum class EBlackboardKeyType : uint8_t {
        Bool = 0,
        Int = 1,
        Float = 2,
        Vector = 3,
        Object = 4,
        Actor = 5,
        Name = 6,
    };

    using FBlackboardValue = std::variant<bool, int32_t, float, glm::vec3, void*, std::string>;

    struct FBlackboardKey {
        std::string Name;
        EBlackboardKeyType Type = EBlackboardKeyType::Bool;
        FBlackboardValue DefaultValue = false;
    };

    /**
     * Asset that defines blackboard keys. Runtime values live on UBlackboardComponent.
     */
    class UBlackboardData : public UObject {
    public:
        UBlackboardData(const std::string& InName = "BlackboardData");

        void AddKey(const FBlackboardKey& InKey);
        const std::vector<FBlackboardKey>& GetKeys() const { return Keys; }
        const FBlackboardKey* FindKey(const std::string& InName) const;

        bool SaveToFile(const std::string& InPath) const;
        bool LoadFromFile(const std::string& InPath);

        static constexpr uint32_t Magic = 0x4B424C4C; // 'LLBK'
        static constexpr uint32_t Version = 1;

    private:
        std::vector<FBlackboardKey> Keys;
    };

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
