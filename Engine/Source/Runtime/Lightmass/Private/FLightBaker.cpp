#include "Lightmass/FLightBaker.hpp"
#include "Assets/FLightmapUV.hpp"
#include "Renderer/FIBLMath.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Leon {
namespace {

    constexpr float kPI = 3.14159265358979323846f;
    constexpr float kEpsilon = 1e-4f;

    uint32_t HashCombine(uint64_t Seed, uint32_t X, uint32_t Y, uint32_t Sample) {
        uint64_t h = Seed;
        h ^= static_cast<uint64_t>(X) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h ^= static_cast<uint64_t>(Y) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h ^= static_cast<uint64_t>(Sample) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        return static_cast<uint32_t>(h);
    }

    float RadicalInverseVdC(uint32_t bits) {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    }

    glm::vec2 HammersleyLocal(uint32_t i, uint32_t N) {
        return {static_cast<float>(i) / static_cast<float>(N), RadicalInverseVdC(i)};
    }

    bool RayTriangle(const glm::vec3& Orig, const glm::vec3& Dir, const glm::vec3& V0, const glm::vec3& V1,
                      const glm::vec3& V2, float& OutT, glm::vec3& OutBary) {
        glm::vec3 e1 = V1 - V0;
        glm::vec3 e2 = V2 - V0;
        glm::vec3 pvec = glm::cross(Dir, e2);
        float det = glm::dot(e1, pvec);
        if (std::abs(det) < 1e-8f)
            return false;
        float invDet = 1.0f / det;
        glm::vec3 tvec = Orig - V0;
        float u = glm::dot(tvec, pvec) * invDet;
        if (u < 0.0f || u > 1.0f)
            return false;
        glm::vec3 qvec = glm::cross(tvec, e1);
        float v = glm::dot(Dir, qvec) * invDet;
        if (v < 0.0f || u + v > 1.0f)
            return false;
        float t = glm::dot(e2, qvec) * invDet;
        if (t <= kEpsilon)
            return false;
        OutT = t;
        OutBary = {1.0f - u - v, u, v};
        return true;
    }

} // namespace

    float FLightBaker::PointAttenuation(float InDistance, float InRadius) {
        if (InRadius <= 0.0f)
            return 0.0f;
        float d = std::max(InDistance, 0.0f);
        float ratio = d / InRadius;
        float soft = std::clamp(1.0f - ratio * ratio * ratio * ratio, 0.0f, 1.0f);
        soft *= soft;
        return soft / (d * d + 1.0f);
    }

    float FLightBaker::SpotConeFactor(const glm::vec3& InLightDir, const glm::vec3& InToLight,
                                        float InCutOffDeg, float InOuterCutOffDeg) {
        glm::vec3 L = glm::normalize(InToLight);
        glm::vec3 D = glm::normalize(InLightDir);
        float theta = glm::dot(L, -D);
        float inner = std::cos(glm::radians(InCutOffDeg));
        float outer = std::cos(glm::radians(InOuterCutOffDeg));
        float eps = std::max(inner - outer, 1e-4f);
        return std::clamp((theta - outer) / eps, 0.0f, 1.0f);
    }

    glm::vec3 FLightBaker::CosineSampleHemisphere(const glm::vec3& InNormal, float InU1, float InU2) {
        return ::Leon::CosineSampleHemisphere(glm::vec2(InU1, InU2), InNormal);
    }

    bool FLightBaker::IntersectScene(const FLightBakerScene& InScene, const glm::vec3& InOrigin,
                                       const glm::vec3& InDir, float InMaxT, float& OutT, uint32_t& OutTri,
                                       glm::vec3& OutBary) {
        bool hit = false;
        float bestT = InMaxT;
        for (uint32_t ti = 0; ti < InScene.Triangles.size(); ++ti) {
            const auto& tri = InScene.Triangles[ti];
            const auto& v0 = InScene.Vertices[tri.I0];
            const auto& v1 = InScene.Vertices[tri.I1];
            const auto& v2 = InScene.Vertices[tri.I2];
            float t = 0.0f;
            glm::vec3 bary;
            if (!RayTriangle(InOrigin, InDir, v0.Position, v1.Position, v2.Position, t, bary))
                continue;
            if (t < bestT) {
                bestT = t;
                OutT = t;
                OutTri = ti;
                OutBary = bary;
                hit = true;
            }
        }
        return hit;
    }

    static bool IsShadowed(const FLightBakerScene& Scene, const glm::vec3& Origin, const glm::vec3& Dir,
                             float MaxT) {
        float t;
        uint32_t tri;
        glm::vec3 bary;
        if (!FLightBaker::IntersectScene(Scene, Origin, Dir, MaxT, t, tri, bary))
            return false;
        return Scene.Triangles[tri].bCastShadow;
    }

    static glm::vec3 EvaluateDirect(const FLightBakerScene& Scene, const glm::vec3& Pos, const glm::vec3& Normal,
                                      const glm::vec3& Albedo, bool bDirectLightingPass) {
        glm::vec3 Lo(0.0f);
        const float kd = 1.0f; // diffuse only bake; metallic handled by reducing albedo contribution upstream

        for (const auto& dl : Scene.DirectionalLights) {
            if (bDirectLightingPass && !dl.bContributeDirect)
                continue;
            glm::vec3 L = glm::normalize(-dl.Light.Direction);
            float NdotL = std::max(glm::dot(Normal, L), 0.0f);
            if (NdotL <= 0.0f)
                continue;
            if (IsShadowed(Scene, Pos + Normal * kEpsilon * 2.0f, L, 1e6f))
                continue;
            glm::vec3 radiance = dl.Light.Color * dl.Light.Intensity;
            Lo += (Albedo / kPI) * radiance * NdotL * kd;
        }

        for (const auto& pl : Scene.PointLights) {
            if (bDirectLightingPass && !pl.bContributeDirect)
                continue;
            glm::vec3 toLight = pl.Light.Position - Pos;
            float dist = glm::length(toLight);
            if (dist < 1e-5f || dist > pl.Light.Radius)
                continue;
            glm::vec3 L = toLight / dist;
            float NdotL = std::max(glm::dot(Normal, L), 0.0f);
            if (NdotL <= 0.0f)
                continue;
            if (IsShadowed(Scene, Pos + Normal * kEpsilon * 2.0f, L, dist - kEpsilon))
                continue;
            float atten = FLightBaker::PointAttenuation(dist, pl.Light.Radius);
            glm::vec3 radiance = pl.Light.Color * pl.Light.Intensity * atten;
            Lo += (Albedo / kPI) * radiance * NdotL * kd;
        }

        for (const auto& sl : Scene.SpotLights) {
            if (bDirectLightingPass && !sl.bContributeDirect)
                continue;
            glm::vec3 toLight = sl.Light.Position - Pos;
            float dist = glm::length(toLight);
            if (dist < 1e-5f || dist > sl.Light.Radius)
                continue;
            glm::vec3 L = toLight / dist;
            float NdotL = std::max(glm::dot(Normal, L), 0.0f);
            if (NdotL <= 0.0f)
                continue;
            float cone = FLightBaker::SpotConeFactor(sl.Light.Direction, toLight, sl.Light.CutOff,
                                                     sl.Light.OuterCutOff);
            if (cone <= 0.0f)
                continue;
            if (IsShadowed(Scene, Pos + Normal * kEpsilon * 2.0f, L, dist - kEpsilon))
                continue;
            float atten = FLightBaker::PointAttenuation(dist, sl.Light.Radius) * cone;
            glm::vec3 radiance = sl.Light.Color * sl.Light.Intensity * atten;
            Lo += (Albedo / kPI) * radiance * NdotL * kd;
        }

        return Lo;
    }

    static glm::vec3 SurfaceRadiance(const FLightBakerScene& Scene, const FBakeVertex& V0, const FBakeVertex& V1,
                                      const FBakeVertex& V2, const glm::vec3& Bary) {
        glm::vec3 albedo = FLightmapUV::Interpolate(V0.Albedo, V1.Albedo, V2.Albedo, Bary);
        float metallic = V0.Metallic * Bary.x + V1.Metallic * Bary.y + V2.Metallic * Bary.z;
        albedo *= (1.0f - metallic);
        glm::vec3 emissive = FLightmapUV::Interpolate(V0.Emissive, V1.Emissive, V2.Emissive, Bary);
        glm::vec3 pos = FLightmapUV::Interpolate(V0.Position, V1.Position, V2.Position, Bary);
        glm::vec3 n = glm::normalize(FLightmapUV::Interpolate(V0.Normal, V1.Normal, V2.Normal, Bary));
        // Indirect gather: include Stationary lights as bounce sources.
        return emissive + EvaluateDirect(Scene, pos, n, albedo, false);
    }

    void FLightBaker::Bake(const FLightBakerScene& InScene, const FLightBakerSettings& InSettings,
                             std::vector<float>& OutRGBA32F) {
        const uint32_t W = InScene.AtlasWidth;
        const uint32_t H = InScene.AtlasHeight;
        OutRGBA32F.assign(static_cast<size_t>(W) * H * 4, 0.0f);
        if (W == 0 || H == 0)
            return;

        std::vector<float> coverage(static_cast<size_t>(W) * H, 0.0f);
        std::vector<glm::vec3> positions(static_cast<size_t>(W) * H, glm::vec3(0.0f));
        std::vector<glm::vec3> normals(static_cast<size_t>(W) * H, glm::vec3(0.0f, 1.0f, 0.0f));
        std::vector<glm::vec3> albedos(static_cast<size_t>(W) * H, glm::vec3(0.0f));
        std::vector<glm::vec3> emissives(static_cast<size_t>(W) * H, glm::vec3(0.0f));
        std::vector<int> chartOf(static_cast<size_t>(W) * H, -1);

        // Rasterize triangles into atlas using chart-local UV1
        for (uint32_t ti = 0; ti < InScene.Triangles.size(); ++ti) {
            const auto& tri = InScene.Triangles[ti];
            if (tri.ChartIndex >= InScene.Charts.size())
                continue;
            const auto& chart = InScene.Charts[tri.ChartIndex];
            const auto& v0 = InScene.Vertices[tri.I0];
            const auto& v1 = InScene.Vertices[tri.I1];
            const auto& v2 = InScene.Vertices[tri.I2];

            auto toAtlas = [&](const glm::vec2& uv) -> glm::vec2 {
                return uv * chart.Scale + chart.Bias;
            };

            glm::vec2 a = toAtlas(v0.LightmapUV);
            glm::vec2 b = toAtlas(v1.LightmapUV);
            glm::vec2 c = toAtlas(v2.LightmapUV);

            glm::vec2 minUV = glm::min(glm::min(a, b), c);
            glm::vec2 maxUV = glm::max(glm::max(a, b), c);
            int x0 = std::max(0, static_cast<int>(std::floor(minUV.x * W)) - 1);
            int y0 = std::max(0, static_cast<int>(std::floor(minUV.y * H)) - 1);
            int x1 = std::min(static_cast<int>(W) - 1, static_cast<int>(std::ceil(maxUV.x * W)) + 1);
            int y1 = std::min(static_cast<int>(H) - 1, static_cast<int>(std::ceil(maxUV.y * H)) + 1);

            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    glm::vec2 p((static_cast<float>(x) + 0.5f) / static_cast<float>(W),
                                (static_cast<float>(y) + 0.5f) / static_cast<float>(H));
                    glm::vec3 bary;
                    if (!FLightmapUV::ComputeBarycentric(p, a, b, c, bary))
                        continue;
                    if (bary.x < -1e-3f || bary.y < -1e-3f || bary.z < -1e-3f)
                        continue;
                    size_t idx = static_cast<size_t>(y) * W + static_cast<size_t>(x);
                    coverage[idx] = 1.0f;
                    chartOf[idx] = static_cast<int>(tri.ChartIndex);
                    positions[idx] = FLightmapUV::Interpolate(v0.Position, v1.Position, v2.Position, bary);
                    normals[idx] =
                        glm::normalize(FLightmapUV::Interpolate(v0.Normal, v1.Normal, v2.Normal, bary));
                    glm::vec3 albedo = FLightmapUV::Interpolate(v0.Albedo, v1.Albedo, v2.Albedo, bary);
                    float metallic = v0.Metallic * bary.x + v1.Metallic * bary.y + v2.Metallic * bary.z;
                    albedos[idx] = albedo * (1.0f - metallic);
                    emissives[idx] = FLightmapUV::Interpolate(v0.Emissive, v1.Emissive, v2.Emissive, bary);
                }
            }
        }

        const uint32_t samples = std::max(1u, InSettings.SamplesPerTexel);
        std::vector<glm::vec3> direct(static_cast<size_t>(W) * H, glm::vec3(0.0f));
        std::vector<glm::vec3> indirect(static_cast<size_t>(W) * H, glm::vec3(0.0f));
        std::vector<float> ao(static_cast<size_t>(W) * H, 1.0f);

        for (uint32_t y = 0; y < H; ++y) {
            for (uint32_t x = 0; x < W; ++x) {
                size_t idx = static_cast<size_t>(y) * W + x;
                if (coverage[idx] <= 0.0f)
                    continue;

                glm::vec3 pos = positions[idx];
                glm::vec3 n = normals[idx];
                glm::vec3 albedo = albedos[idx];
                direct[idx] = emissives[idx] + EvaluateDirect(InScene, pos, n, albedo, true);

                glm::vec3 indir(0.0f);
                float aoSum = 0.0f;
                for (uint32_t s = 0; s < samples; ++s) {
                    uint32_t h = HashCombine(InSettings.Seed, x, y, s);
                    glm::vec2 xi = HammersleyLocal(h % samples, samples);
                    // scramble slightly for uniqueness per texel
                    xi.x = std::fmod(xi.x + RadicalInverseVdC(h), 1.0f);
                    xi.y = std::fmod(xi.y + RadicalInverseVdC(h ^ 0xA5A5A5A5u), 1.0f);

                    glm::vec3 dir = CosineSampleHemisphere(n, xi.x, xi.y);
                    float tHit;
                    uint32_t hitTri;
                    glm::vec3 bary;
                    bool hit =
                        IntersectScene(InScene, pos + n * kEpsilon * 2.0f, dir, 1e6f, tHit, hitTri, bary);

                    if (InSettings.bAmbientOcclusion) {
                        float aoRayT = InSettings.AORadius;
                        float tAo;
                        uint32_t triAo;
                        glm::vec3 baryAo;
                        bool occluded = IntersectScene(InScene, pos + n * kEpsilon * 2.0f, dir, aoRayT, tAo,
                                                       triAo, baryAo);
                        aoSum += occluded ? 0.0f : 1.0f;
                    }

                    if (!hit)
                        continue;

                    // Multi-bounce: gather surface outgoing radiance (direct + emissive) for bounce 1,
                    // and recurse limited times with continuation.
                    const auto& tri = InScene.Triangles[hitTri];
                    const auto& hv0 = InScene.Vertices[tri.I0];
                    const auto& hv1 = InScene.Vertices[tri.I1];
                    const auto& hv2 = InScene.Vertices[tri.I2];
                    glm::vec3 hitPos = FLightmapUV::Interpolate(hv0.Position, hv1.Position, hv2.Position, bary);
                    glm::vec3 hitN =
                        glm::normalize(FLightmapUV::Interpolate(hv0.Normal, hv1.Normal, hv2.Normal, bary));
                    glm::vec3 throughput = albedo; // lambert BRDF * cos / pdf cancels for cosine sampling → albedo
                    glm::vec3 Li = SurfaceRadiance(InScene, hv0, hv1, hv2, bary);

                    glm::vec3 path = throughput * Li;
                    glm::vec3 curPos = hitPos;
                    glm::vec3 curN = hitN;
                    glm::vec3 curAlbedo =
                        FLightmapUV::Interpolate(hv0.Albedo, hv1.Albedo, hv2.Albedo, bary) *
                        (1.0f - (hv0.Metallic * bary.x + hv1.Metallic * bary.y + hv2.Metallic * bary.z));

                    for (uint32_t bounce = 1; bounce < InSettings.NumIndirectBounces; ++bounce) {
                        uint32_t hb = HashCombine(InSettings.Seed, x, y, s + bounce * 1024u);
                        glm::vec2 xi2 = HammersleyLocal(hb % samples, samples);
                        glm::vec3 dir2 = CosineSampleHemisphere(curN, xi2.x, xi2.y);
                        float t2;
                        uint32_t tri2;
                        glm::vec3 bary2;
                        if (!IntersectScene(InScene, curPos + curN * kEpsilon * 2.0f, dir2, 1e6f, t2, tri2,
                                            bary2))
                            break;
                        const auto& tref = InScene.Triangles[tri2];
                        const auto& w0 = InScene.Vertices[tref.I0];
                        const auto& w1 = InScene.Vertices[tref.I1];
                        const auto& w2 = InScene.Vertices[tref.I2];
                        path += curAlbedo * SurfaceRadiance(InScene, w0, w1, w2, bary2);
                        curPos = FLightmapUV::Interpolate(w0.Position, w1.Position, w2.Position, bary2);
                        curN = glm::normalize(FLightmapUV::Interpolate(w0.Normal, w1.Normal, w2.Normal, bary2));
                        float met = w0.Metallic * bary2.x + w1.Metallic * bary2.y + w2.Metallic * bary2.z;
                        curAlbedo =
                            FLightmapUV::Interpolate(w0.Albedo, w1.Albedo, w2.Albedo, bary2) * (1.0f - met);
                    }

                    indir += path;
                }

                if (samples > 0)
                    indir /= static_cast<float>(samples);
                indirect[idx] = indir * InSettings.IndirectIntensity;

                if (InSettings.bAmbientOcclusion && samples > 0) {
                    float aoVal = aoSum / static_cast<float>(samples);
                    ao[idx] = glm::mix(1.0f, aoVal, InSettings.AOIntensity);
                }
            }
        }

        // Dilate empty border texels once for filtering
        std::vector<glm::vec3> filtered(static_cast<size_t>(W) * H, glm::vec3(0.0f));
        for (uint32_t y = 0; y < H; ++y) {
            for (uint32_t x = 0; x < W; ++x) {
                size_t idx = static_cast<size_t>(y) * W + x;
                glm::vec3 irradiance = (direct[idx] + indirect[idx]) * ao[idx];
                // Store irradiance (lighting). Runtime multiplies by albedo/PI again would double-count.
                // Spec: lightmap = irradiance; Final = albedo/PI * irradiance for diffuse.
                // Our EvaluateDirect already includes albedo/PI. Store lighting as "irradiance-like"
                // by dividing out albedo when possible so runtime can do albedo * lightmap.
                glm::vec3 stored = irradiance;
                if (glm::length(albedos[idx]) > 1e-4f) {
                    // Convert baked outgoing diffuse approx back toward irradiance
                    stored = irradiance / glm::max(albedos[idx], glm::vec3(1e-3f));
                }
                filtered[idx] = stored;

                if (coverage[idx] <= 0.0f) {
                    // pull from neighbors
                    glm::vec3 sum(0.0f);
                    float wsum = 0.0f;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = static_cast<int>(x) + dx;
                            int ny = static_cast<int>(y) + dy;
                            if (nx < 0 || ny < 0 || nx >= static_cast<int>(W) || ny >= static_cast<int>(H))
                                continue;
                            size_t nidx = static_cast<size_t>(ny) * W + static_cast<size_t>(nx);
                            if (coverage[nidx] <= 0.0f)
                                continue;
                            sum += filtered[nidx];
                            wsum += 1.0f;
                        }
                    }
                    if (wsum > 0.0f)
                        filtered[idx] = sum / wsum;
                }
            }
        }

        for (uint32_t i = 0; i < W * H; ++i) {
            OutRGBA32F[i * 4 + 0] = filtered[i].r;
            OutRGBA32F[i * 4 + 1] = filtered[i].g;
            OutRGBA32F[i * 4 + 2] = filtered[i].b;
            OutRGBA32F[i * 4 + 3] = coverage[i];
        }
    }

} // namespace Leon
