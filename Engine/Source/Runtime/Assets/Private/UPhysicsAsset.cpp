#include "Assets/UPhysicsAsset.hpp"

#include <fstream>

namespace Leon {

    namespace {
        void WriteString(std::ostream& Out, const std::string& InStr) {
            uint32_t len = static_cast<uint32_t>(InStr.size());
            Out.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len > 0)
                Out.write(InStr.data(), static_cast<std::streamsize>(len));
        }

        bool ReadString(std::istream& In, std::string& OutStr) {
            uint32_t len = 0;
            In.read(reinterpret_cast<char*>(&len), sizeof(len));
            if (!In || len > 1024 * 1024)
                return false;
            OutStr.assign(len, '\0');
            if (len > 0)
                In.read(OutStr.data(), static_cast<std::streamsize>(len));
            return static_cast<bool>(In);
        }
    } // namespace

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
            WriteString(out, body.BoneName);
            uint8_t shape = static_cast<uint8_t>(body.Shape);
            out.write(reinterpret_cast<const char*>(&shape), 1);
            out.write(reinterpret_cast<const char*>(&body.Offset), sizeof(glm::vec3));
            out.write(reinterpret_cast<const char*>(&body.BoxExtent), sizeof(glm::vec3));
            out.write(reinterpret_cast<const char*>(&body.Radius), sizeof(float));
            out.write(reinterpret_cast<const char*>(&body.CapsuleHalfHeight), sizeof(float));
        }
        uint32_t constraintCount = static_cast<uint32_t>(Constraints.size());
        out.write(reinterpret_cast<const char*>(&constraintCount), sizeof(constraintCount));
        for (const auto& c : Constraints) {
            WriteString(out, c.BoneA);
            WriteString(out, c.BoneB);
            out.write(reinterpret_cast<const char*>(&c.RestLength), sizeof(float));
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
        Constraints.clear();
        Bodies.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            FPhysicsAssetBody body;
            if (!ReadString(in, body.BoneName))
                return false;
            uint8_t shape = 0;
            in.read(reinterpret_cast<char*>(&shape), 1);
            body.Shape = static_cast<EPhysicsAssetBodyShape>(shape);
            in.read(reinterpret_cast<char*>(&body.Offset), sizeof(glm::vec3));
            in.read(reinterpret_cast<char*>(&body.BoxExtent), sizeof(glm::vec3));
            in.read(reinterpret_cast<char*>(&body.Radius), sizeof(float));
            in.read(reinterpret_cast<char*>(&body.CapsuleHalfHeight), sizeof(float));
            Bodies.push_back(body);
        }
        if (ver >= 2 && in) {
            uint32_t constraintCount = 0;
            in.read(reinterpret_cast<char*>(&constraintCount), sizeof(constraintCount));
            Constraints.reserve(constraintCount);
            for (uint32_t i = 0; i < constraintCount; ++i) {
                FPhysicsAssetConstraint c;
                if (!ReadString(in, c.BoneA) || !ReadString(in, c.BoneB))
                    return false;
                in.read(reinterpret_cast<char*>(&c.RestLength), sizeof(float));
                Constraints.push_back(c);
            }
        }
        return true;
    }

} // namespace Leon
