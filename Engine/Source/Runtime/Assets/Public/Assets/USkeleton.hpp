#pragma once

#include "Core/Base.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Assets/FAnimTypes.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace Leon {

    class USkeleton : public std::enable_shared_from_this<USkeleton> {
    public:
        USkeleton(const std::string& InName = "Skeleton");
        static TRef<USkeleton> Create(const std::string& InName = "Skeleton");

        const std::string& GetName() const { return Name; }
        void SetName(const std::string& InName) { Name = InName; }

        const std::string& GetAssetPath() const { return AssetPath; }
        void SetAssetPath(const std::string& InPath) { AssetPath = InPath; }

        const FUUID& GetUUID() const { return UUID; }
        void SetUUID(const FUUID& InUUID) { UUID = InUUID; }

        std::vector<FSkeletonBone>& GetBones() { return Bones; }
        const std::vector<FSkeletonBone>& GetBones() const { return Bones; }

        uint32_t GetNumBones() const { return static_cast<uint32_t>(Bones.size()); }

        int32_t FindBoneIndex(const std::string& InName) const;
        void RebuildLookup();

        FPose GetRestPose() const;

        /** 1 for bone and all descendants, 0 otherwise. */
        std::vector<float> BuildDescendantMask(int32_t InRootBone) const;

        /** Upper body = first spine-like bone and descendants (Mixamo / UE naming). */
        std::vector<float> BuildUpperBodyMask() const;

        int32_t FindFirstBoneContaining(const char* InToken) const;

        bool SaveToFile(const std::string& InFilePath) const;
        bool LoadFromFile(const std::string& InFilePath);

    private:
        std::string Name;
        std::string AssetPath;
        FUUID UUID;
        std::vector<FSkeletonBone> Bones;
        std::unordered_map<std::string, int32_t> NameToIndex;
        std::unordered_map<std::string, int32_t> ShortNameToIndex;
    };

} // namespace Leon
