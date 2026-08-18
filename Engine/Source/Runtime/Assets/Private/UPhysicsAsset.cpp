#include "Assets/UPhysicsAsset.hpp"
#include "Assets/USkeleton.hpp"

#include <fstream>
#include <unordered_set>

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

        bool AddCapsuleBody(UPhysicsAsset& OutAsset, const USkeleton& InSkeleton, const char* InToken, float InRadius,
                            float InHalfHeight, float InMass, std::unordered_set<int32_t>& InUsedBones) {
            const int32_t idx = InSkeleton.FindFirstBoneContaining(InToken);
            if (idx < 0 || InUsedBones.count(idx) != 0)
                return false;
            const auto& bones = InSkeleton.GetBones();
            if (idx >= static_cast<int32_t>(bones.size()))
                return false;
            FPhysicsAssetBody body;
            body.BoneName = bones[static_cast<size_t>(idx)].Name;
            body.Shape = EPhysicsAssetBodyShape::Capsule;
            body.Radius = InRadius;
            body.CapsuleHalfHeight = InHalfHeight;
            body.Mass = InMass;
            OutAsset.AddBody(body);
            InUsedBones.insert(idx);
            return true;
        }
    } // namespace

    UPhysicsAsset::UPhysicsAsset(const std::string& InName) : UObject(InName) {}

    TRef<UPhysicsAsset> UPhysicsAsset::CreateHumanoidFromSkeleton(const USkeleton& InSkeleton) {
        auto asset = MakeRef<UPhysicsAsset>("HumanoidRagdoll");
        std::unordered_set<int32_t> used;

        AddCapsuleBody(*asset, InSkeleton, "hips", 0.12f, 0.14f, 12.0f, used);
        if (asset->GetBodies().empty())
            AddCapsuleBody(*asset, InSkeleton, "pelvis", 0.12f, 0.14f, 12.0f, used);

        AddCapsuleBody(*asset, InSkeleton, "spine", 0.11f, 0.16f, 14.0f, used);
        AddCapsuleBody(*asset, InSkeleton, "neck", 0.06f, 0.08f, 3.0f, used);
        AddCapsuleBody(*asset, InSkeleton, "head", 0.10f, 0.10f, 5.0f, used);

        AddCapsuleBody(*asset, InSkeleton, "leftarm", 0.06f, 0.14f, 3.0f, used);
        AddCapsuleBody(*asset, InSkeleton, "leftforearm", 0.05f, 0.14f, 2.5f, used);
        AddCapsuleBody(*asset, InSkeleton, "rightarm", 0.06f, 0.14f, 3.0f, used);
        AddCapsuleBody(*asset, InSkeleton, "rightforearm", 0.05f, 0.14f, 2.5f, used);

        if (!AddCapsuleBody(*asset, InSkeleton, "leftupleg", 0.08f, 0.18f, 8.0f, used))
            AddCapsuleBody(*asset, InSkeleton, "leftthigh", 0.08f, 0.18f, 8.0f, used);
        AddCapsuleBody(*asset, InSkeleton, "leftleg", 0.07f, 0.18f, 5.0f, used);

        if (!AddCapsuleBody(*asset, InSkeleton, "rightupleg", 0.08f, 0.18f, 8.0f, used))
            AddCapsuleBody(*asset, InSkeleton, "rightthigh", 0.08f, 0.18f, 8.0f, used);
        AddCapsuleBody(*asset, InSkeleton, "rightleg", 0.07f, 0.18f, 5.0f, used);

        if (asset->GetBodies().size() < 2)
            return nullptr;

        const auto& bones = InSkeleton.GetBones();
        const auto& bodies = asset->GetBodies();
        for (size_t i = 0; i < bodies.size(); ++i) {
            const int32_t idx = InSkeleton.FindBoneIndex(bodies[i].BoneName);
            if (idx < 0)
                continue;
            const int32_t parent = bones[static_cast<size_t>(idx)].ParentIndex;
            if (parent < 0)
                continue;
            const std::string& parentName = bones[static_cast<size_t>(parent)].Name;
            for (size_t j = 0; j < bodies.size(); ++j) {
                if (bodies[j].BoneName != parentName)
                    continue;
                FPhysicsAssetConstraint c;
                c.BoneA = parentName;
                c.BoneB = bodies[i].BoneName;
                c.Type = EPhysicsConstraintType::SwingTwist;
                c.Axis = glm::vec3(0.0f, 1.0f, 0.0f);
                c.Swing1LimitRadians = 0.8f;
                c.Swing2LimitRadians = 0.6f;
                c.TwistLimitRadians = 0.4f;
                asset->AddConstraint(c);
                break;
            }
        }
        return asset;
    }

    bool UPhysicsAsset::SaveToFile(const std::string& InPath) const {
        std::ofstream out(InPath, std::ios::binary);
        if (!out)
            return false;
        uint32_t magic = Magic;
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
            out.write(reinterpret_cast<const char*>(&body.Mass), sizeof(float));
        }
        uint32_t constraintCount = static_cast<uint32_t>(Constraints.size());
        out.write(reinterpret_cast<const char*>(&constraintCount), sizeof(constraintCount));
        for (const auto& c : Constraints) {
            WriteString(out, c.BoneA);
            WriteString(out, c.BoneB);
            uint8_t type = static_cast<uint8_t>(c.Type);
            out.write(reinterpret_cast<const char*>(&type), 1);
            out.write(reinterpret_cast<const char*>(&c.RestLength), sizeof(float));
            out.write(reinterpret_cast<const char*>(&c.Axis), sizeof(glm::vec3));
            out.write(reinterpret_cast<const char*>(&c.Swing1LimitRadians), sizeof(float));
            out.write(reinterpret_cast<const char*>(&c.Swing2LimitRadians), sizeof(float));
            out.write(reinterpret_cast<const char*>(&c.TwistLimitRadians), sizeof(float));
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
        if (magic != Magic && magic != 0x48535950)
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
            if (ver >= 3)
                in.read(reinterpret_cast<char*>(&body.Mass), sizeof(float));
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
                if (ver >= 3) {
                    uint8_t type = 0;
                    in.read(reinterpret_cast<char*>(&type), 1);
                    c.Type = static_cast<EPhysicsConstraintType>(type);
                    in.read(reinterpret_cast<char*>(&c.RestLength), sizeof(float));
                    in.read(reinterpret_cast<char*>(&c.Axis), sizeof(glm::vec3));
                    in.read(reinterpret_cast<char*>(&c.Swing1LimitRadians), sizeof(float));
                    in.read(reinterpret_cast<char*>(&c.Swing2LimitRadians), sizeof(float));
                    in.read(reinterpret_cast<char*>(&c.TwistLimitRadians), sizeof(float));
                } else {
                    in.read(reinterpret_cast<char*>(&c.RestLength), sizeof(float));
                    c.Type = EPhysicsConstraintType::Distance;
                }
                Constraints.push_back(c);
            }
        }
        return true;
    }

} // namespace Leon
