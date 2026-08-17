#pragma once

#include "Core/Base.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Assets/FAnimTypes.hpp"
#include "Assets/USkeleton.hpp"

#include <string>
#include <vector>
#include <algorithm>

namespace Leon {

    struct FVectorKeyframe {
        float Time = 0.0f;
        glm::vec3 Value{0.0f};
    };

    struct FQuatKeyframe {
        float Time = 0.0f;
        glm::quat Value{1.0f, 0.0f, 0.0f, 0.0f};
    };

    struct FAnimBoneTrack {
        std::string BoneName;
        std::vector<FVectorKeyframe> TranslationKeys;
        std::vector<FQuatKeyframe> RotationKeys;
        std::vector<FVectorKeyframe> ScaleKeys;
    };

    class UAnimSequence : public std::enable_shared_from_this<UAnimSequence> {
    public:
        UAnimSequence(const std::string& InName = "AnimSequence");
        static TRef<UAnimSequence> Create(const std::string& InName = "AnimSequence");

        const std::string& GetName() const { return Name; }
        void SetName(const std::string& InName) { Name = InName; }

        const std::string& GetAssetPath() const { return AssetPath; }
        void SetAssetPath(const std::string& InPath) { AssetPath = InPath; }

        const FUUID& GetUUID() const { return UUID; }
        void SetUUID(const FUUID& InUUID) { UUID = InUUID; }

        const std::string& GetSkeletonPath() const { return SkeletonPath; }
        void SetSkeletonPath(const std::string& InPath) { SkeletonPath = InPath; }

        float GetDuration() const { return Duration; }
        void SetDuration(float InDuration) { Duration = std::max(InDuration, 0.0f); }

        float GetSampleRate() const { return SampleRate; }
        void SetSampleRate(float InRate) { SampleRate = std::max(InRate, 1.0f); }

        bool IsLooping() const { return bLooping; }
        void SetLooping(bool bInLooping) { bLooping = bInLooping; }

        std::vector<FAnimBoneTrack>& GetTracks() { return Tracks; }
        const std::vector<FAnimBoneTrack>& GetTracks() const { return Tracks; }

        /** Map tracks to skeleton bone indices (by name, then namespace-stripped name). */
        void LinkSkeleton(const TRef<USkeleton>& InSkeleton);

        const std::vector<int32_t>& GetTrackToBone() const { return TrackToBone; }

        float WrapTime(float InTime, bool bLoop) const;

        bool SaveToFile(const std::string& InFilePath) const;
        bool LoadFromFile(const std::string& InFilePath);

    private:
        std::string Name;
        std::string AssetPath;
        std::string SkeletonPath;
        FUUID UUID;
        FUUID SkeletonUUID;
        float Duration = 0.0f;
        float SampleRate = 30.0f;
        bool bLooping = true;
        std::vector<FAnimBoneTrack> Tracks;
        std::vector<int32_t> TrackToBone;
    };

} // namespace Leon
