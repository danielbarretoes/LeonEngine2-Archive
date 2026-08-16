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
    inline glm::vec4 PackTangent(const glm::vec3& InTangent, const glm::vec3& InNormal,
                                 const glm::vec3& InBitangent) {
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

} // namespace Leon
