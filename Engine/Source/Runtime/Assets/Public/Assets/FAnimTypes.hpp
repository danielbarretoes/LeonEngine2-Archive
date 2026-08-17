#pragma once

#include "Core/Base.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    constexpr uint32_t kMaxBones = 128;
    constexpr uint32_t kMaxBoneInfluences = 4;

    /**
     * Authored mesh forward vs engine -Z forward.
     * SourcePosZ: Mixamo / typical FBX characters. Corrected on the skeletal
     * mesh component (180° yaw), never by rotating the pawn actor.
     */
    enum class EAssetForwardAxis : uint8_t {
        EngineNegZ = 0,
        SourcePosZ = 1,
    };

    constexpr uint32_t LSKELETON_MAGIC = 0x4C4B534C;     // 'LSKL'
    constexpr uint32_t LSKELETALMESH_MAGIC = 0x4D4B534C; // 'LSKM'
    constexpr uint32_t LANIM_MAGIC = 0x4D4E414C;         // 'LANM'
    constexpr uint32_t LBLEND_MAGIC = 0x444C424C;        // 'LBLD'
    constexpr uint32_t LSKELETON_VERSION = 1;
    constexpr uint32_t LSKELETALMESH_VERSION = 2;
    constexpr uint32_t LANIM_VERSION = 1;
    constexpr uint32_t LBLEND_VERSION = 1;

    /**
     * Compact replicated animation inputs. Pose is never sent over the network —
     * clients evaluate the same graph from these fields (deterministic).
     */
    struct FAnimRepState {
        float Speed = 0.0f;
        float Direction = 0.0f;
        float AimPitch = 0.0f;
        uint32_t Flags = 0;

        static constexpr uint32_t FlagInAir = 1u << 0;
        static constexpr uint32_t FlagCrouched = 1u << 1;

        bool IsInAir() const { return (Flags & FlagInAir) != 0; }
        bool IsCrouched() const { return (Flags & FlagCrouched) != 0; }

        void SetFlag(uint32_t InBit, bool bValue) {
            if (bValue)
                Flags |= InBit;
            else
                Flags &= ~InBit;
        }
    };

    struct FBoneTransform {
        glm::vec3 Translation{0.0f};
        glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 Scale{1.0f};

        static FBoneTransform Identity() { return {}; }

        glm::mat4 ToMatrix() const {
            glm::mat4 t = glm::translate(glm::mat4(1.0f), Translation);
            glm::mat4 r = glm::toMat4(glm::normalize(Rotation));
            glm::mat4 s = glm::scale(glm::mat4(1.0f), Scale);
            return t * r * s;
        }

        static FBoneTransform FromMatrix(const glm::mat4& InMatrix) {
            FBoneTransform out;
            out.Translation = glm::vec3(InMatrix[3]);
            glm::vec3 col0(InMatrix[0]);
            glm::vec3 col1(InMatrix[1]);
            glm::vec3 col2(InMatrix[2]);
            out.Scale = glm::vec3(glm::length(col0), glm::length(col1), glm::length(col2));
            glm::mat3 rot(1.0f);
            rot[0] = out.Scale.x > 1e-8f ? col0 / out.Scale.x : glm::vec3(1, 0, 0);
            rot[1] = out.Scale.y > 1e-8f ? col1 / out.Scale.y : glm::vec3(0, 1, 0);
            rot[2] = out.Scale.z > 1e-8f ? col2 / out.Scale.z : glm::vec3(0, 0, 1);
            out.Rotation = glm::normalize(glm::quat_cast(rot));
            return out;
        }

        static FBoneTransform Blend(const FBoneTransform& InA, const FBoneTransform& InB, float InAlpha) {
            float a = glm::clamp(InAlpha, 0.0f, 1.0f);
            FBoneTransform out;
            out.Translation = glm::mix(InA.Translation, InB.Translation, a);
            out.Rotation = glm::normalize(glm::slerp(InA.Rotation, InB.Rotation, a));
            out.Scale = glm::mix(InA.Scale, InB.Scale, a);
            return out;
        }
    };

    struct FPose {
        std::vector<FBoneTransform> LocalTransforms;

        void SetNum(uint32_t InCount) { LocalTransforms.assign(InCount, FBoneTransform::Identity()); }
        uint32_t Num() const { return static_cast<uint32_t>(LocalTransforms.size()); }
        bool IsEmpty() const { return LocalTransforms.empty(); }
    };

    struct FSkeletonBone {
        std::string Name;
        int32_t ParentIndex = -1;
        FBoneTransform RestLocal;
        glm::mat4 InverseBindPose{1.0f};
    };

    inline std::string StripBoneNamespace(const std::string& InName) {
        size_t colon = InName.find_last_of(':');
        if (colon == std::string::npos || colon + 1 >= InName.size())
            return InName;
        return InName.substr(colon + 1);
    }

    inline std::string ToLowerCopy(const std::string& InStr) {
        std::string out = InStr;
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    inline bool BoneNameContains(const std::string& InName, const char* InToken) {
        return ToLowerCopy(StripBoneNamespace(InName)).find(InToken) != std::string::npos;
    }

} // namespace Leon
