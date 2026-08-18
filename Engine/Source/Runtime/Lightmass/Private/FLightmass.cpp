#include "Lightmass/FLightmass.hpp"
#include "Assets/FAssetPath.hpp"
#include "Assets/FHDRImporter.hpp"
#include "Assets/FLightmapAsset.hpp"
#include "Assets/FLightmapUV.hpp"
#include "Assets/FTextureImporter.hpp"
#include "Assets/UAssetManager.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Core/FLog.hpp"
#include "Engine/Components.hpp"
#include "Engine/FMapSerializer.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Lightmass/FLightBaker.hpp"
#include "Lightmass/FLightmapBuilder.hpp"
#include "Renderer/FColorSpace.hpp"
#include "Renderer/FIBLMath.hpp"
#include "Renderer/FMaterial.hpp"
#include "Renderer/FMaterialInstance.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace Leon {
namespace {

    void LogLM(const std::string& Msg) { std::cout << "[Lightmass] " << Msg << "\n"; }

    uint64_t HashBytes(uint64_t Hash, const void* Data, size_t Size) {
        const auto* b = static_cast<const uint8_t*>(Data);
        for (size_t i = 0; i < Size; ++i) {
            Hash ^= b[i];
            Hash *= 1099511628211ull;
        }
        return Hash;
    }

    uint64_t HashString(uint64_t Hash, const std::string& InStr) {
        return HashBytes(Hash, InStr.data(), InStr.size());
    }

    uint64_t HashVec3(uint64_t Hash, const glm::vec3& InV) {
        return HashBytes(Hash, &InV, sizeof(InV));
    }

    uint64_t HashAssetFileFingerprint(uint64_t Hash, const std::string& InPath) {
        if (InPath.empty())
            return Hash;
        std::string resolved = InPath;
        if (!fs::path(InPath).is_absolute()) {
            fs::path candidate = fs::path(UAssetManager::GetContentRoot()) / InPath;
            if (!fs::exists(candidate))
                candidate = InPath;
            resolved = candidate.string();
        }
        Hash = HashString(Hash, FAssetPath::Normalize(resolved));
        std::error_code ec;
        if (fs::exists(resolved, ec)) {
            auto sz = fs::file_size(resolved, ec);
            auto mt = fs::last_write_time(resolved, ec);
            if (!ec) {
                Hash = HashBytes(Hash, &sz, sizeof(sz));
                auto ticks = mt.time_since_epoch().count();
                Hash = HashBytes(Hash, &ticks, sizeof(ticks));
            }
        }
        return Hash;
    }

    struct FBakeAlbedoTexture {
        std::vector<uint8_t> Pixels;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Channels = 4;
        bool bSRGB = true;
    };

    struct FBakeMaterialSample {
        glm::vec3 Albedo{0.7f};
        float Metallic = 0.0f;
        float Roughness = 0.5f;
        glm::vec3 Emissive{0.0f};
        FBakeAlbedoTexture AlbedoMap;
    };

    bool LoadBakeAlbedoTexture(const std::string& InVirtualOrAbsolutePath, FBakeAlbedoTexture& OutTex) {
        if (InVirtualOrAbsolutePath.empty())
            return false;

        fs::path resolved = InVirtualOrAbsolutePath;
        if (!resolved.is_absolute()) {
            fs::path underContent = fs::path(UAssetManager::GetContentRoot()) / InVirtualOrAbsolutePath;
            if (fs::exists(underContent))
                resolved = underContent;
        }
        if (!fs::exists(resolved))
            return false;

        FNativeTextureData tex;
        if (!tex.LoadFromFile(resolved.string()) || tex.Mips.empty() || tex.Mips[0].Pixels.empty())
            return false;

        const auto& mip = tex.Mips[0];
        OutTex.Pixels = mip.Pixels;
        OutTex.Width = mip.Width;
        OutTex.Height = mip.Height;
        OutTex.Channels = std::max(1u, tex.Header.Channels);
        OutTex.bSRGB = tex.Header.ColorSpace == 1 && !IsLinearDataTexturePath(resolved.string());
        return OutTex.Width > 0 && OutTex.Height > 0;
    }

    glm::vec3 SampleAlbedoAtUV(const FBakeMaterialSample& InMat, const glm::vec2& InUV) {
        const FBakeAlbedoTexture& tex = InMat.AlbedoMap;
        if (tex.Pixels.empty() || tex.Width == 0 || tex.Height == 0)
            return InMat.Albedo;

        float u = InUV.x - std::floor(InUV.x);
        float v = InUV.y - std::floor(InUV.y);
        int x = std::clamp(static_cast<int>(u * static_cast<float>(tex.Width)), 0, static_cast<int>(tex.Width) - 1);
        int y = std::clamp(static_cast<int>(v * static_cast<float>(tex.Height)), 0, static_cast<int>(tex.Height) - 1);
        size_t i = (static_cast<size_t>(y) * tex.Width + static_cast<size_t>(x)) * tex.Channels;
        if (i + tex.Channels - 1 >= tex.Pixels.size())
            return InMat.Albedo;

        glm::vec3 encoded(tex.Pixels[i + 0] / 255.0f,
                          (tex.Channels > 1 ? tex.Pixels[i + 1] : tex.Pixels[i + 0]) / 255.0f,
                          (tex.Channels > 2 ? tex.Pixels[i + 2] : tex.Pixels[i + 0]) / 255.0f);
        glm::vec3 linear = tex.bSRGB ? SRGBToLinear(encoded) : encoded;
        return linear * InMat.Albedo;
    }

    FBakeMaterialSample SampleMaterialInstance(const TRef<FMaterialInstance>& InMatInst) {
        FBakeMaterialSample sample;
        if (!InMatInst)
            return sample;

        sample.Albedo = InMatInst->GetAlbedoColor();
        sample.Metallic = InMatInst->GetMetallic();
        sample.Roughness = InMatInst->GetRoughness();
        sample.Emissive = InMatInst->GetEmissiveColor() * InMatInst->GetEmissiveIntensity();

        if (InMatInst->GetParent()) {
            const std::string& texPath = InMatInst->GetParent()->GetTexturePath(0);
            LoadBakeAlbedoTexture(texPath, sample.AlbedoMap);
        }
        return sample;
    }

    FBakeMaterialSample ResolveBakeMaterial(AActor& Actor) {
        if (Actor.HasComponent<FMaterialComponent>()) {
            auto& mat = Actor.GetComponent<FMaterialComponent>();
            if (mat.MaterialInstance)
                return SampleMaterialInstance(mat.MaterialInstance);
        }

        if (Actor.HasComponent<FStaticMeshComponent>()) {
            auto& smc = Actor.GetComponent<FStaticMeshComponent>();
            if (!smc.MaterialOverrides.empty()) {
                for (const auto& overrideMat : smc.MaterialOverrides) {
                    if (overrideMat)
                        return SampleMaterialInstance(overrideMat);
                }
            }
            if (smc.StaticMesh && !smc.StaticMesh->GetMaterialSlots().empty()) {
                const auto& slot = smc.StaticMesh->GetMaterialSlots()[0];
                if (slot.MaterialInstance)
                    return SampleMaterialInstance(slot.MaterialInstance);
                if (!slot.DefaultMaterialPath.empty()) {
                    auto base = UAssetManager::GetMaterial(slot.DefaultMaterialPath);
                    if (base)
                        return SampleMaterialInstance(base->CreateInstance());
                }
            }
        }
        return {};
    }

    void AppendBox(FLightBakerScene& Scene, uint32_t ChartIndex, const glm::mat4& M, float Size,
                   const FBakeMaterialSample& Mat, bool bCastShadow) {
        float h = Size * 0.5f;
        struct FLightmassFace {
            glm::vec3 P[4];
            glm::vec3 N;
        };
        FLightmassFace faces[6] = {
            { { glm::vec3(-h, -h, h), glm::vec3(h, -h, h), glm::vec3(h, h, h), glm::vec3(-h, h, h) },
              glm::vec3(0, 0, 1) },
            { { glm::vec3(h, -h, -h), glm::vec3(-h, -h, -h), glm::vec3(-h, h, -h), glm::vec3(h, h, -h) },
              glm::vec3(0, 0, -1) },
            { { glm::vec3(-h, h, h), glm::vec3(h, h, h), glm::vec3(h, h, -h), glm::vec3(-h, h, -h) },
              glm::vec3(0, 1, 0) },
            { { glm::vec3(-h, -h, -h), glm::vec3(h, -h, -h), glm::vec3(h, -h, h), glm::vec3(-h, -h, h) },
              glm::vec3(0, -1, 0) },
            { { glm::vec3(-h, -h, -h), glm::vec3(-h, -h, h), glm::vec3(-h, h, h), glm::vec3(-h, h, -h) },
              glm::vec3(-1, 0, 0) },
            { { glm::vec3(h, -h, h), glm::vec3(h, -h, -h), glm::vec3(h, h, -h), glm::vec3(h, h, h) },
              glm::vec3(1, 0, 0) },
        };

        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(M)));
        const float cell = 1.0f / 6.0f;
        for (int f = 0; f < 6; ++f) {
            float ox = static_cast<float>(f) * cell + 0.02f;
            float usable = cell - 0.04f;
            glm::vec2 uvs[4] = {{ox, 0.02f},
                                 {ox + usable, 0.02f},
                                 {ox + usable, 0.02f + usable},
                                 {ox, 0.02f + usable}};
            uint32_t base = static_cast<uint32_t>(Scene.Vertices.size());
            for (int i = 0; i < 4; ++i) {
                FBakeVertex v;
                glm::vec4 wp = M * glm::vec4(faces[f].P[i], 1.0f);
                v.Position = glm::vec3(wp);
                v.Normal = glm::normalize(normalMat * faces[f].N);
                v.LightmapUV = uvs[i];
                v.Albedo = Mat.Albedo;
                v.Metallic = Mat.Metallic;
                v.Roughness = Mat.Roughness;
                v.Emissive = Mat.Emissive;
                Scene.Vertices.push_back(v);
            }
            Scene.Triangles.push_back({base + 0, base + 1, base + 2, ChartIndex, bCastShadow});
            Scene.Triangles.push_back({base + 0, base + 2, base + 3, ChartIndex, bCastShadow});
        }
    }

    void AppendPlane(FLightBakerScene& Scene, uint32_t ChartIndex, const glm::mat4& M, float Width, float Depth,
                     const FBakeMaterialSample& Mat, bool bCastShadow) {
        float hx = Width * 0.5f;
        float hz = Depth * 0.5f;
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(M)));
        glm::vec3 n = glm::normalize(normalMat * glm::vec3(0, 1, 0));
        glm::vec3 p0 = glm::vec3(M * glm::vec4(-hx, 0, -hz, 1));
        glm::vec3 p1 = glm::vec3(M * glm::vec4(hx, 0, -hz, 1));
        glm::vec3 p2 = glm::vec3(M * glm::vec4(hx, 0, hz, 1));
        glm::vec3 p3 = glm::vec3(M * glm::vec4(-hx, 0, hz, 1));
        uint32_t base = static_cast<uint32_t>(Scene.Vertices.size());
        auto push = [&](const glm::vec3& p, const glm::vec2& uv) {
            FBakeVertex v;
            v.Position = p;
            v.Normal = n;
            v.LightmapUV = uv;
            v.Albedo = Mat.Albedo;
            v.Metallic = Mat.Metallic;
            v.Roughness = Mat.Roughness;
            v.Emissive = Mat.Emissive;
            Scene.Vertices.push_back(v);
        };
        push(p0, {0.02f, 0.02f});
        push(p1, {0.98f, 0.02f});
        push(p2, {0.98f, 0.98f});
        push(p3, {0.02f, 0.98f});
        Scene.Triangles.push_back({base + 0, base + 1, base + 2, ChartIndex, bCastShadow});
        Scene.Triangles.push_back({base + 0, base + 2, base + 3, ChartIndex, bCastShadow});
    }

    void AppendSphere(FLightBakerScene& Scene, uint32_t ChartIndex, const glm::mat4& M, float Radius,
                      const FBakeMaterialSample& Mat, bool bCastShadow, uint32_t Segments = 16,
                      uint32_t Rings = 12) {
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(M)));
        uint32_t base = static_cast<uint32_t>(Scene.Vertices.size());
        for (uint32_t y = 0; y <= Rings; ++y) {
            float v = static_cast<float>(y) / static_cast<float>(Rings);
            float phi = v * 3.14159265f;
            float sy = std::cos(phi);
            float sr = std::sin(phi);
            for (uint32_t x = 0; x <= Segments; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(Segments);
                float theta = u * 6.2831853f;
                glm::vec3 local(sr * std::cos(theta) * Radius, sy * Radius, sr * std::sin(theta) * Radius);
                glm::vec3 nLocal = glm::normalize(local);
                FBakeVertex vert;
                vert.Position = glm::vec3(M * glm::vec4(local, 1.0f));
                vert.Normal = glm::normalize(normalMat * nLocal);
                vert.LightmapUV = {u * 0.96f + 0.02f, v * 0.96f + 0.02f};
                vert.Albedo = Mat.Albedo;
                vert.Metallic = Mat.Metallic;
                vert.Roughness = Mat.Roughness;
                vert.Emissive = Mat.Emissive;
                Scene.Vertices.push_back(vert);
            }
        }
        for (uint32_t y = 0; y < Rings; ++y) {
            for (uint32_t x = 0; x < Segments; ++x) {
                uint32_t i0 = base + y * (Segments + 1) + x;
                uint32_t i1 = i0 + 1;
                uint32_t i2 = i0 + (Segments + 1);
                uint32_t i3 = i2 + 1;
                Scene.Triangles.push_back({i0, i2, i1, ChartIndex, bCastShadow});
                Scene.Triangles.push_back({i1, i2, i3, ChartIndex, bCastShadow});
            }
        }
    }

    void AppendCylinder(FLightBakerScene& Scene, uint32_t ChartIndex, const glm::mat4& M, float BottomRadius,
                        float TopRadius, float Height, const FBakeMaterialSample& Mat, bool bCastShadow,
                        uint32_t Segments = 16) {
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(M)));
        float hy = Height * 0.5f;
        uint32_t base = static_cast<uint32_t>(Scene.Vertices.size());
        for (uint32_t i = 0; i <= Segments; ++i) {
            float u = static_cast<float>(i) / static_cast<float>(Segments);
            float a = u * 6.2831853f;
            float c = std::cos(a);
            float s = std::sin(a);
            for (int row = 0; row < 2; ++row) {
                float y = (row == 0) ? -hy : hy;
                float r = (row == 0) ? BottomRadius : TopRadius;
                glm::vec3 local(c * r, y, s * r);
                glm::vec3 nLocal = glm::normalize(glm::vec3(c, 0.0f, s));
                FBakeVertex vert;
                vert.Position = glm::vec3(M * glm::vec4(local, 1.0f));
                vert.Normal = glm::normalize(normalMat * nLocal);
                vert.LightmapUV = {u * 0.96f + 0.02f, (row == 0 ? 0.02f : 0.98f)};
                vert.Albedo = Mat.Albedo;
                vert.Metallic = Mat.Metallic;
                vert.Roughness = Mat.Roughness;
                vert.Emissive = Mat.Emissive;
                Scene.Vertices.push_back(vert);
            }
        }
        for (uint32_t i = 0; i < Segments; ++i) {
            uint32_t i0 = base + i * 2;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + 2;
            uint32_t i3 = i0 + 3;
            Scene.Triangles.push_back({i0, i2, i1, ChartIndex, bCastShadow});
            Scene.Triangles.push_back({i1, i2, i3, ChartIndex, bCastShadow});
        }
    }

    void AppendRamp(FLightBakerScene& Scene, uint32_t ChartIndex, const glm::mat4& M, float Width, float Height,
                     float Depth, const FBakeMaterialSample& Mat, bool bCastShadow) {
        float hx = Width * 0.5f;
        float hz = Depth * 0.5f;
        // Wedge: bottom rectangle + slope + sides approximated as box-like tris
        glm::vec3 corners[6] = {{-hx, 0, -hz}, {hx, 0, -hz}, {hx, 0, hz}, {-hx, 0, hz},
                                 {-hx, Height, -hz}, {hx, Height, -hz}};
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(M)));
        auto push = [&](const glm::vec3& p, const glm::vec3& n, const glm::vec2& uv) {
            FBakeVertex v;
            v.Position = glm::vec3(M * glm::vec4(p, 1.0f));
            v.Normal = glm::normalize(normalMat * n);
            v.LightmapUV = uv;
            v.Albedo = Mat.Albedo;
            v.Metallic = Mat.Metallic;
            v.Roughness = Mat.Roughness;
            v.Emissive = Mat.Emissive;
            Scene.Vertices.push_back(v);
        };
        uint32_t base = static_cast<uint32_t>(Scene.Vertices.size());
        // Bottom
        push(corners[0], {0, -1, 0}, {0.02f, 0.02f});
        push(corners[1], {0, -1, 0}, {0.98f, 0.02f});
        push(corners[2], {0, -1, 0}, {0.98f, 0.48f});
        push(corners[3], {0, -1, 0}, {0.02f, 0.48f});
        Scene.Triangles.push_back({base + 0, base + 1, base + 2, ChartIndex, bCastShadow});
        Scene.Triangles.push_back({base + 0, base + 2, base + 3, ChartIndex, bCastShadow});
        // Slope
        glm::vec3 slopeN = glm::normalize(glm::cross(corners[1] - corners[0], corners[4] - corners[0]));
        uint32_t s = static_cast<uint32_t>(Scene.Vertices.size());
        push(corners[0], slopeN, {0.02f, 0.52f});
        push(corners[1], slopeN, {0.98f, 0.52f});
        push(corners[5], slopeN, {0.98f, 0.98f});
        push(corners[4], slopeN, {0.02f, 0.98f});
        Scene.Triangles.push_back({s + 0, s + 1, s + 2, ChartIndex, bCastShadow});
        Scene.Triangles.push_back({s + 0, s + 2, s + 3, ChartIndex, bCastShadow});
    }

    void AppendPyramid(FLightBakerScene& Scene, uint32_t ChartIndex, const glm::mat4& M, float Width, float Height,
                        float Depth, const FBakeMaterialSample& Mat, bool bCastShadow) {
        float hx = Width * 0.5f;
        float hz = Depth * 0.5f;
        glm::vec3 apex(0, Height, 0);
        glm::vec3 basePts[4] = {{-hx, 0, -hz}, {hx, 0, -hz}, {hx, 0, hz}, {-hx, 0, hz}};
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(M)));
        auto pushTri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float u0) {
            glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
            uint32_t base = static_cast<uint32_t>(Scene.Vertices.size());
            glm::vec3 pts[3] = {a, b, c};
            glm::vec2 uvs[3] = {{u0, 0.02f}, {u0 + 0.2f, 0.02f}, {u0 + 0.1f, 0.98f}};
            for (int i = 0; i < 3; ++i) {
                FBakeVertex v;
                v.Position = glm::vec3(M * glm::vec4(pts[i], 1.0f));
                v.Normal = glm::normalize(normalMat * n);
                v.LightmapUV = uvs[i];
                v.Albedo = Mat.Albedo;
                v.Metallic = Mat.Metallic;
                v.Roughness = Mat.Roughness;
                v.Emissive = Mat.Emissive;
                Scene.Vertices.push_back(v);
            }
            Scene.Triangles.push_back({base + 0, base + 1, base + 2, ChartIndex, bCastShadow});
        };
        pushTri(basePts[0], basePts[1], basePts[2], 0.02f);
        pushTri(basePts[0], basePts[2], basePts[3], 0.28f);
        pushTri(basePts[0], basePts[1], apex, 0.52f);
        pushTri(basePts[1], basePts[2], apex, 0.68f);
        pushTri(basePts[2], basePts[3], apex, 0.84f);
        pushTri(basePts[3], basePts[0], apex, 0.02f);
    }

    void AppendProceduralMesh(FLightBakerScene& Scene, uint32_t ChartIndex, const FMeshComponent& Mesh,
                               const glm::mat4& M, const FBakeMaterialSample& Mat) {
        const std::string& type = Mesh.MeshType;
        if (type == "Plane" || type == "Quad") {
            AppendPlane(Scene, ChartIndex, M, Mesh.MeshWidth, Mesh.MeshDepth, Mat, Mesh.bCastShadows);
        } else if (type == "Sphere") {
            AppendSphere(Scene, ChartIndex, M, Mesh.MeshRadius > 0 ? Mesh.MeshRadius : 0.5f, Mat, Mesh.bCastShadows);
        } else if (type == "Cylinder") {
            AppendCylinder(Scene, ChartIndex, M, Mesh.MeshRadius, Mesh.MeshRadius,
                           Mesh.MeshHeight > 0 ? Mesh.MeshHeight : 1.0f, Mat, Mesh.bCastShadows);
        } else if (type == "Cone") {
            AppendCylinder(Scene, ChartIndex, M, Mesh.MeshRadius, 0.0f, Mesh.MeshHeight > 0 ? Mesh.MeshHeight : 1.0f,
                           Mat, Mesh.bCastShadows);
        } else if (type == "Ramp") {
            AppendRamp(Scene, ChartIndex, M, Mesh.MeshWidth, Mesh.MeshHeight, Mesh.MeshDepth, Mat, Mesh.bCastShadows);
        } else if (type == "Pyramid") {
            AppendPyramid(Scene, ChartIndex, M, Mesh.MeshWidth, Mesh.MeshHeight, Mesh.MeshDepth, Mat,
                         Mesh.bCastShadows);
        } else {
            AppendBox(Scene, ChartIndex, M, Mesh.MeshSize > 0 ? Mesh.MeshSize : 1.0f, Mat, Mesh.bCastShadows);
        }
    }

    void PersistMissingLightmapUVs(UStaticMesh& Mesh) {
        if (FLightmapUV::HasLightmapUV(Mesh))
            return;
        FLightmapUV::GenerateBoxPackedLightmapUVs(Mesh);
        if (Mesh.GetAssetPath().empty())
            return;
        if (Mesh.SaveToFile(Mesh.GetAssetPath()))
            LogLM("Wrote lightmap UVs to " + Mesh.GetAssetPath());
        else
            LogLM("Warning: failed to persist lightmap UVs to " + Mesh.GetAssetPath());
    }

    void PersistWorldLightmapUVs(UWorld& InWorld) {
        for (auto& actorRef : InWorld.GetAllActors()) {
            if (!actorRef || !actorRef->HasComponent<FStaticMeshComponent>())
                continue;
            auto& smc = actorRef->GetComponent<FStaticMeshComponent>();
            if (smc.Mobility != EComponentMobility::Static || !smc.StaticMesh)
                continue;
            PersistMissingLightmapUVs(*smc.StaticMesh);
        }
    }

    void AppendStaticMesh(FLightBakerScene& Scene, uint32_t ChartIndex, UStaticMesh& Mesh, const glm::mat4& M,
                           const FBakeMaterialSample& Mat, bool bCastShadow) {
        auto& verts = Mesh.GetVertices();
        const auto& indices = Mesh.GetIndices();
        if (verts.empty() || indices.size() < 3)
            return;

        PersistMissingLightmapUVs(Mesh);

        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(M)));
        uint32_t base = static_cast<uint32_t>(Scene.Vertices.size());
        for (const auto& sv : verts) {
            FBakeVertex v;
            v.Position = glm::vec3(M * glm::vec4(sv.Position, 1.0f));
            v.Normal = glm::normalize(normalMat * sv.Normal);
            v.LightmapUV = sv.LightmapUV;
            v.Albedo = SampleAlbedoAtUV(Mat, sv.TexCoord);
            v.Metallic = Mat.Metallic;
            v.Roughness = Mat.Roughness;
            v.Emissive = Mat.Emissive;
            Scene.Vertices.push_back(v);
        }
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            Scene.Triangles.push_back(
                {base + indices[i], base + indices[i + 1], base + indices[i + 2], ChartIndex, bCastShadow});
        }
    }

    fs::path ResolveLightmapAbsolutePath(const std::string& InMapPath, const std::string& InLightmapVirtual) {
        fs::path mapDir = fs::path(InMapPath).parent_path();
        fs::path contentRoot = mapDir;
        if (mapDir.filename() == "Maps")
            contentRoot = mapDir.parent_path();
        // Support both "Lightmaps/X.llightmap" and "/Game/Lightmaps/X.llightmap"
        std::string rel = InLightmapVirtual;
        if (rel.rfind("/Game/", 0) == 0)
            rel = rel.substr(6);
        else if (rel.rfind("Game/", 0) == 0)
            rel = rel.substr(5);
        return contentRoot / rel;
    }

    FWorldSettingsComponent* FindWorldSettings(UWorld& InWorld) {
        for (auto& actorRef : InWorld.GetAllActors()) {
            if (actorRef && actorRef->HasComponent<FWorldSettingsComponent>())
                return &actorRef->GetComponent<FWorldSettingsComponent>();
        }
        return nullptr;
    }

    const FSkyboxComponent* FindSkybox(UWorld& InWorld) {
        const FSkyboxComponent* found = nullptr;
        for (auto& actorRef : InWorld.GetAllActors()) {
            if (actorRef && actorRef->HasComponent<FSkyboxComponent>())
                found = &actorRef->GetComponent<FSkyboxComponent>();
        }
        return found;
    }

    void FillBakeEnvironment(FLightBakerScene& Scene, const FSkyboxComponent& InSkybox) {
        Scene.Environment.bEnabled = InSkybox.bEnabled;
        Scene.Environment.Zenith = InSkybox.SkyZenithColor;
        Scene.Environment.Horizon = InSkybox.HorizonColor;
        Scene.Environment.Ground = InSkybox.GroundColor;
        Scene.Environment.Intensity = InSkybox.EnvironmentIntensity;

        if (!InSkybox.bEnabled || !InSkybox.bUseHDREnvironmentMap)
            return;

        std::string hdrPath = InSkybox.HDREnvironmentMapPath;
        if (hdrPath.empty())
            return;
        hdrPath = UAssetManager::ResolveVirtualPath(hdrPath);
        if (hdrPath.size() < 5 || hdrPath.substr(hdrPath.size() - 5) != ".lhdr")
            return;

        FNativeHDRData hdr;
        if (!hdr.LoadFromFile(hdrPath) || hdr.Pixels.empty())
            return;
        Scene.Environment.HDRRGBA = std::move(hdr.Pixels);
        Scene.Environment.HDRWidth = static_cast<int>(hdr.Header.Width);
        Scene.Environment.HDRHeight = static_cast<int>(hdr.Header.Height);
    }

    void ApplyWorldSettings(const FWorldSettingsComponent& Ws, FLightmassSettings& OutSettings,
                            std::string& OutLightmapVirtual) {
        if (Ws.LightmapResolution > 0)
            OutSettings.LightmapResolution = Ws.LightmapResolution;
        OutSettings.NumIndirectBounces = Ws.NumIndirectBounces;
        OutSettings.SamplesPerTexel = Ws.SamplesPerTexel;
        OutSettings.IndirectIntensity = Ws.IndirectIntensity;
        OutSettings.bAmbientOcclusion = Ws.bAmbientOcclusion;
        OutSettings.AOIntensity = Ws.AOIntensity;
        OutSettings.AORadius = Ws.AORadius;
        OutSettings.TexelPadding = Ws.TexelPadding;
        OutSettings.WorldScale = Ws.WorldScale;
        OutSettings.LightingBuildQuality = Ws.LightingBuildQuality;
        if (!Ws.LightmapAssetPath.empty())
            OutLightmapVirtual = Ws.LightmapAssetPath;
    }

} // namespace

    uint64_t FLightmass::ComputeBakeInputHash(const UWorld& InWorld, const FLightmassSettings& InSettings) {
        uint64_t hash = 14695981039346656037ull;
        constexpr uint32_t kBakerAlgorithmVersion = 4;
        hash = HashBytes(hash, &kBakerAlgorithmVersion, sizeof(kBakerAlgorithmVersion));

        hash = HashBytes(hash, &InSettings.LightmapResolution, sizeof(InSettings.LightmapResolution));
        hash = HashBytes(hash, &InSettings.NumIndirectBounces, sizeof(InSettings.NumIndirectBounces));
        hash = HashBytes(hash, &InSettings.SamplesPerTexel, sizeof(InSettings.SamplesPerTexel));
        hash = HashBytes(hash, &InSettings.IndirectIntensity, sizeof(InSettings.IndirectIntensity));
        uint8_t ao = InSettings.bAmbientOcclusion ? 1 : 0;
        hash = HashBytes(hash, &ao, sizeof(ao));
        hash = HashBytes(hash, &InSettings.AOIntensity, sizeof(InSettings.AOIntensity));
        hash = HashBytes(hash, &InSettings.AORadius, sizeof(InSettings.AORadius));
        hash = HashBytes(hash, &InSettings.TexelPadding, sizeof(InSettings.TexelPadding));
        hash = HashBytes(hash, &InSettings.WorldScale, sizeof(InSettings.WorldScale));
        hash = HashBytes(hash, &InSettings.DeterministicSeed, sizeof(InSettings.DeterministicSeed));
        uint8_t quality = static_cast<uint8_t>(InSettings.LightingBuildQuality);
        hash = HashBytes(hash, &quality, sizeof(quality));

        for (const auto& actorRef : InWorld.GetAllActors()) {
            if (!actorRef)
                continue;
            AActor& actor = *actorRef;

            if (actor.HasComponent<FTransformComponent>()) {
                const auto& t = actor.GetComponent<FTransformComponent>();
                hash = HashVec3(hash, t.Translation);
                hash = HashVec3(hash, t.Rotation);
                hash = HashVec3(hash, t.Scale);
            }

            if (actor.HasComponent<FStaticMeshComponent>()) {
                const auto& smc = actor.GetComponent<FStaticMeshComponent>();
                uint8_t mob = static_cast<uint8_t>(smc.Mobility);
                hash = HashBytes(hash, &mob, sizeof(mob));
                hash = HashBytes(hash, &smc.LightmapResolution, sizeof(smc.LightmapResolution));
                hash = HashBytes(hash, &smc.bCastShadows, sizeof(smc.bCastShadows));
                hash = HashString(hash, actor.GetName());
                if (smc.Mobility == EComponentMobility::Static && smc.StaticMesh) {
                    hash = HashString(hash, smc.StaticMesh->GetAssetPath());
                    hash = HashAssetFileFingerprint(hash, smc.StaticMesh->GetAssetPath());
                    for (const auto& slot : smc.StaticMesh->GetMaterialSlots()) {
                        hash = HashString(hash, slot.DefaultMaterialPath);
                        hash = HashAssetFileFingerprint(hash, slot.DefaultMaterialPath);
                    }
                }
                for (const auto& path : smc.MaterialOverridePaths) {
                    hash = HashString(hash, path);
                    hash = HashAssetFileFingerprint(hash, path);
                }
                if (actor.HasComponent<FMaterialComponent>()) {
                    hash = HashString(hash, actor.GetComponent<FMaterialComponent>().AssetPath);
                    hash = HashAssetFileFingerprint(hash, actor.GetComponent<FMaterialComponent>().AssetPath);
                }
            } else if (actor.HasComponent<FMeshComponent>()) {
                const auto& mc = actor.GetComponent<FMeshComponent>();
                uint8_t mob = static_cast<uint8_t>(mc.Mobility);
                hash = HashBytes(hash, &mob, sizeof(mob));
                hash = HashBytes(hash, &mc.LightmapResolution, sizeof(mc.LightmapResolution));
                hash = HashBytes(hash, &mc.bCastShadows, sizeof(mc.bCastShadows));
                hash = HashString(hash, mc.MeshType);
                hash = HashBytes(hash, &mc.MeshSize, sizeof(mc.MeshSize));
                hash = HashBytes(hash, &mc.MeshWidth, sizeof(mc.MeshWidth));
                hash = HashBytes(hash, &mc.MeshHeight, sizeof(mc.MeshHeight));
                hash = HashBytes(hash, &mc.MeshDepth, sizeof(mc.MeshDepth));
                hash = HashBytes(hash, &mc.MeshRadius, sizeof(mc.MeshRadius));
                hash = HashString(hash, actor.GetName());
                if (actor.HasComponent<FMaterialComponent>()) {
                    hash = HashString(hash, actor.GetComponent<FMaterialComponent>().AssetPath);
                    hash = HashAssetFileFingerprint(hash, actor.GetComponent<FMaterialComponent>().AssetPath);
                }
            }

            auto hashLight = [&](ELightMobility Mobility, bool bEnabled, const auto& LightPayload) {
                if (!bEnabled || !IsLightmassBakeLight(Mobility))
                    return;
                uint8_t mob = static_cast<uint8_t>(Mobility);
                hash = HashBytes(hash, &mob, sizeof(mob));
                hash = HashBytes(hash, &LightPayload, sizeof(LightPayload));
            };

            if (actor.HasComponent<FDirectionalLightComponent>()) {
                const auto& c = actor.GetComponent<FDirectionalLightComponent>();
                hashLight(c.Mobility, c.bEnabled, c.Light);
            }
            if (actor.HasComponent<FPointLightComponent>()) {
                const auto& c = actor.GetComponent<FPointLightComponent>();
                hashLight(c.Mobility, c.bEnabled, c.Light);
            }
            if (actor.HasComponent<FSpotLightComponent>()) {
                const auto& c = actor.GetComponent<FSpotLightComponent>();
                hashLight(c.Mobility, c.bEnabled, c.Light);
            }

            if (actor.HasComponent<FSkyboxComponent>()) {
                const auto& sky = actor.GetComponent<FSkyboxComponent>();
                uint8_t en = sky.bEnabled ? 1 : 0;
                uint8_t useHdr = sky.bUseHDREnvironmentMap ? 1 : 0;
                hash = HashBytes(hash, &en, sizeof(en));
                hash = HashBytes(hash, &useHdr, sizeof(useHdr));
                hash = HashVec3(hash, sky.SkyZenithColor);
                hash = HashVec3(hash, sky.HorizonColor);
                hash = HashVec3(hash, sky.GroundColor);
                hash = HashBytes(hash, &sky.EnvironmentIntensity, sizeof(sky.EnvironmentIntensity));
                hash = HashString(hash, sky.HDREnvironmentMapPath);
                hash = HashAssetFileFingerprint(hash, sky.HDREnvironmentMapPath);
            }
        }

        return hash;
    }

    bool FLightmass::ValidateMap(const std::string& InMapPath, std::string& OutMessage) {
        if (!fs::exists(InMapPath)) {
            OutMessage = "Map file not found";
            return false;
        }

        fs::path mapPath = InMapPath;
        fs::path contentRoot = mapPath.parent_path();
        if (contentRoot.filename() == "Maps")
            contentRoot = contentRoot.parent_path();
        UAssetManager::SetContentRoot(contentRoot.string());

        auto world = UWorld::Create();
        FMapSerializer serializer(world);
        if (!serializer.Deserialize(InMapPath)) {
            OutMessage = "Failed to deserialize map";
            return false;
        }

        FLightmassSettings settings;
        std::string lightmapVirtual;
        bool bStaticLighting = false;
        uint64_t storedHash = 0;
        if (FWorldSettingsComponent* ws = FindWorldSettings(*world)) {
            bStaticLighting = ws->bStaticLighting;
            storedHash = ws->LightmapBakeHash;
            lightmapVirtual = ws->LightmapAssetPath;
            ApplyWorldSettings(*ws, settings, lightmapVirtual);
        }

        uint32_t staticMeshes = 0;
        uint32_t bakeLights = 0;
        uint32_t badCharts = 0;
        for (auto& actorRef : world->GetAllActors()) {
            if (!actorRef)
                continue;
            if (actorRef->HasComponent<FStaticMeshComponent>()) {
                auto& smc = actorRef->GetComponent<FStaticMeshComponent>();
                if (smc.Mobility == EComponentMobility::Static) {
                    ++staticMeshes;
                    if (smc.LightmapIndex < 0 || !std::isfinite(smc.LightmapScale.x) ||
                        !std::isfinite(smc.LightmapBias.x))
                        ++badCharts;
                }
            }
            if (actorRef->HasComponent<FMeshComponent>()) {
                auto& mc = actorRef->GetComponent<FMeshComponent>();
                if (mc.Mobility == EComponentMobility::Static) {
                    ++staticMeshes;
                    if (mc.LightmapIndex < 0 || !std::isfinite(mc.LightmapScale.x) ||
                        !std::isfinite(mc.LightmapBias.x))
                        ++badCharts;
                }
            }
            if (actorRef->HasComponent<FDirectionalLightComponent>()) {
                auto& c = actorRef->GetComponent<FDirectionalLightComponent>();
                if (c.bEnabled && IsLightmassBakeLight(c.Mobility))
                    ++bakeLights;
            }
            if (actorRef->HasComponent<FPointLightComponent>()) {
                auto& c = actorRef->GetComponent<FPointLightComponent>();
                if (c.bEnabled && IsLightmassBakeLight(c.Mobility))
                    ++bakeLights;
            }
            if (actorRef->HasComponent<FSpotLightComponent>()) {
                auto& c = actorRef->GetComponent<FSpotLightComponent>();
                if (c.bEnabled && IsLightmassBakeLight(c.Mobility))
                    ++bakeLights;
            }
        }

        std::ostringstream ss;
        ss << "Static meshes: " << staticMeshes << ", Bake lights (Static+Stationary): " << bakeLights;

        if (staticMeshes == 0) {
            OutMessage = ss.str() + " — FAIL: no static geometry";
            return false;
        }

        if (bStaticLighting) {
            if (lightmapVirtual.empty()) {
                OutMessage = ss.str() + " — FAIL: StaticLighting enabled but LightmapAsset empty";
                return false;
            }
            fs::path lightmapAbs = ResolveLightmapAbsolutePath(InMapPath, lightmapVirtual);
            if (!fs::exists(lightmapAbs)) {
                OutMessage = ss.str() + " — FAIL: missing lightmap " + lightmapAbs.string();
                return false;
            }
            FLightmapAsset atlas;
            if (!atlas.LoadFromFile(lightmapAbs.string()) || atlas.GetWidth() == 0 || atlas.GetHeight() == 0) {
                OutMessage = ss.str() + " — FAIL: invalid lightmap asset";
                return false;
            }
            uint64_t expected = ComputeBakeInputHash(*world, settings);
            if (storedHash == 0) {
                OutMessage = ss.str() + " — FAIL: LightmapBakeHash is 0";
                return false;
            }
            if (storedHash != expected) {
                ss << " — FAIL: LightmapBakeHash stale (stored=" << std::hex << storedHash
                   << " expected=" << expected << std::dec << ")";
                OutMessage = ss.str();
                return false;
            }
            if (atlas.GetContentHash() == 0) {
                OutMessage = ss.str() + " — FAIL: .llightmap ContentHash is 0";
                return false;
            }
            if (atlas.GetContentHash() != expected) {
                ss << " — FAIL: .llightmap ContentHash stale";
                OutMessage = ss.str();
                return false;
            }
            ss << ", Atlas " << atlas.GetWidth() << "x" << atlas.GetHeight();
            if (badCharts > 0) {
                OutMessage = ss.str() + " — FAIL: " + std::to_string(badCharts) + " meshes missing chart metadata";
                return false;
            }
            ss << " — OK";
        }

        OutMessage = ss.str();
        return true;
    }

    FLightmassBakeResult FLightmass::BakeMap(const std::string& InMapPath, const FLightmassSettings& InSettings,
                                               bool bForce) {
        FLightmassBakeResult result;
        LogLM("Loading map...");

        if (!fs::exists(InMapPath)) {
            result.Message = "Map not found: " + InMapPath;
            return result;
        }

        auto world = UWorld::Create();
        FMapSerializer serializer(world);
        if (!serializer.Deserialize(InMapPath)) {
            result.Message = "Failed to load map";
            return result;
        }

        FLightmassSettings settings = InSettings;
        std::string lightmapVirtual = "/Game/Lightmaps/" + fs::path(InMapPath).stem().string() + ".llightmap";

        if (FWorldSettingsComponent* ws = FindWorldSettings(*world)) {
            ApplyWorldSettings(*ws, settings, lightmapVirtual);
        }

        PersistWorldLightmapUVs(*world);

        uint64_t bakeHash = ComputeBakeInputHash(*world, settings);
        result.BakeHash = bakeHash;

        fs::path lightmapAbs = ResolveLightmapAbsolutePath(InMapPath, lightmapVirtual);

        if (!bForce && fs::exists(lightmapAbs)) {
            FLightmapAsset existing;
            bool bAtlasMatch = existing.LoadFromFile(lightmapAbs.string()) && existing.GetContentHash() != 0 &&
                               existing.GetContentHash() == bakeHash;
            bool bWorldMatch = false;
            if (FWorldSettingsComponent* ws = FindWorldSettings(*world))
                bWorldMatch = ws->LightmapBakeHash != 0 && ws->LightmapBakeHash == bakeHash;
            if (bAtlasMatch && bWorldMatch) {
                result.bSuccess = true;
                result.Message = "Lightmap cache valid";
                result.LightmapPath = lightmapAbs.string();
                LogLM("Build skipped (cache valid).");
                return result;
            }
        }

        FLightBakerScene scene;
        std::vector<FLightmapChart> charts;
        struct FBakeInstanceRef {
            AActor* Actor = nullptr;
            bool bIsStaticMeshAsset = false;
        };
        std::vector<FBakeInstanceRef> instances;

        for (auto& actorRef : world->GetAllActors()) {
            if (!actorRef)
                continue;

            if (actorRef->HasComponent<FStaticMeshComponent>()) {
                auto& smc = actorRef->GetComponent<FStaticMeshComponent>();
                if (smc.Mobility != EComponentMobility::Static || !smc.StaticMesh)
                    continue;
                FLightmapChart chart;
                chart.InstanceIndex = static_cast<uint32_t>(charts.size());
                chart.Resolution = smc.LightmapResolution > 0 ? smc.LightmapResolution : settings.LightmapResolution;
                charts.push_back(chart);
                instances.push_back({actorRef.get(), true});
            } else if (actorRef->HasComponent<FMeshComponent>()) {
                auto& mc = actorRef->GetComponent<FMeshComponent>();
                if (mc.Mobility != EComponentMobility::Static)
                    continue;
                FLightmapChart chart;
                chart.InstanceIndex = static_cast<uint32_t>(charts.size());
                chart.Resolution = mc.LightmapResolution > 0 ? mc.LightmapResolution : settings.LightmapResolution;
                charts.push_back(chart);
                instances.push_back({actorRef.get(), false});
            }

            if (actorRef->HasComponent<FDirectionalLightComponent>()) {
                auto& c = actorRef->GetComponent<FDirectionalLightComponent>();
                if (c.bEnabled && IsLightmassBakeLight(c.Mobility))
                    scene.DirectionalLights.push_back({c.Light, DoesLightmassBakeDirect(c.Mobility)});
            }
            if (actorRef->HasComponent<FPointLightComponent>()) {
                auto& c = actorRef->GetComponent<FPointLightComponent>();
                if (c.bEnabled && IsLightmassBakeLight(c.Mobility)) {
                    FPointLight l = c.Light;
                    l.Position = actorRef->GetComponent<FTransformComponent>().Translation;
                    scene.PointLights.push_back({l, DoesLightmassBakeDirect(c.Mobility)});
                }
            }
            if (actorRef->HasComponent<FSpotLightComponent>()) {
                auto& c = actorRef->GetComponent<FSpotLightComponent>();
                if (c.bEnabled && IsLightmassBakeLight(c.Mobility)) {
                    FSpotLight l = c.Light;
                    l.Position = actorRef->GetComponent<FTransformComponent>().Translation;
                    scene.SpotLights.push_back({l, DoesLightmassBakeDirect(c.Mobility)});
                }
            }
        }

        result.StaticMeshCount = static_cast<uint32_t>(instances.size());
        result.StaticLightCount = static_cast<uint32_t>(scene.DirectionalLights.size() + scene.PointLights.size() +
                                                       scene.SpotLights.size());
        LogLM("Static meshes: " + std::to_string(result.StaticMeshCount));
        LogLM("Bake lights (Static+Stationary): " + std::to_string(result.StaticLightCount));

        if (instances.empty()) {
            result.Message = "No static geometry to bake";
            return result;
        }

        LogLM("Building lightmap UVs...");
        LogLM("Building atlas...");
        FLightmapBuilder::PackCharts(charts, scene.AtlasWidth, scene.AtlasHeight,
                                       static_cast<uint32_t>(std::max(0.0f, settings.TexelPadding)));
        scene.Charts = charts;
        result.AtlasWidth = scene.AtlasWidth;
        result.AtlasHeight = scene.AtlasHeight;

        for (size_t i = 0; i < instances.size(); ++i) {
            AActor* actor = instances[i].Actor;
            auto& transform = actor->GetComponent<FTransformComponent>();
            glm::mat4 M = transform.GetTransform();
            FBakeMaterialSample mat = ResolveBakeMaterial(*actor);
            uint32_t chartIndex = static_cast<uint32_t>(i);

            if (instances[i].bIsStaticMeshAsset) {
                auto& smc = actor->GetComponent<FStaticMeshComponent>();
                AppendStaticMesh(scene, chartIndex, *smc.StaticMesh, M, mat, smc.bCastShadows);
                smc.LightmapIndex = static_cast<int32_t>(i);
                smc.LightmapScale = charts[i].Scale;
                smc.LightmapBias = charts[i].Bias;
                smc.LightmapAssetPath = lightmapVirtual;
            } else {
                auto& mc = actor->GetComponent<FMeshComponent>();
                AppendProceduralMesh(scene, chartIndex, mc, M, mat);
                mc.LightmapIndex = static_cast<int32_t>(i);
                mc.LightmapScale = charts[i].Scale;
                mc.LightmapBias = charts[i].Bias;
                mc.LightmapAssetPath = lightmapVirtual;
            }
        }

        LogLM("Baking direct lighting...");
        LogLM("Baking indirect lighting...");

        FLightBakerSettings bakerSettings;
        bakerSettings.NumIndirectBounces = settings.NumIndirectBounces;
        bakerSettings.SamplesPerTexel = settings.SamplesPerTexel;
        bakerSettings.IndirectIntensity = settings.IndirectIntensity;
        bakerSettings.bAmbientOcclusion = settings.bAmbientOcclusion;
        bakerSettings.AOIntensity = settings.AOIntensity;
        bakerSettings.AORadius = settings.AORadius * std::max(settings.WorldScale, 1e-4f);
        bakerSettings.Seed = settings.DeterministicSeed;

        if (const FSkyboxComponent* sky = FindSkybox(*world)) {
            FillBakeEnvironment(scene, *sky);
            if (scene.Environment.bEnabled) {
                if (scene.Environment.HDRWidth > 0) {
                    LogLM("Bake environment: HDR " + std::to_string(scene.Environment.HDRWidth) + "x" +
                          std::to_string(scene.Environment.HDRHeight) +
                          " intensity=" + std::to_string(scene.Environment.Intensity));
                } else {
                    LogLM("Bake environment: atmosphere intensity=" + std::to_string(scene.Environment.Intensity));
                }
            }
        }

        std::vector<float> pixels;
        FLightBaker::Bake(scene, bakerSettings, pixels);

        LogLM("Writing lightmaps...");
        fs::create_directories(lightmapAbs.parent_path());
        FLightmapAsset asset;
        asset.Allocate(scene.AtlasWidth, scene.AtlasHeight);
        asset.GetPixelsRGBA32F() = std::move(pixels);
        asset.SetContentHash(bakeHash);
        if (!asset.SaveToFile(lightmapAbs.string())) {
            result.Message = "Failed to write lightmap";
            return result;
        }

        FWorldSettingsComponent* ws = FindWorldSettings(*world);
        if (!ws) {
            AActor* env = world->FindActorByName("Environment Skybox");
            if (!env)
                env = world->SpawnActor("Environment Skybox");
            ws = &env->AddComponent<FWorldSettingsComponent>();
        }
        ws->bStaticLighting = true;
        ws->LightmapAssetPath = lightmapVirtual;
        ws->LightmapBakeHash = bakeHash;
        ws->LightmapResolution = settings.LightmapResolution;
        ws->NumIndirectBounces = settings.NumIndirectBounces;
        ws->SamplesPerTexel = settings.SamplesPerTexel;
        ws->IndirectIntensity = settings.IndirectIntensity;
        ws->bAmbientOcclusion = settings.bAmbientOcclusion;
        ws->AOIntensity = settings.AOIntensity;
        ws->AORadius = settings.AORadius;
        ws->TexelPadding = settings.TexelPadding;
        ws->WorldScale = settings.WorldScale;
        ws->LightingBuildQuality = settings.LightingBuildQuality;

        if (!serializer.Serialize(InMapPath)) {
            LogLM("Warning: failed to write lightmap metadata back to map");
        }

        result.bSuccess = true;
        result.LightmapPath = lightmapAbs.string();
        result.Message = "Build completed successfully.";
        LogLM("Build completed successfully.");
        return result;
    }

} // namespace Leon
