#include "Assets/FEngineBuiltins.hpp"
#include "Assets/FLightmapUV.hpp"
#include "Assets/FMeshImporter.hpp"
#include "Assets/UAssetManager.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectPaths.hpp"
#include "Engine/FMaterialSerializer.hpp"
#include "Renderer/FMaterial.hpp"
#include "Renderer/FVertexLayout.hpp"
#include "RHI/FTexture.hpp"
#include "RHI/IRenderDriver.hpp"

#include <cmath>
#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

namespace fs = std::filesystem;

namespace Leon {

    namespace {

        void PushVert(std::vector<FCanonicalMeshVertex>& Out, const glm::vec3& InP, const glm::vec3& InN,
                      const glm::vec2& InUV, const glm::vec3& InT, const glm::vec3& InB,
                      const glm::vec2& InLM = glm::vec2(0.02f)) {
            FCanonicalMeshVertex v;
            v.Position = InP;
            v.Normal = InN;
            v.TexCoord = InUV;
            v.Tangent = PackTangent(InT, InN, InB);
            v.Color = glm::vec3(1.0f);
            v.LightmapUV = InLM;
            Out.push_back(v);
        }

        void PushQuadVerts(std::vector<FCanonicalMeshVertex>& Out, const glm::vec3 InP[4], const glm::vec2 InUV[4],
                           const glm::vec3& InN, const glm::vec3& InT, const glm::vec3& InB, int InLmFace = -1) {
            for (int i = 0; i < 4; ++i) {
                glm::vec2 lm = InUV[i] * 0.96f + glm::vec2(0.02f);
                if (InLmFace >= 0)
                    lm = PackLightmapCell(InUV[i], InLmFace % 3, InLmFace / 3, 3, 2);
                PushVert(Out, InP[i], InN, InUV[i], InT, InB, lm);
            }
        }

        void AppendQuadIndices(std::vector<uint32_t>& Out, uint32_t Base) {
            Out.push_back(Base + 0);
            Out.push_back(Base + 1);
            Out.push_back(Base + 2);
            Out.push_back(Base + 2);
            Out.push_back(Base + 3);
            Out.push_back(Base + 0);
        }

        TRef<UStaticMesh> FinalizePrimitiveMesh(const std::string& InName, std::vector<FCanonicalMeshVertex>&& InVerts,
                                                std::vector<uint32_t>&& InIndices) {
            auto mesh = UStaticMesh::Create(InName);
            mesh->GetVertices() = std::move(InVerts);
            mesh->GetIndices() = std::move(InIndices);
            FStaticSubmesh sub;
            sub.Name = InName;
            sub.IndexCount = static_cast<uint32_t>(mesh->GetIndices().size());
            sub.VertexCount = static_cast<uint32_t>(mesh->GetVertices().size());
            sub.MaterialSlotIndex = 0;
            mesh->GetSubmeshes().push_back(sub);
            mesh->GetMaterialSlots().push_back({"Default", FEngineBuiltins::kWorldGridMaterial, nullptr});
            mesh->CalculateBounds();
            // Build* helpers write PackLightmapCell / island UVs into LightmapUV.
            mesh->SetHasUniqueLightmapUV(true);
            return mesh;
        }

        TRef<UStaticMesh> BuildBoxMesh(const std::string& InName, float InSize) {
            const float h = InSize * 0.5f;
            const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            std::vector<FCanonicalMeshVertex> verts;
            std::vector<uint32_t> indices;
            verts.reserve(24);
            indices.reserve(36);

            auto face = [&](const glm::vec3 p[4], const glm::vec3& n, const glm::vec3& t, const glm::vec3& b, int lm) {
                uint32_t base = static_cast<uint32_t>(verts.size());
                PushQuadVerts(verts, p, uv, n, t, b, lm);
                AppendQuadIndices(indices, base);
            };

            {
                glm::vec3 p[4] = {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}};
                face(p, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, 0);
            }
            {
                glm::vec3 p[4] = {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}};
                face(p, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, 1);
            }
            {
                glm::vec3 p[4] = {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}};
                face(p, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, 2);
            }
            {
                glm::vec3 p[4] = {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}};
                face(p, {0, -1, 0}, {1, 0, 0}, {0, 0, 1}, 3);
            }
            {
                glm::vec3 p[4] = {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}};
                face(p, {-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, 4);
            }
            {
                glm::vec3 p[4] = {{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}};
                face(p, {1, 0, 0}, {0, 0, -1}, {0, 1, 0}, 5);
            }

            return FinalizePrimitiveMesh(InName, std::move(verts), std::move(indices));
        }

        TRef<UStaticMesh> BuildSphereMesh(const std::string& InName, float InRadius, unsigned InSegments,
                                          unsigned InRings) {
            constexpr float PI = 3.14159265358979323846f;
            std::vector<FCanonicalMeshVertex> verts;
            std::vector<uint32_t> indices;
            for (unsigned y = 0; y <= InRings; ++y) {
                float v = static_cast<float>(y) / static_cast<float>(InRings);
                float phi = v * PI;
                for (unsigned x = 0; x <= InSegments; ++x) {
                    float u = static_cast<float>(x) / static_cast<float>(InSegments);
                    float theta = u * (PI * 2.0f);
                    float nx = std::cos(theta) * std::sin(phi);
                    float ny = std::cos(phi);
                    float nz = std::sin(theta) * std::sin(phi);
                    glm::vec3 n(nx, ny, nz);
                    glm::vec3 t(-std::sin(theta), 0.0f, std::cos(theta));
                    if (glm::dot(t, t) < 1e-8f)
                        t = glm::vec3(1, 0, 0);
                    else
                        t = glm::normalize(t);
                    glm::vec3 b = glm::normalize(glm::cross(n, t));
                    PushVert(verts, n * InRadius, n, {u, v}, t, b, {u * 0.96f + 0.02f, v * 0.96f + 0.02f});
                }
            }
            for (unsigned y = 0; y < InRings; ++y) {
                for (unsigned x = 0; x < InSegments; ++x) {
                    uint32_t i0 = y * (InSegments + 1) + x;
                    uint32_t i1 = (y + 1) * (InSegments + 1) + x;
                    uint32_t i2 = (y + 1) * (InSegments + 1) + (x + 1);
                    uint32_t i3 = y * (InSegments + 1) + (x + 1);
                    indices.push_back(i0);
                    indices.push_back(i2);
                    indices.push_back(i1);
                    indices.push_back(i0);
                    indices.push_back(i3);
                    indices.push_back(i2);
                }
            }
            return FinalizePrimitiveMesh(InName, std::move(verts), std::move(indices));
        }

        TRef<UStaticMesh> BuildCylinderMesh(const std::string& InName, float InRadius, float InHeight,
                                            unsigned InSegments) {
            constexpr float PI = 3.14159265358979323846f;
            const float h = InHeight * 0.5f;
            std::vector<FCanonicalMeshVertex> verts;
            std::vector<uint32_t> indices;

            uint32_t sideBase = 0;
            for (unsigned x = 0; x <= InSegments; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(InSegments);
                float theta = u * (PI * 2.0f);
                float c = std::cos(theta), s = std::sin(theta);
                glm::vec3 n(c, 0, s);
                glm::vec3 t(-s, 0, c);
                glm::vec3 b = glm::normalize(glm::cross(n, t));
                PushVert(verts, {InRadius * c, -h, InRadius * s}, n, {u, 0}, t, b);
                PushVert(verts, {InRadius * c, h, InRadius * s}, n, {u, 1}, t, b);
            }
            for (unsigned x = 0; x < InSegments; ++x) {
                uint32_t b0 = sideBase + x * 2, t0 = b0 + 1, b1 = sideBase + (x + 1) * 2, t1 = b1 + 1;
                indices.push_back(b0);
                indices.push_back(t0);
                indices.push_back(t1);
                indices.push_back(b0);
                indices.push_back(t1);
                indices.push_back(b1);
            }

            auto addCap = [&](float y, const glm::vec3& n, bool bTop) {
                uint32_t center = static_cast<uint32_t>(verts.size());
                glm::vec3 t(1, 0, 0);
                glm::vec3 b = glm::normalize(glm::cross(n, t));
                PushVert(verts, {0, y, 0}, n, {0.5f, 0.5f}, t, b);
                uint32_t ring = static_cast<uint32_t>(verts.size());
                for (unsigned x = 0; x <= InSegments; ++x) {
                    float u = static_cast<float>(x) / static_cast<float>(InSegments);
                    float theta = u * (PI * 2.0f);
                    float c = std::cos(theta), s = std::sin(theta);
                    PushVert(verts, {InRadius * c, y, InRadius * s}, n,
                             {0.5f + 0.5f * c, 0.5f + (bTop ? 0.5f : -0.5f) * s}, t, b);
                }
                for (unsigned x = 0; x < InSegments; ++x) {
                    if (bTop) {
                        indices.push_back(center);
                        indices.push_back(ring + x + 1);
                        indices.push_back(ring + x);
                    } else {
                        indices.push_back(center);
                        indices.push_back(ring + x);
                        indices.push_back(ring + x + 1);
                    }
                }
            };
            addCap(h, {0, 1, 0}, true);
            addCap(-h, {0, -1, 0}, false);

            return FinalizePrimitiveMesh(InName, std::move(verts), std::move(indices));
        }

        TRef<UStaticMesh> BuildPlaneMesh(const std::string& InName, float InWidth, float InDepth) {
            const float hx = InWidth * 0.5f, hz = InDepth * 0.5f;
            std::vector<FCanonicalMeshVertex> verts;
            std::vector<uint32_t> indices;
            glm::vec3 n(0, 1, 0), t(1, 0, 0), b(0, 0, -1);
            PushVert(verts, {-hx, 0, -hz}, n, {0, InDepth / 4}, t, b, {0.02f, 0.02f});
            PushVert(verts, {-hx, 0, hz}, n, {0, 0}, t, b, {0.02f, 0.98f});
            PushVert(verts, {hx, 0, hz}, n, {InWidth / 4, 0}, t, b, {0.98f, 0.98f});
            PushVert(verts, {hx, 0, -hz}, n, {InWidth / 4, InDepth / 4}, t, b, {0.98f, 0.02f});
            indices = {0, 1, 2, 0, 2, 3};
            return FinalizePrimitiveMesh(InName, std::move(verts), std::move(indices));
        }

        void EnsureWorldGridMaterialFile(const fs::path& InPath) {
            if (fs::exists(InPath))
                return;
            fs::create_directories(InPath.parent_path());
            FMaterial mat("M_WorldGrid");
            mat.SetAssetPath(FEngineBuiltins::kWorldGridMaterial);
            mat.SetAlbedoColor(glm::vec3(1.0f));
            mat.SetMetallic(0.0f);
            mat.SetRoughness(0.65f);
            mat.SetAO(1.0f);
            mat.SetUVTiling({2.0f, 2.0f});
            mat.SetTexturePath(0, FEngineBuiltins::kWorldGridTexture);
            mat.SetUseAlbedoMap(true);
            if (FMaterialSerializer::Serialize(InPath.string(), mat))
                LE_CORE_INFO("FEngineBuiltins: Wrote {}", InPath.string());
        }

        void EnsureMeshFile(const std::string& InVirtualPath, const TRef<UStaticMesh>& InMesh, bool bForceWrite = false) {
            if (!InMesh)
                return;
            const std::string disk = UAssetManager::ResolveVirtualPath(InVirtualPath);
            if (disk.empty())
                return;
            if (!bForceWrite && fs::exists(disk))
                return;
            fs::create_directories(fs::path(disk).parent_path());
            InMesh->SetAssetPath(InVirtualPath);
            if (InMesh->SaveToFile(disk))
                LE_CORE_INFO("FEngineBuiltins: Wrote {}", disk);
        }

        /** Prefer authored FBX next to the .lmesh (Blender export); else keep procedural mesh. */
        TRef<UStaticMesh> ResolvePrimitiveMesh(const std::string& InVirtualPath, const std::string& InName,
                                               const TRef<UStaticMesh>& InProceduralFallback) {
            const std::string lmeshDisk = UAssetManager::ResolveVirtualPath(InVirtualPath);
            if (lmeshDisk.empty())
                return InProceduralFallback;

            const fs::path fbxPath = fs::path(lmeshDisk).replace_extension(".fbx");
            if (fs::exists(fbxPath)) {
                FMeshImportSettings settings;
                settings.bSplitStaticMeshes = false;
                settings.bExtractMaterials = false;
                settings.bGenerateTangents = true;
                settings.bGenerateNormalsIfMissing = true;

                FMeshImportResult result;
                if (FMeshImporter::ImportFBX(fbxPath.string(), settings, result) && result.StaticMesh) {
                    auto mesh = result.StaticMesh;
                    mesh->GetMaterialSlots().clear();
                    mesh->GetMaterialSlots().push_back({"Default", FEngineBuiltins::kWorldGridMaterial, nullptr});
                    if (mesh->GetSubmeshes().empty()) {
                        FStaticSubmesh sub;
                        sub.Name = InName;
                        sub.IndexCount = static_cast<uint32_t>(mesh->GetIndices().size());
                        sub.VertexCount = static_cast<uint32_t>(mesh->GetVertices().size());
                        sub.MaterialSlotIndex = 0;
                        mesh->GetSubmeshes().push_back(sub);
                    }
                    // FBX import leaves LightmapUV at (0,0) — bake needs unique UV1.
                    FLightmapUV::GenerateBoxPackedLightmapUVs(*mesh);
                    mesh->CalculateBounds();
                    EnsureMeshFile(InVirtualPath, mesh, true);
                    LE_CORE_INFO("FEngineBuiltins: Imported primitive '{}' from {} (with lightmap UVs)", InName,
                                 fbxPath.string());
                    return mesh;
                }
                for (const auto& err : result.Errors)
                    LE_CORE_WARN("FEngineBuiltins: FBX import '{}': {}", fbxPath.string(), err);
            }

            EnsureMeshFile(InVirtualPath, InProceduralFallback, false);
            return InProceduralFallback;
        }

        void RegisterMesh(const std::string& InVirtualPath, const TRef<UStaticMesh>& InMesh) {
            if (!InMesh)
                return;
            InMesh->SetAssetPath(InVirtualPath);
            if (FRenderDriverRegistry::GetActiveDriver())
                InMesh->CreateGPUResources();
            UAssetManager::AddStaticMesh(InVirtualPath, InMesh);
            const std::string resolved = UAssetManager::ResolveVirtualPath(InVirtualPath);
            if (!resolved.empty() && resolved != InVirtualPath)
                UAssetManager::AddStaticMesh(resolved, InMesh);
        }

        void RegisterWorldGrid(const fs::path& InEngineRes) {
            auto checker = UAssetManager::GetDefaultCheckerTexture();
            if (checker) {
                UAssetManager::AddTexture2D(FEngineBuiltins::kWorldGridTexture, checker);
                UAssetManager::AddTexture2D("/" + std::string(FEngineBuiltins::kWorldGridTexture), checker);
            }

            EnsureWorldGridMaterialFile(InEngineRes / "Materials" / "M_WorldGrid.lmat");

            auto mat = FMaterial::Create("M_WorldGrid");
            mat->SetAssetPath(FEngineBuiltins::kWorldGridMaterial);
            mat->SetAlbedoColor(glm::vec3(1.0f));
            mat->SetMetallic(0.0f);
            mat->SetRoughness(0.65f);
            mat->SetAO(1.0f);
            mat->SetUVTiling({2.0f, 2.0f});
            mat->SetTexturePath(0, FEngineBuiltins::kWorldGridTexture);
            mat->SetUseAlbedoMap(true);
            if (checker)
                mat->SetAlbedoMap(checker);

            UAssetManager::AddMaterial(FEngineBuiltins::kWorldGridMaterial, mat);
            UAssetManager::AddMaterial("/" + std::string(FEngineBuiltins::kWorldGridMaterial), mat);
            UAssetManager::AddMaterial("Engine/Materials/M_WorldGrid", mat);
        }

    } // namespace

    void FEngineBuiltins::EnsureAndRegister() {
        const fs::path engineRes = fs::path(FProjectPaths::EngineContentDir());
        RegisterWorldGrid(engineRes);

        auto cube = ResolvePrimitiveMesh(kMeshCube, "Cube", BuildBoxMesh("Cube", 1.0f));
        auto sphere = ResolvePrimitiveMesh(kMeshSphere, "Sphere", BuildSphereMesh("Sphere", 0.5f, 32, 16));
        auto cylinder = ResolvePrimitiveMesh(kMeshCylinder, "Cylinder", BuildCylinderMesh("Cylinder", 0.5f, 1.0f, 32));
        auto plane = ResolvePrimitiveMesh(kMeshPlane, "Plane", BuildPlaneMesh("Plane", 2.0f, 2.0f));

        RegisterMesh(kMeshCube, cube);
        RegisterMesh(kMeshSphere, sphere);
        RegisterMesh(kMeshCylinder, cylinder);
        RegisterMesh(kMeshPlane, plane);

        LE_CORE_INFO("FEngineBuiltins: Registered Engine primitive meshes and M_WorldGrid");
    }

} // namespace Leon
