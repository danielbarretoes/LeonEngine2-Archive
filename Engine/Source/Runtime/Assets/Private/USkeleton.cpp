#include "Assets/USkeleton.hpp"
#include "Core/FLog.hpp"

#include <fstream>

namespace Leon {

    namespace {
        void WriteString(std::ostream& Out, const std::string& InStr) {
            uint32_t len = static_cast<uint32_t>(InStr.size());
            Out.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len > 0)
                Out.write(InStr.data(), len);
        }

        bool ReadString(std::istream& In, std::string& OutStr) {
            uint32_t len = 0;
            In.read(reinterpret_cast<char*>(&len), sizeof(len));
            if (!In || len > 1024 * 1024)
                return false;
            OutStr.assign(len, '\0');
            if (len > 0)
                In.read(OutStr.data(), len);
            return static_cast<bool>(In);
        }
    } // namespace

    USkeleton::USkeleton(const std::string& InName) : Name(InName), UUID(FUUID::FromPath(InName)) {}

    TRef<USkeleton> USkeleton::Create(const std::string& InName) {
        return MakeRef<USkeleton>(InName);
    }

    void USkeleton::RebuildLookup() {
        NameToIndex.clear();
        ShortNameToIndex.clear();
        NameToIndex.reserve(Bones.size());
        ShortNameToIndex.reserve(Bones.size());
        for (uint32_t i = 0; i < Bones.size(); ++i) {
            NameToIndex[Bones[i].Name] = static_cast<int32_t>(i);
            ShortNameToIndex[StripBoneNamespace(Bones[i].Name)] = static_cast<int32_t>(i);
        }
    }

    int32_t USkeleton::FindBoneIndex(const std::string& InName) const {
        auto it = NameToIndex.find(InName);
        if (it != NameToIndex.end())
            return it->second;
        auto shortIt = ShortNameToIndex.find(StripBoneNamespace(InName));
        if (shortIt != ShortNameToIndex.end())
            return shortIt->second;
        return -1;
    }

    FPose USkeleton::GetRestPose() const {
        FPose pose;
        pose.LocalTransforms.resize(Bones.size());
        for (size_t i = 0; i < Bones.size(); ++i)
            pose.LocalTransforms[i] = Bones[i].RestLocal;
        return pose;
    }

    std::vector<float> USkeleton::BuildDescendantMask(int32_t InRootBone) const {
        std::vector<float> mask(Bones.size(), 0.0f);
        if (InRootBone < 0 || InRootBone >= static_cast<int32_t>(Bones.size()))
            return mask;
        mask[static_cast<size_t>(InRootBone)] = 1.0f;
        bool bChanged = true;
        while (bChanged) {
            bChanged = false;
            for (size_t i = 0; i < Bones.size(); ++i) {
                int32_t parent = Bones[i].ParentIndex;
                if (parent >= 0 && mask[static_cast<size_t>(parent)] > 0.5f && mask[i] < 0.5f) {
                    mask[i] = 1.0f;
                    bChanged = true;
                }
            }
        }
        return mask;
    }

    int32_t USkeleton::FindFirstBoneContaining(const char* InToken) const {
        for (uint32_t i = 0; i < Bones.size(); ++i) {
            if (BoneNameContains(Bones[i].Name, InToken))
                return static_cast<int32_t>(i);
        }
        return -1;
    }

    std::vector<float> USkeleton::BuildUpperBodyMask() const {
        int32_t spine = FindFirstBoneContaining("spine");
        if (spine < 0)
            spine = FindFirstBoneContaining("chest");
        if (spine < 0)
            spine = FindFirstBoneContaining("spine1");
        if (spine < 0)
            return std::vector<float>(Bones.size(), 0.0f);
        return BuildDescendantMask(spine);
    }

    bool USkeleton::SaveToFile(const std::string& InFilePath) const {
        std::ofstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("USkeleton: Failed to open \"{0}\" for writing", InFilePath);
            return false;
        }

        uint32_t magic = LSKELETON_MAGIC;
        uint32_t version = LSKELETON_VERSION;
        uint32_t boneCount = static_cast<uint32_t>(Bones.size());
        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        file.write(reinterpret_cast<const char*>(&UUID.High), sizeof(UUID.High));
        file.write(reinterpret_cast<const char*>(&UUID.Low), sizeof(UUID.Low));
        WriteString(file, Name);
        file.write(reinterpret_cast<const char*>(&boneCount), sizeof(boneCount));

        for (const auto& bone : Bones) {
            WriteString(file, bone.Name);
            file.write(reinterpret_cast<const char*>(&bone.ParentIndex), sizeof(bone.ParentIndex));
            file.write(reinterpret_cast<const char*>(&bone.RestLocal.Translation), sizeof(glm::vec3));
            file.write(reinterpret_cast<const char*>(&bone.RestLocal.Rotation), sizeof(glm::quat));
            file.write(reinterpret_cast<const char*>(&bone.RestLocal.Scale), sizeof(glm::vec3));
            file.write(reinterpret_cast<const char*>(&bone.InverseBindPose), sizeof(glm::mat4));
        }
        return file.good();
    }

    bool USkeleton::LoadFromFile(const std::string& InFilePath) {
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("USkeleton: Failed to open \"{0}\" for reading", InFilePath);
            return false;
        }

        uint32_t magic = 0, version = 0, boneCount = 0;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (magic != LSKELETON_MAGIC || version != LSKELETON_VERSION) {
            LE_CORE_ERROR("USkeleton: Invalid magic/version in \"{0}\"", InFilePath);
            return false;
        }
        file.read(reinterpret_cast<char*>(&UUID.High), sizeof(UUID.High));
        file.read(reinterpret_cast<char*>(&UUID.Low), sizeof(UUID.Low));
        if (!ReadString(file, Name))
            return false;
        file.read(reinterpret_cast<char*>(&boneCount), sizeof(boneCount));
        if (!file || boneCount > kMaxBones * 4)
            return false;

        Bones.resize(boneCount);
        for (auto& bone : Bones) {
            if (!ReadString(file, bone.Name))
                return false;
            file.read(reinterpret_cast<char*>(&bone.ParentIndex), sizeof(bone.ParentIndex));
            file.read(reinterpret_cast<char*>(&bone.RestLocal.Translation), sizeof(glm::vec3));
            file.read(reinterpret_cast<char*>(&bone.RestLocal.Rotation), sizeof(glm::quat));
            file.read(reinterpret_cast<char*>(&bone.RestLocal.Scale), sizeof(glm::vec3));
            file.read(reinterpret_cast<char*>(&bone.InverseBindPose), sizeof(glm::mat4));
        }
        RebuildLookup();
        AssetPath = InFilePath;
        return file.good();
    }

} // namespace Leon
