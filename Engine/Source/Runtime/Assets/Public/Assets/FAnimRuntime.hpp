#pragma once

#include "Assets/FAnimTypes.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/UBlendSpace.hpp"
#include "Assets/USkeleton.hpp"

namespace Leon {

    /**
     * Canonical animation evaluation path.
     * Flow: sample clips → blend space / state blend → layered mask → local pose →
     * component matrices → skinning palette (component * inverseBind).
     */
    class FAnimRuntime {
    public:
        static void RestPose(const USkeleton& InSkeleton, FPose& OutPose);
        static void IdentityPose(const USkeleton& InSkeleton, FPose& OutPose);

        static void SampleSequence(const UAnimSequence& InSequence, const USkeleton& InSkeleton, float InTime,
                                   bool bLoop, FPose& OutPose);

        static void BlendPoses(const FPose& InA, const FPose& InB, float InAlpha, FPose& OutPose);

        static void SampleBlendSpace(const UBlendSpace& InBlendSpace, const USkeleton& InSkeleton, glm::vec2 InParam,
                                     float InTime, bool bLoop, FPose& OutPose);

        static void LayeredBlend(const FPose& InBase, const FPose& InOverlay, const std::vector<float>& InBoneWeights,
                                 FPose& OutPose);

        static void LocalToComponent(const USkeleton& InSkeleton, const FPose& InLocal,
                                     std::vector<glm::mat4>& OutComponent);

        static void BuildSkinningPalette(const USkeleton& InSkeleton, const std::vector<glm::mat4>& InComponent,
                                         std::vector<glm::mat4>& OutPalette);

        static glm::vec3 SampleVecKeys(const std::vector<FVectorKeyframe>& InKeys, float InTime,
                                       const glm::vec3& InFallback);
        static glm::quat SampleQuatKeys(const std::vector<FQuatKeyframe>& InKeys, float InTime,
                                        const glm::quat& InFallback);
    };

} // namespace Leon
