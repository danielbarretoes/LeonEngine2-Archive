#include "AI/UBlackboardData.hpp"

#include <fstream>
#include <sstream>

namespace Leon {

    UBlackboardData::UBlackboardData(const std::string& InName) : UObject(InName) {}

    void UBlackboardData::AddKey(const FBlackboardKey& InKey) { Keys.push_back(InKey); }

    const FBlackboardKey* UBlackboardData::FindKey(const std::string& InName) const {
        for (const auto& key : Keys) {
            if (key.Name == InName)
                return &key;
        }
        return nullptr;
    }

    bool UBlackboardData::SaveToFile(const std::string& InPath) const {
        std::ofstream out(InPath, std::ios::trunc);
        if (!out)
            return false;
        out << "LBLACKBOARD " << Version << "\n";
        for (const auto& key : Keys) {
            out << "Key " << key.Name << " ";
            switch (key.Type) {
            case EBlackboardKeyType::Bool:
                out << "Bool " << (std::get<bool>(key.DefaultValue) ? 1 : 0) << "\n";
                break;
            case EBlackboardKeyType::Int:
                out << "Int " << std::get<int32_t>(key.DefaultValue) << "\n";
                break;
            case EBlackboardKeyType::Float:
                out << "Float " << std::get<float>(key.DefaultValue) << "\n";
                break;
            case EBlackboardKeyType::Vector: {
                auto v = std::get<glm::vec3>(key.DefaultValue);
                out << "Vector " << v.x << " " << v.y << " " << v.z << "\n";
                break;
            }
            case EBlackboardKeyType::Actor:
            case EBlackboardKeyType::Object:
                out << "Object\n";
                break;
            default:
                out << "Name " << std::get<std::string>(key.DefaultValue) << "\n";
                break;
            }
        }
        return true;
    }

    bool UBlackboardData::LoadFromFile(const std::string& InPath) {
        std::ifstream in(InPath);
        if (!in)
            return false;
        Keys.clear();
        std::string header;
        int ver = 0;
        in >> header >> ver;
        std::string tag;
        while (in >> tag) {
            if (tag != "Key")
                break;
            FBlackboardKey key;
            std::string type;
            in >> key.Name >> type;
            if (type == "Bool") {
                int v = 0;
                in >> v;
                key.Type = EBlackboardKeyType::Bool;
                key.DefaultValue = v != 0;
            } else if (type == "Int") {
                int32_t v = 0;
                in >> v;
                key.Type = EBlackboardKeyType::Int;
                key.DefaultValue = v;
            } else if (type == "Float") {
                float v = 0.0f;
                in >> v;
                key.Type = EBlackboardKeyType::Float;
                key.DefaultValue = v;
            } else if (type == "Vector") {
                glm::vec3 v{0.0f};
                in >> v.x >> v.y >> v.z;
                key.Type = EBlackboardKeyType::Vector;
                key.DefaultValue = v;
            } else if (type == "Object" || type == "Actor") {
                key.Type = EBlackboardKeyType::Actor;
                key.DefaultValue = static_cast<void*>(nullptr);
            } else {
                std::string v;
                in >> v;
                key.Type = EBlackboardKeyType::Name;
                key.DefaultValue = v;
            }
            Keys.push_back(key);
        }
        return true;
    }

    UBlackboardComponent::UBlackboardComponent(const std::string& InName) : UActorComponent(InName) {}

    void UBlackboardComponent::InitializeFrom(const TRef<UBlackboardData>& InAsset) {
        Asset = InAsset;
        Values.clear();
        if (!Asset)
            return;
        for (const auto& key : Asset->GetKeys())
            Values[key.Name] = key.DefaultValue;
    }

    void UBlackboardComponent::SetValueAsBool(const std::string& InKey, bool bValue) { Values[InKey] = bValue; }

    bool UBlackboardComponent::GetValueAsBool(const std::string& InKey, bool bDefault) const {
        auto it = Values.find(InKey);
        if (it == Values.end() || !std::holds_alternative<bool>(it->second))
            return bDefault;
        return std::get<bool>(it->second);
    }

    void UBlackboardComponent::SetValueAsInt(const std::string& InKey, int32_t InValue) { Values[InKey] = InValue; }

    int32_t UBlackboardComponent::GetValueAsInt(const std::string& InKey, int32_t InDefault) const {
        auto it = Values.find(InKey);
        if (it == Values.end() || !std::holds_alternative<int32_t>(it->second))
            return InDefault;
        return std::get<int32_t>(it->second);
    }

    void UBlackboardComponent::SetValueAsFloat(const std::string& InKey, float InValue) { Values[InKey] = InValue; }

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

    void UBlackboardComponent::SetValueAsObject(const std::string& InKey, void* InObject) { Values[InKey] = InObject; }

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

    bool UBlackboardComponent::HasKey(const std::string& InKey) const { return Values.find(InKey) != Values.end(); }

} // namespace Leon
