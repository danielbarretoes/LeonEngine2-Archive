#include "Assets/FMeshSimplifier.hpp"
#include "Renderer/FVertexLayout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace Leon {

    namespace {
        struct FClusterAccum {
            glm::vec3 Position{0.0f};
            glm::vec3 Normal{0.0f};
            glm::vec2 TexCoord{0.0f};
            glm::vec4 Tangent{0.0f};
            glm::vec3 Color{0.0f};
            glm::vec2 LightmapUV{0.0f};
            uint32_t Count = 0;
        };

        uint64_t PackCell(int InX, int InY, int InZ) {
            const auto wrap = [](int InV) { return static_cast<uint64_t>(static_cast<uint32_t>(InV) & 0x1FFFFFu); };
            return wrap(InX) | (wrap(InY) << 21) | (wrap(InZ) << 42);
        }

        void RebuildNormalsAndTangents(std::vector<FStaticMeshVertex>& InOutVertices,
                                       const std::vector<uint32_t>& InIndices) {
            for (auto& v : InOutVertices)
                v.Normal = glm::vec3(0.0f);
            const size_t n = InOutVertices.size();
            for (size_t i = 0; i + 2 < InIndices.size(); i += 3) {
                const uint32_t i0 = InIndices[i];
                const uint32_t i1 = InIndices[i + 1];
                const uint32_t i2 = InIndices[i + 2];
                if (i0 >= n || i1 >= n || i2 >= n)
                    continue;
                const glm::vec3 e1 = InOutVertices[i1].Position - InOutVertices[i0].Position;
                const glm::vec3 e2 = InOutVertices[i2].Position - InOutVertices[i0].Position;
                const glm::vec3 face = glm::cross(e1, e2);
                InOutVertices[i0].Normal += face;
                InOutVertices[i1].Normal += face;
                InOutVertices[i2].Normal += face;
            }
            for (auto& v : InOutVertices) {
                const float lenSq = glm::dot(v.Normal, v.Normal);
                v.Normal = lenSq > 1.0e-12f ? glm::normalize(v.Normal) : glm::vec3(0.0f, 1.0f, 0.0f);
            }
            GenerateLengyelTangents(InOutVertices, InIndices);
        }

        bool ClusterSubmesh(const std::vector<FStaticMeshVertex>& InVertices, const std::vector<uint32_t>& InIndices,
                            const FStaticSubmesh& InSubmesh, uint32_t InTargetTris, FStaticSubmesh& OutSubmesh,
                            std::vector<FStaticMeshVertex>& OutVertices, std::vector<uint32_t>& OutIndices) {
            OutSubmesh = InSubmesh;
            OutSubmesh.IndexOffset = static_cast<uint32_t>(OutIndices.size());
            OutSubmesh.VertexOffset = static_cast<uint32_t>(OutVertices.size());
            OutSubmesh.IndexCount = 0;
            OutSubmesh.VertexCount = 0;

            const uint32_t indexEnd = InSubmesh.IndexOffset + InSubmesh.IndexCount;
            if (InSubmesh.IndexCount < 3 || indexEnd > InIndices.size())
                return false;

            glm::vec3 bMin(std::numeric_limits<float>::max());
            glm::vec3 bMax(-std::numeric_limits<float>::max());
            uint32_t validVerts = 0;
            for (uint32_t i = InSubmesh.IndexOffset; i < indexEnd; ++i) {
                const uint32_t vi = InIndices[i];
                if (vi >= InVertices.size())
                    continue;
                bMin = glm::min(bMin, InVertices[vi].Position);
                bMax = glm::max(bMax, InVertices[vi].Position);
                ++validVerts;
            }
            if (validVerts < 3)
                return false;

            const glm::vec3 extent = glm::max(bMax - bMin, glm::vec3(1.0e-4f));
            const float maxExtent = std::max({extent.x, extent.y, extent.z});
            const uint32_t srcTris = InSubmesh.IndexCount / 3;
            const uint32_t target = std::max(1u, std::min(srcTris, InTargetTris));
            const int gridRes = std::max(2, static_cast<int>(std::ceil(std::cbrt(static_cast<double>(target * 2u)))));
            const float cell = maxExtent / static_cast<float>(gridRes);

            std::unordered_map<uint64_t, uint32_t> cellToCluster;
            std::vector<FClusterAccum> clusters;
            cellToCluster.reserve(target);
            clusters.reserve(target);

            auto addVertex = [&](const FStaticMeshVertex& InV) -> uint32_t {
                const glm::vec3 local = (InV.Position - bMin) / cell;
                const int cx = static_cast<int>(std::floor(local.x));
                const int cy = static_cast<int>(std::floor(local.y));
                const int cz = static_cast<int>(std::floor(local.z));
                const uint64_t key = PackCell(cx, cy, cz);
                auto it = cellToCluster.find(key);
                if (it == cellToCluster.end()) {
                    const uint32_t id = static_cast<uint32_t>(clusters.size());
                    cellToCluster.emplace(key, id);
                    FClusterAccum acc;
                    acc.Position = InV.Position;
                    acc.Normal = InV.Normal;
                    acc.TexCoord = InV.TexCoord;
                    acc.Tangent = InV.Tangent;
                    acc.Color = InV.Color;
                    acc.LightmapUV = InV.LightmapUV;
                    acc.Count = 1;
                    clusters.push_back(acc);
                    return id;
                }
                FClusterAccum& acc = clusters[it->second];
                acc.Position += InV.Position;
                acc.Normal += InV.Normal;
                acc.TexCoord += InV.TexCoord;
                acc.Tangent += InV.Tangent;
                acc.Color += InV.Color;
                acc.LightmapUV += InV.LightmapUV;
                acc.Count += 1;
                return it->second;
            };

            std::vector<uint32_t> remap;
            remap.reserve(InSubmesh.IndexCount);
            for (uint32_t i = InSubmesh.IndexOffset; i < indexEnd; ++i) {
                const uint32_t vi = InIndices[i];
                if (vi >= InVertices.size())
                    return false;
                remap.push_back(addVertex(InVertices[vi]));
            }
            if (clusters.empty())
                return false;

            const uint32_t baseVertex = static_cast<uint32_t>(OutVertices.size());
            OutVertices.reserve(OutVertices.size() + clusters.size());
            for (const auto& acc : clusters) {
                const float inv = 1.0f / static_cast<float>(std::max(acc.Count, 1u));
                FStaticMeshVertex v;
                v.Position = acc.Position * inv;
                v.Normal = acc.Normal * inv;
                v.TexCoord = acc.TexCoord * inv;
                v.Tangent = acc.Tangent * inv;
                v.Color = acc.Color * inv;
                v.LightmapUV = acc.LightmapUV * inv;
                OutVertices.push_back(v);
            }

            uint32_t kept = 0;
            for (size_t t = 0; t + 2 < remap.size(); t += 3) {
                const uint32_t a = remap[t];
                const uint32_t b = remap[t + 1];
                const uint32_t c = remap[t + 2];
                if (a == b || b == c || a == c)
                    continue;
                const glm::vec3 p0 = OutVertices[baseVertex + a].Position;
                const glm::vec3 p1 = OutVertices[baseVertex + b].Position;
                const glm::vec3 p2 = OutVertices[baseVertex + c].Position;
                if (glm::dot(glm::cross(p1 - p0, p2 - p0), glm::cross(p1 - p0, p2 - p0)) < 1.0e-16f)
                    continue;
                OutIndices.push_back(baseVertex + a);
                OutIndices.push_back(baseVertex + b);
                OutIndices.push_back(baseVertex + c);
                kept += 3;
            }

            OutSubmesh.IndexCount = kept;
            OutSubmesh.VertexCount = static_cast<uint32_t>(clusters.size());
            return kept >= 3;
        }

        void CopySubmesh(const std::vector<FStaticMeshVertex>& InVertices, const std::vector<uint32_t>& InIndices,
                         const FStaticSubmesh& InSubmesh, FStaticSubmesh& OutSubmesh,
                         std::vector<FStaticMeshVertex>& OutVertices, std::vector<uint32_t>& OutIndices) {
            OutSubmesh = InSubmesh;
            OutSubmesh.IndexOffset = static_cast<uint32_t>(OutIndices.size());
            OutSubmesh.VertexOffset = static_cast<uint32_t>(OutVertices.size());
            const uint32_t indexEnd = std::min(InSubmesh.IndexOffset + InSubmesh.IndexCount,
                                               static_cast<uint32_t>(InIndices.size()));
            for (uint32_t i = InSubmesh.IndexOffset; i < indexEnd; ++i) {
                const uint32_t vi = InIndices[i];
                if (vi >= InVertices.size())
                    continue;
                OutIndices.push_back(static_cast<uint32_t>(OutVertices.size()));
                OutVertices.push_back(InVertices[vi]);
            }
            OutSubmesh.IndexCount = static_cast<uint32_t>(OutIndices.size() - OutSubmesh.IndexOffset);
            OutSubmesh.VertexCount = static_cast<uint32_t>(OutVertices.size() - OutSubmesh.VertexOffset);
        }
    } // namespace

    bool FMeshSimplifier::Simplify(const std::vector<FStaticMeshVertex>& InVertices,
                                   const std::vector<uint32_t>& InIndices,
                                   const std::vector<FStaticSubmesh>& InSubmeshes, float InTriangleRatio,
                                   uint32_t InMinTriangleCount, FStaticMeshLOD& OutLOD) {
        OutLOD = {};
        OutLOD.TriangleRatio = InTriangleRatio;
        if (InVertices.empty() || InIndices.size() < 3)
            return false;

        const uint32_t srcTris = static_cast<uint32_t>(InIndices.size() / 3);
        const float ratio = std::clamp(InTriangleRatio, 0.01f, 1.0f);
        const uint32_t globalTarget = std::max(InMinTriangleCount, static_cast<uint32_t>(std::ceil(srcTris * ratio)));
        if (globalTarget >= srcTris)
            return false;

        for (const auto& sub : InSubmeshes) {
            const uint32_t subTris = sub.IndexCount / 3;
            uint32_t target = subTris;
            if (srcTris > 0)
                target = std::max(1u, static_cast<uint32_t>(std::ceil(static_cast<float>(subTris) * ratio)));
            target = std::max(target, std::min(subTris, InMinTriangleCount));

            FStaticSubmesh outSub;
            const uint32_t vBefore = static_cast<uint32_t>(OutLOD.Vertices.size());
            const uint32_t iBefore = static_cast<uint32_t>(OutLOD.Indices.size());
            bool ok = false;
            if (subTris > target)
                ok = ClusterSubmesh(InVertices, InIndices, sub, target, outSub, OutLOD.Vertices, OutLOD.Indices);
            if (!ok || outSub.IndexCount / 3 > subTris) {
                OutLOD.Vertices.resize(vBefore);
                OutLOD.Indices.resize(iBefore);
                CopySubmesh(InVertices, InIndices, sub, outSub, OutLOD.Vertices, OutLOD.Indices);
            }
            OutLOD.Submeshes.push_back(outSub);
        }

        if (OutLOD.Indices.size() < 3)
            return false;

        RebuildNormalsAndTangents(OutLOD.Vertices, OutLOD.Indices);
        if (OutLOD.Indices.size() / 3 >= srcTris)
            return false;
        return true;
    }

} // namespace Leon
