#pragma once

#include "Core/Base.hpp"
#include "Assets/FAnimTypes.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/USkeleton.hpp"

#include <string>
#include <vector>

namespace Leon {

    struct FBlendSpaceSample {
        glm::vec2 Coord{0.0f};
        std::string SequencePath;
        TRef<UAnimSequence> Sequence;
    };

    /**
     * 1D or 2D blend space. 1D uses AxisX only; 2D uses axis-normalized IDW over the
     * nearest samples (deterministic, circular Direction wrap).
     */
    class UBlendSpace : public std::enable_shared_from_this<UBlendSpace> {
    public:
        UBlendSpace(const std::string& InName = "BlendSpace");
        static TRef<UBlendSpace> Create(const std::string& InName = "BlendSpace");

        const std::string& GetName() const { return Name; }
        void SetName(const std::string& InName) { Name = InName; }

        const std::string& GetAssetPath() const { return AssetPath; }
        void SetAssetPath(const std::string& InPath) { AssetPath = InPath; }

        const std::string& GetSkeletonPath() const { return SkeletonPath; }
        void SetSkeletonPath(const std::string& InPath) { SkeletonPath = InPath; }

        bool Is2D() const { return bIs2D; }
        void Set2D(bool bIn2D) { bIs2D = bIn2D; }

        glm::vec2 GetAxisMin() const { return AxisMin; }
        glm::vec2 GetAxisMax() const { return AxisMax; }
        void SetAxisRange(const glm::vec2& InMin, const glm::vec2& InMax) {
            AxisMin = InMin;
            AxisMax = InMax;
        }

        std::vector<FBlendSpaceSample>& GetSamples() { return Samples; }
        const std::vector<FBlendSpaceSample>& GetSamples() const { return Samples; }

        void AddSample(const glm::vec2& InCoord, const TRef<UAnimSequence>& InSequence);
        void AddSample(const glm::vec2& InCoord, const std::string& InSequencePath);

        /** Resolve Sequence pointers from SequencePath via AssetManager. */
        void ResolveSequences();

        void EvaluateWeights(glm::vec2 InParam, std::vector<float>& OutWeights) const;

        bool SaveToFile(const std::string& InFilePath) const;
        bool LoadFromFile(const std::string& InFilePath);

    private:
        std::string Name;
        std::string AssetPath;
        std::string SkeletonPath;
        bool bIs2D = false;
        glm::vec2 AxisMin{0.0f, -180.0f};
        glm::vec2 AxisMax{600.0f, 180.0f};
        std::vector<FBlendSpaceSample> Samples;
    };

} // namespace Leon
