#include "AI/UBlackboardData.hpp"

#include <fstream>
#include <sstream>

namespace Leon {

    UBlackboardData::UBlackboardData(const std::string& InName) : UObject(InName) {}

    void UBlackboardData::AddKey(const FBlackboardKey& InKey) {
        Keys.push_back(InKey);
    }

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

} // namespace Leon
