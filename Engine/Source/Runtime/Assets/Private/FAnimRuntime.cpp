#include "Assets/FAnimRuntime.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    void FAnimRuntime::RestPose(const USkeleton& InSkeleton, FPose& OutPose) {
        OutPose = InSkeleton.GetRestPose();
    }

    void FAnimRuntime::IdentityPose(const USkeleton& InSkeleton, FPose& OutPose) {
        OutPose.SetNum(InSkeleton.GetNumBones());
    }

    glm::vec3 FAnimRuntime::SampleVecKeys(const std::vector<FVectorKeyframe>& InKeys, float InTime,
                                          const glm::vec3& InFallback) {
        if (InKeys.empty())
            return InFallback;
        if (InKeys.size() == 1 || InTime <= InKeys.front().Time)
            return InKeys.front().Value;
        if (InTime >= InKeys.back().Time)
            return InKeys.back().Value;
        for (size_t i = 0; i + 1 < InKeys.size(); ++i) {
            if (InTime >= InKeys[i].Time && InTime <= InKeys[i + 1].Time) {
                float span = std::max(InKeys[i + 1].Time - InKeys[i].Time, 1e-6f);
                float a = (InTime - InKeys[i].Time) / span;
                return glm::mix(InKeys[i].Value, InKeys[i + 1].Value, a);
            }
        }
        return InKeys.back().Value;
    }

    glm::quat FAnimRuntime::SampleQuatKeys(const std::vector<FQuatKeyframe>& InKeys, float InTime,
                                           const glm::quat& InFallback) {
        if (InKeys.empty())
            return InFallback;
        if (InKeys.size() == 1 || InTime <= InKeys.front().Time)
            return glm::normalize(InKeys.front().Value);
        if (InTime >= InKeys.back().Time)
            return glm::normalize(InKeys.back().Value);
        for (size_t i = 0; i + 1 < InKeys.size(); ++i) {
            if (InTime >= InKeys[i].Time && InTime <= InKeys[i + 1].Time) {
                float span = std::max(InKeys[i + 1].Time - InKeys[i].Time, 1e-6f);
                float a = (InTime - InKeys[i].Time) / span;
                return glm::normalize(glm::slerp(InKeys[i].Value, InKeys[i + 1].Value, a));
            }
        }
        return glm::normalize(InKeys.back().Value);
    }

    void FAnimRuntime::SampleSequence(const UAnimSequence& InSequence, const USkeleton& InSkeleton, float InTime,
                                      bool bLoop, FPose& OutPose) {
        RestPose(InSkeleton, OutPose);
        float t = InSequence.WrapTime(InTime, bLoop);
        const auto& tracks = InSequence.GetTracks();
        const auto& map = InSequence.GetTrackToBone();
        for (size_t i = 0; i < tracks.size(); ++i) {
            int32_t bone = (i < map.size()) ? map[i] : InSkeleton.FindBoneIndex(tracks[i].BoneName);
            if (bone < 0 || bone >= static_cast<int32_t>(OutPose.Num()))
                continue;
            FBoneTransform& xf = OutPose.LocalTransforms[static_cast<size_t>(bone)];
            const FBoneTransform& rest = InSkeleton.GetBones()[static_cast<size_t>(bone)].RestLocal;
            xf.Translation = SampleVecKeys(tracks[i].TranslationKeys, t, rest.Translation);
            xf.Rotation = SampleQuatKeys(tracks[i].RotationKeys, t, rest.Rotation);
            xf.Scale = SampleVecKeys(tracks[i].ScaleKeys, t, rest.Scale);
        }
    }

    void FAnimRuntime::BlendPoses(const FPose& InA, const FPose& InB, float InAlpha, FPose& OutPose) {
        uint32_t n = std::max(InA.Num(), InB.Num());
        OutPose.SetNum(n);
        float a = glm::clamp(InAlpha, 0.0f, 1.0f);
        for (uint32_t i = 0; i < n; ++i) {
            FBoneTransform left = i < InA.Num() ? InA.LocalTransforms[i] : FBoneTransform::Identity();
            FBoneTransform right = i < InB.Num() ? InB.LocalTransforms[i] : FBoneTransform::Identity();
            OutPose.LocalTransforms[i] = FBoneTransform::Blend(left, right, a);
        }
    }

    void FAnimRuntime::SampleBlendSpace(const UBlendSpace& InBlendSpace, const USkeleton& InSkeleton, glm::vec2 InParam,
                                        float InTime, bool bLoop, FPose& OutPose) {
        RestPose(InSkeleton, OutPose);
        const auto& samples = InBlendSpace.GetSamples();
        if (samples.empty())
            return;

        std::vector<float> weights;
        InBlendSpace.EvaluateWeights(InParam, weights);

        bool bFirst = true;
        FPose acc;
        float accWeight = 0.0f;
        for (size_t i = 0; i < samples.size(); ++i) {
            if (i >= weights.size() || weights[i] < 1e-5f || !samples[i].Sequence)
                continue;
            FPose sampled;
            SampleSequence(*samples[i].Sequence, InSkeleton, InTime, bLoop, sampled);
            if (bFirst) {
                acc = std::move(sampled);
                accWeight = weights[i];
                bFirst = false;
            } else {
                float a = weights[i] / std::max(accWeight + weights[i], 1e-6f);
                FPose blended;
                BlendPoses(acc, sampled, a, blended);
                acc = std::move(blended);
                accWeight += weights[i];
            }
        }
        if (!bFirst)
            OutPose = std::move(acc);
    }

    void FAnimRuntime::LayeredBlend(const FPose& InBase, const FPose& InOverlay,
                                    const std::vector<float>& InBoneWeights, FPose& OutPose) {
        uint32_t n = std::max(InBase.Num(), InOverlay.Num());
        OutPose.SetNum(n);
        for (uint32_t i = 0; i < n; ++i) {
            FBoneTransform base = i < InBase.Num() ? InBase.LocalTransforms[i] : FBoneTransform::Identity();
            FBoneTransform over = i < InOverlay.Num() ? InOverlay.LocalTransforms[i] : base;
            float w = (i < InBoneWeights.size()) ? glm::clamp(InBoneWeights[i], 0.0f, 1.0f) : 0.0f;
            OutPose.LocalTransforms[i] = FBoneTransform::Blend(base, over, w);
        }
    }

    void FAnimRuntime::LocalToComponent(const USkeleton& InSkeleton, const FPose& InLocal,
                                        std::vector<glm::mat4>& OutComponent) {
        const auto& bones = InSkeleton.GetBones();
        OutComponent.assign(bones.size(), glm::mat4(1.0f));
        for (size_t i = 0; i < bones.size(); ++i) {
            glm::mat4 local =
                (i < InLocal.Num()) ? InLocal.LocalTransforms[i].ToMatrix() : bones[i].RestLocal.ToMatrix();
            int32_t parent = bones[i].ParentIndex;
            if (parent >= 0 && parent < static_cast<int32_t>(i))
                OutComponent[i] = OutComponent[static_cast<size_t>(parent)] * local;
            else
                OutComponent[i] = local;
        }
    }

    void FAnimRuntime::BuildSkinningPalette(const USkeleton& InSkeleton, const std::vector<glm::mat4>& InComponent,
                                            std::vector<glm::mat4>& OutPalette) {
        const auto& bones = InSkeleton.GetBones();
        OutPalette.resize(bones.size(), glm::mat4(1.0f));
        for (size_t i = 0; i < bones.size(); ++i) {
            glm::mat4 component = i < InComponent.size() ? InComponent[i] : glm::mat4(1.0f);
            OutPalette[i] = component * bones[i].InverseBindPose;
        }
    }

} // namespace Leon
