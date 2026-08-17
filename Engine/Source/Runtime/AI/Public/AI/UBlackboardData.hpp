#pragma once

#include "Core/Base.hpp"
#include "Gameplay/UObject.hpp"

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
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

} // namespace Leon
