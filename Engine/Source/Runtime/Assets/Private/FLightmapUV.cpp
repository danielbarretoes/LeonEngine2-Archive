#include "Assets/FLightmapUV.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace Leon {

    bool FLightmapUV::HasLightmapUV(const UStaticMesh& InMesh) {
        if (InMesh.HasUniqueLightmapUV())
            return true;

        // Older .lmesh may already store PackLightmapCell UV1 without the flag set.
        const auto& verts = InMesh.GetVertices();
        if (verts.size() < 3)
            return false;
        float minU = verts[0].LightmapUV.x;
        float maxU = minU;
        float minV = verts[0].LightmapUV.y;
        float maxV = minV;
        for (const auto& v : verts) {
            minU = std::min(minU, v.LightmapUV.x);
            maxU = std::max(maxU, v.LightmapUV.x);
            minV = std::min(minV, v.LightmapUV.y);
            maxV = std::max(maxV, v.LightmapUV.y);
        }
        return (maxU - minU) > 0.01f && (maxV - minV) > 0.01f;
    }

    bool FLightmapUV::ComputeBarycentric(const glm::vec2& InP, const glm::vec2& InA, const glm::vec2& InB,
                                         const glm::vec2& InC, glm::vec3& OutBary) {
        glm::vec2 v0 = InB - InA;
        glm::vec2 v1 = InC - InA;
        glm::vec2 v2 = InP - InA;
        float den = v0.x * v1.y - v1.x * v0.y;
        if (std::abs(den) < 1e-12f)
            return false;
        float inv = 1.0f / den;
        float v = (v2.x * v1.y - v1.x * v2.y) * inv;
        float w = (v0.x * v2.y - v2.x * v0.y) * inv;
        float u = 1.0f - v - w;
        OutBary = {u, v, w};
        return true;
    }

    glm::vec3 FLightmapUV::Interpolate(const glm::vec3& InA, const glm::vec3& InB, const glm::vec3& InC,
                                       const glm::vec3& InBary) {
        return InA * InBary.x + InB * InBary.y + InC * InBary.z;
    }

    void FLightmapUV::GenerateBoxPackedLightmapUVs(UStaticMesh& InMesh, float InPadding) {
        auto& verts = InMesh.GetVertices();
        auto& indices = InMesh.GetIndices();
        if (verts.empty() || indices.size() < 3)
            return;

        const uint32_t triCount = static_cast<uint32_t>(indices.size() / 3);
        if (triCount == 0)
            return;

        const uint32_t grid = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(triCount))));
        const float cell = 1.0f / static_cast<float>(grid);
        const float pad = std::clamp(InPadding, 0.0f, cell * 0.45f);
        const float usable = cell - 2.0f * pad;

        std::vector<FStaticMeshVertex> newVerts;
        std::vector<uint32_t> newIndices;
        newVerts.reserve(static_cast<size_t>(triCount) * 3);
        newIndices.reserve(static_cast<size_t>(triCount) * 3);

        auto& submeshes = InMesh.GetSubmeshes();
        if (submeshes.empty()) {
            FStaticSubmesh whole;
            whole.IndexOffset = 0;
            whole.IndexCount = static_cast<uint32_t>(indices.size());
            whole.VertexOffset = 0;
            whole.VertexCount = static_cast<uint32_t>(verts.size());
            submeshes.push_back(whole);
        }

        std::vector<FStaticSubmesh> rebuiltSubmeshes;
        rebuiltSubmeshes.reserve(submeshes.size());

        for (const auto& sub : submeshes) {
            FStaticSubmesh out = sub;
            out.VertexOffset = static_cast<uint32_t>(newVerts.size());
            out.IndexOffset = static_cast<uint32_t>(newIndices.size());

            uint32_t firstTri = sub.IndexOffset / 3;
            uint32_t nTri = sub.IndexCount / 3;
            for (uint32_t t = 0; t < nTri; ++t) {
                uint32_t srcTri = firstTri + t;
                uint32_t i0 = indices[srcTri * 3 + 0];
                uint32_t i1 = indices[srcTri * 3 + 1];
                uint32_t i2 = indices[srcTri * 3 + 2];
                if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size())
                    continue;

                uint32_t gx = srcTri % grid;
                uint32_t gy = srcTri / grid;
                float ox = static_cast<float>(gx) * cell + pad;
                float oy = static_cast<float>(gy) * cell + pad;

                FStaticMeshVertex v0 = verts[i0];
                FStaticMeshVertex v1 = verts[i1];
                FStaticMeshVertex v2 = verts[i2];
                v0.LightmapUV = {ox, oy};
                v1.LightmapUV = {ox + usable, oy};
                v2.LightmapUV = {ox + usable * 0.5f, oy + usable};

                uint32_t base = static_cast<uint32_t>(newVerts.size());
                newVerts.push_back(v0);
                newVerts.push_back(v1);
                newVerts.push_back(v2);
                newIndices.push_back(base);
                newIndices.push_back(base + 1);
                newIndices.push_back(base + 2);
            }

            out.VertexCount = static_cast<uint32_t>(newVerts.size()) - out.VertexOffset;
            out.IndexCount = static_cast<uint32_t>(newIndices.size()) - out.IndexOffset;
            rebuiltSubmeshes.push_back(out);
        }

        verts = std::move(newVerts);
        indices = std::move(newIndices);
        submeshes = std::move(rebuiltSubmeshes);
        InMesh.CalculateBounds();
        InMesh.SetHasUniqueLightmapUV(true);
    }

    FLightmapUVValidationResult FLightmapUV::Validate(const UStaticMesh& InMesh, float InMinIslandPadding) {
        FLightmapUVValidationResult result;
        result.bHasUV1 = HasLightmapUV(InMesh);
        const auto& verts = InMesh.GetVertices();
        const auto& indices = InMesh.GetIndices();
        const uint32_t triCount = static_cast<uint32_t>(indices.size() / 3);

        for (const auto& v : verts) {
            if (v.LightmapUV.x < -1e-4f || v.LightmapUV.y < -1e-4f || v.LightmapUV.x > 1.0f + 1e-4f ||
                v.LightmapUV.y > 1.0f + 1e-4f) {
                result.bInUnitSquare = false;
            }
        }

        for (uint32_t t = 0; t < triCount; ++t) {
            uint32_t i0 = indices[t * 3 + 0];
            uint32_t i1 = indices[t * 3 + 1];
            uint32_t i2 = indices[t * 3 + 2];
            if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size())
                continue;
            glm::vec2 a = verts[i0].LightmapUV;
            glm::vec2 b = verts[i1].LightmapUV;
            glm::vec2 c = verts[i2].LightmapUV;

            for (uint32_t u = t + 1; u < triCount; ++u) {
                uint32_t j0 = indices[u * 3 + 0];
                uint32_t j1 = indices[u * 3 + 1];
                uint32_t j2 = indices[u * 3 + 2];
                if (j0 >= verts.size() || j1 >= verts.size() || j2 >= verts.size())
                    continue;
                glm::vec2 minA = glm::min(glm::min(a, b), c);
                glm::vec2 maxA = glm::max(glm::max(a, b), c);
                glm::vec2 minB = glm::min(glm::min(verts[j0].LightmapUV, verts[j1].LightmapUV), verts[j2].LightmapUV);
                glm::vec2 maxB = glm::max(glm::max(verts[j0].LightmapUV, verts[j1].LightmapUV), verts[j2].LightmapUV);
                bool overlap = maxA.x > minB.x + InMinIslandPadding && maxB.x > minA.x + InMinIslandPadding &&
                               maxA.y > minB.y + InMinIslandPadding && maxB.y > minA.y + InMinIslandPadding;
                if (overlap) {
                    result.bHasOverlaps = true;
                    result.bHasEnoughPadding = false;
                }
            }
        }

        result.bValid = result.bHasUV1 && result.bInUnitSquare && !result.bHasOverlaps;
        if (!result.bValid) {
            std::ostringstream ss;
            if (!result.bHasUV1)
                ss << "missing UV1; ";
            if (!result.bInUnitSquare)
                ss << "UV1 outside unit square; ";
            if (result.bHasOverlaps)
                ss << "overlapping charts; ";
            result.Message = ss.str();
        }
        return result;
    }

} // namespace Leon
