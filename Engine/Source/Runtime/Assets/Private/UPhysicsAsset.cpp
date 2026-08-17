#include "Assets/UPhysicsAsset.hpp"

#include <fstream>

namespace Leon {

    UPhysicsAsset::UPhysicsAsset(const std::string& InName) : UObject(InName) {}

    bool UPhysicsAsset::SaveToFile(const std::string& InPath) const {
        std::ofstream out(InPath, std::ios::binary);
        if (!out)
            return false;
        uint32_t magic = 0x4853504C; // LPHY
        uint32_t ver = Version;
        uint32_t count = static_cast<uint32_t>(Bodies.size());
        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out.write(reinterpret_cast<const char*>(&ver), sizeof(ver));
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& body : Bodies) {
            uint32_t n = static_cast<uint32_t>(body.BoneName.size());
            out.write(reinterpret_cast<const char*>(&n), sizeof(n));
            out.write(body.BoneName.data(), n);
            uint8_t shape = static_cast<uint8_t>(body.Shape);
            out.write(reinterpret_cast<const char*>(&shape), 1);
            out.write(reinterpret_cast<const char*>(&body.Offset), sizeof(glm::vec3));
            out.write(reinterpret_cast<const char*>(&body.BoxExtent), sizeof(glm::vec3));
            out.write(reinterpret_cast<const char*>(&body.Radius), sizeof(float));
            out.write(reinterpret_cast<const char*>(&body.CapsuleHalfHeight), sizeof(float));
        }
        return true;
    }

    bool UPhysicsAsset::LoadFromFile(const std::string& InPath) {
        std::ifstream in(InPath, std::ios::binary);
        if (!in)
            return false;
        uint32_t magic = 0;
        uint32_t ver = 0;
        uint32_t count = 0;
        in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        in.read(reinterpret_cast<char*>(&ver), sizeof(ver));
        in.read(reinterpret_cast<char*>(&count), sizeof(count));
        if (magic != 0x4853504C)
            return false;
        Bodies.clear();
        Bodies.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            FPhysicsAssetBody body;
            uint32_t n = 0;
            in.read(reinterpret_cast<char*>(&n), sizeof(n));
            body.BoneName.resize(n);
            in.read(body.BoneName.data(), n);
            uint8_t shape = 0;
            in.read(reinterpret_cast<char*>(&shape), 1);
            body.Shape = static_cast<EPhysicsAssetBodyShape>(shape);
            in.read(reinterpret_cast<char*>(&body.Offset), sizeof(glm::vec3));
            in.read(reinterpret_cast<char*>(&body.BoxExtent), sizeof(glm::vec3));
            in.read(reinterpret_cast<char*>(&body.Radius), sizeof(float));
            in.read(reinterpret_cast<char*>(&body.CapsuleHalfHeight), sizeof(float));
            Bodies.push_back(body);
        }
        return true;
    }

} // namespace Leon
