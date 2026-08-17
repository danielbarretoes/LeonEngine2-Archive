#pragma once

#include "RHI/FBuffer.hpp"
#include "Renderer/FRenderingMath.hpp"

#include <glm/glm.hpp>
#include <cmath>
#include <vector>

namespace Leon {

    /**
     * Canonical interleaved mesh vertex. Locations MUST match PBR_Lit.glsl / ShadowDepth.glsl:
     *   0 Position   vec3
     *   1 Normal     vec3
     *   2 TexCoord   vec2  (UV0)
     *   3 Tangent    vec4  (xyz + handedness; B = cross(N, T) * w, T×B ≈ N when w = +1)
     *   4 Color      vec3  (linear)
     *   5 LightmapUV vec2  (UV1)
     *
     * Coordinate system: right-handed, Y-up, CCW front faces, OpenGL NDC.
     */
#pragma pack(push, 1)
    struct FCanonicalMeshVertex {
        glm::vec3 Position{0.0f};
        glm::vec3 Normal{0.0f, 1.0f, 0.0f};
        glm::vec2 TexCoord{0.0f};
        glm::vec4 Tangent{1.0f, 0.0f, 0.0f, 1.0f};
        glm::vec3 Color{1.0f};
        glm::vec2 LightmapUV{0.0f};
    };
#pragma pack(pop)
    static_assert(sizeof(FCanonicalMeshVertex) == 68, "canonical vertex must be 17 tightly packed floats");

    inline FBufferLayout MakeCanonicalMeshLayout() {
        return {{EShaderDataType::Float3, "aPos"},      {EShaderDataType::Float3, "aNormal"},
                {EShaderDataType::Float2, "aTexCoord"}, {EShaderDataType::Float4, "aTangent"},
                {EShaderDataType::Float3, "aColor"},    {EShaderDataType::Float2, "aLightmapUV"}};
    }

    /** Pack TBN into tangent.xyz + sign. Right-handed when T×B·N > 0 ⇒ w = +1. */
    inline glm::vec4 PackTangent(const glm::vec3& InTangent, const glm::vec3& InNormal, const glm::vec3& InBitangent) {
        glm::vec3 n = SafeNormalize(InNormal, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::vec3 t = SafeNormalize(InTangent, glm::vec3(1.0f, 0.0f, 0.0f));
        t = SafeNormalize(t - n * glm::dot(n, t), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec3 b = InBitangent;
        if (glm::dot(b, b) < kRenderingEpsilon)
            b = glm::cross(n, t);
        else
            b = glm::normalize(b);
        float sign = glm::dot(glm::cross(t, b), n) < 0.0f ? -1.0f : 1.0f;
        return glm::vec4(t, sign);
    }

    inline glm::vec3 UnpackBitangent(const glm::vec4& InTangent, const glm::vec3& InNormal) {
        glm::vec3 n = SafeNormalize(InNormal, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::vec3 t = SafeNormalize(glm::vec3(InTangent), glm::vec3(1.0f, 0.0f, 0.0f));
        return glm::cross(n, t) * InTangent.w;
    }

    inline void AppendCanonicalVertex(std::vector<float>& Out, const glm::vec3& InP, const glm::vec3& InN,
                                      const glm::vec2& InUV, const glm::vec3& InT, const glm::vec3& InB,
                                      const glm::vec3& InColor = glm::vec3(1.0f),
                                      const glm::vec2& InLightmapUV = glm::vec2(0.0f)) {
        glm::vec4 packed = PackTangent(InT, InN, InB);
        Out.push_back(InP.x);
        Out.push_back(InP.y);
        Out.push_back(InP.z);
        Out.push_back(InN.x);
        Out.push_back(InN.y);
        Out.push_back(InN.z);
        Out.push_back(InUV.x);
        Out.push_back(InUV.y);
        Out.push_back(packed.x);
        Out.push_back(packed.y);
        Out.push_back(packed.z);
        Out.push_back(packed.w);
        Out.push_back(InColor.x);
        Out.push_back(InColor.y);
        Out.push_back(InColor.z);
        Out.push_back(InLightmapUV.x);
        Out.push_back(InLightmapUV.y);
    }

    constexpr uint32_t kCanonicalVertexFloats = 17;

/** Skinned vertex: canonical attributes + 4 bone influences (GPU linear blend skinning). */
#pragma pack(push, 1)
    struct FSkinnedMeshVertex {
        glm::vec3 Position{0.0f};
        glm::vec3 Normal{0.0f, 1.0f, 0.0f};
        glm::vec2 TexCoord{0.0f};
        glm::vec4 Tangent{1.0f, 0.0f, 0.0f, 1.0f};
        glm::vec3 Color{1.0f};
        glm::vec2 LightmapUV{0.0f};
        glm::ivec4 BoneIndices{0};
        glm::vec4 BoneWeights{1.0f, 0.0f, 0.0f, 0.0f};
    };
#pragma pack(pop)
    static_assert(sizeof(FSkinnedMeshVertex) == 100, "skinned vertex must be tightly packed");

    inline FBufferLayout MakeSkinnedMeshLayout() {
        return {{EShaderDataType::Float3, "aPos"},       {EShaderDataType::Float3, "aNormal"},
                {EShaderDataType::Float2, "aTexCoord"},  {EShaderDataType::Float4, "aTangent"},
                {EShaderDataType::Float3, "aColor"},     {EShaderDataType::Float2, "aLightmapUV"},
                {EShaderDataType::Int4, "aBoneIndices"}, {EShaderDataType::Float4, "aBoneWeights"}};
    }

    /**
     * Lengyel tangent generation from UV0. Overwrites Tangent (xyz + handedness).
     * Degenerate UV triangles are skipped; those vertices keep an orthogonal fallback.
     */
    inline void GenerateLengyelTangents(std::vector<FCanonicalMeshVertex>& InOutVertices,
                                        const std::vector<uint32_t>& InIndices) {
        const size_t n = InOutVertices.size();
        if (n == 0 || InIndices.size() < 3)
            return;

        std::vector<glm::vec3> tanAcc(n, glm::vec3(0.0f));
        std::vector<glm::vec3> bitAcc(n, glm::vec3(0.0f));

        for (size_t i = 0; i + 2 < InIndices.size(); i += 3) {
            uint32_t i0 = InIndices[i];
            uint32_t i1 = InIndices[i + 1];
            uint32_t i2 = InIndices[i + 2];
            if (i0 >= n || i1 >= n || i2 >= n)
                continue;

            const FCanonicalMeshVertex& v0 = InOutVertices[i0];
            const FCanonicalMeshVertex& v1 = InOutVertices[i1];
            const FCanonicalMeshVertex& v2 = InOutVertices[i2];

            glm::vec3 e1 = v1.Position - v0.Position;
            glm::vec3 e2 = v2.Position - v0.Position;
            glm::vec2 d1 = v1.TexCoord - v0.TexCoord;
            glm::vec2 d2 = v2.TexCoord - v0.TexCoord;
            float det = d1.x * d2.y - d2.x * d1.y;
            if (std::abs(det) < kRenderingEpsilon)
                continue;
            float r = 1.0f / det;
            glm::vec3 t = (e1 * d2.y - e2 * d1.y) * r;
            glm::vec3 b = (e2 * d1.x - e1 * d2.x) * r;
            tanAcc[i0] += t;
            tanAcc[i1] += t;
            tanAcc[i2] += t;
            bitAcc[i0] += b;
            bitAcc[i1] += b;
            bitAcc[i2] += b;
        }

        for (size_t i = 0; i < n; ++i) {
            InOutVertices[i].Tangent = PackTangent(tanAcc[i], InOutVertices[i].Normal, bitAcc[i]);
        }
    }

} // namespace Leon
