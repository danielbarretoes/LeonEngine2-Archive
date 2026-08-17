#include "Assets/UBlendSpace.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectPaths.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

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

    UBlendSpace::UBlendSpace(const std::string& InName) : Name(InName) {}

    TRef<UBlendSpace> UBlendSpace::Create(const std::string& InName) {
        return MakeRef<UBlendSpace>(InName);
    }

    void UBlendSpace::AddSample(const glm::vec2& InCoord, const TRef<UAnimSequence>& InSequence) {
        FBlendSpaceSample sample;
        sample.Coord = InCoord;
        sample.Sequence = InSequence;
        if (InSequence) {
            sample.SequencePath = FProjectPaths::MakeVirtualPath(InSequence->GetAssetPath());
            if (sample.SequencePath.empty())
                sample.SequencePath = InSequence->GetAssetPath();
        }
        Samples.push_back(std::move(sample));
    }

    void UBlendSpace::AddSample(const glm::vec2& InCoord, const std::string& InSequencePath) {
        FBlendSpaceSample sample;
        sample.Coord = InCoord;
        sample.SequencePath = FProjectPaths::MakeVirtualPath(InSequencePath);
        if (sample.SequencePath.empty())
            sample.SequencePath = InSequencePath;
        Samples.push_back(std::move(sample));
    }

    void UBlendSpace::ResolveSequences() {
        for (auto& sample : Samples) {
            if (!sample.Sequence && !sample.SequencePath.empty())
                sample.Sequence = UAssetManager::GetAnimSequence(sample.SequencePath);
        }
    }

    void UBlendSpace::EvaluateWeights(glm::vec2 InParam, std::vector<float>& OutWeights) const {
        OutWeights.assign(Samples.size(), 0.0f);
        if (Samples.empty())
            return;

        glm::vec2 p = InParam;
        p.x = glm::clamp(p.x, AxisMin.x, AxisMax.x);
        if (bIs2D)
            p.y = glm::clamp(p.y, AxisMin.y, AxisMax.y);
        else
            p.y = 0.0f;

        if (Samples.size() == 1) {
            OutWeights[0] = 1.0f;
            return;
        }

        if (!bIs2D) {
            std::vector<size_t> order(Samples.size());
            for (size_t i = 0; i < Samples.size(); ++i)
                order[i] = i;
            std::sort(order.begin(), order.end(),
                      [&](size_t a, size_t b) { return Samples[a].Coord.x < Samples[b].Coord.x; });

            if (p.x <= Samples[order.front()].Coord.x) {
                OutWeights[order.front()] = 1.0f;
                return;
            }
            if (p.x >= Samples[order.back()].Coord.x) {
                OutWeights[order.back()] = 1.0f;
                return;
            }
            for (size_t i = 0; i + 1 < order.size(); ++i) {
                float x0 = Samples[order[i]].Coord.x;
                float x1 = Samples[order[i + 1]].Coord.x;
                if (p.x >= x0 && p.x <= x1) {
                    float span = std::max(x1 - x0, 1e-6f);
                    float a = (p.x - x0) / span;
                    OutWeights[order[i]] = 1.0f - a;
                    OutWeights[order[i + 1]] = a;
                    return;
                }
            }
            OutWeights[order.back()] = 1.0f;
            return;
        }

        // Normalize Speed/Direction into a unit rectangle so IDW is not dominated by
        // the larger numeric axis (cm/s vs degrees). Keep only the nearest samples
        // so opposite-direction clips do not leak into 8-way locomotion.
        const float rangeX = std::max(AxisMax.x - AxisMin.x, 1e-3f);
        const float rangeY = std::max(AxisMax.y - AxisMin.y, 1e-3f);

        std::vector<float> distSq(Samples.size(), 0.0f);
        for (size_t i = 0; i < Samples.size(); ++i) {
            glm::vec2 d = p - Samples[i].Coord;
            d.x /= rangeX;
            d.y /= rangeY;
            if (rangeY > 1e-3f) {
                while (d.y > 0.5f)
                    d.y -= 1.0f;
                while (d.y < -0.5f)
                    d.y += 1.0f;
            }
            distSq[i] = glm::dot(d, d);
        }

        std::vector<size_t> order(Samples.size());
        for (size_t i = 0; i < Samples.size(); ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return distSq[a] < distSq[b]; });

        constexpr size_t kMaxNeighbors = 6;
        const size_t neighborCount = std::min(kMaxNeighbors, order.size());
        float weightSum = 0.0f;
        for (size_t n = 0; n < neighborCount; ++n) {
            const size_t i = order[n];
            const float w = 1.0f / (distSq[i] + 1e-6f);
            OutWeights[i] = w;
            weightSum += w;
        }
        if (weightSum > 1e-8f) {
            for (float& w : OutWeights)
                w /= weightSum;
        }
    }

    bool UBlendSpace::SaveToFile(const std::string& InFilePath) const {
        std::ofstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("UBlendSpace: Failed to open \"{0}\" for writing", InFilePath);
            return false;
        }
        uint32_t magic = LBLEND_MAGIC;
        uint32_t version = LBLEND_VERSION;
        uint8_t is2d = bIs2D ? 1 : 0;
        uint32_t count = static_cast<uint32_t>(Samples.size());
        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        WriteString(file, Name);
        WriteString(file, SkeletonPath);
        file.write(reinterpret_cast<const char*>(&is2d), sizeof(is2d));
        file.write(reinterpret_cast<const char*>(&AxisMin.x), sizeof(float));
        file.write(reinterpret_cast<const char*>(&AxisMin.y), sizeof(float));
        file.write(reinterpret_cast<const char*>(&AxisMax.x), sizeof(float));
        file.write(reinterpret_cast<const char*>(&AxisMax.y), sizeof(float));
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& sample : Samples) {
            file.write(reinterpret_cast<const char*>(&sample.Coord.x), sizeof(float));
            file.write(reinterpret_cast<const char*>(&sample.Coord.y), sizeof(float));
            std::string path = FProjectPaths::MakeVirtualPath(sample.SequencePath);
            if (path.empty())
                path = sample.SequencePath;
            WriteString(file, path);
        }
        return file.good();
    }

    bool UBlendSpace::LoadFromFile(const std::string& InFilePath) {
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("UBlendSpace: Failed to open \"{0}\" for reading", InFilePath);
            return false;
        }
        uint32_t magic = 0, version = 0, count = 0;
        uint8_t is2d = 0;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (magic != LBLEND_MAGIC || version != LBLEND_VERSION) {
            LE_CORE_ERROR("UBlendSpace: Invalid magic/version in \"{0}\"", InFilePath);
            return false;
        }
        if (!ReadString(file, Name) || !ReadString(file, SkeletonPath))
            return false;
        file.read(reinterpret_cast<char*>(&is2d), sizeof(is2d));
        file.read(reinterpret_cast<char*>(&AxisMin.x), sizeof(float));
        file.read(reinterpret_cast<char*>(&AxisMin.y), sizeof(float));
        file.read(reinterpret_cast<char*>(&AxisMax.x), sizeof(float));
        file.read(reinterpret_cast<char*>(&AxisMax.y), sizeof(float));
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        bIs2D = is2d != 0;
        if (!file || count > 4096)
            return false;
        Samples.resize(count);
        for (auto& sample : Samples) {
            file.read(reinterpret_cast<char*>(&sample.Coord.x), sizeof(float));
            file.read(reinterpret_cast<char*>(&sample.Coord.y), sizeof(float));
            if (!ReadString(file, sample.SequencePath))
                return false;
            std::string virt = FProjectPaths::MakeVirtualPath(sample.SequencePath);
            if (!virt.empty())
                sample.SequencePath = virt;
            sample.Sequence = nullptr;
        }
        AssetPath = InFilePath;
        return file.good();
    }

} // namespace Leon
