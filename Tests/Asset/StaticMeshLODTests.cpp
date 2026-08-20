#include <doctest/doctest.h>
#include "Assets/FLODSettings.hpp"
#include "Assets/FMeshSimplifier.hpp"
#include "Assets/UStaticMesh.hpp"
#include "GPU/HeadlessGLContext.hpp"

#include <cmath>
#include <algorithm>
#include <filesystem>
#include <limits>

using namespace Leon;
namespace fs = std::filesystem;

namespace {
    TRef<UStaticMesh> MakeGridMesh(int InSegs) {
        auto mesh = UStaticMesh::Create("LODGrid");
        const int segs = std::max(InSegs, 1);
        auto& verts = mesh->GetVertices();
        auto& indices = mesh->GetIndices();
        verts.reserve(static_cast<size_t>(segs + 1) * static_cast<size_t>(segs + 1));
        for (int z = 0; z <= segs; ++z) {
            for (int x = 0; x <= segs; ++x) {
                FStaticMeshVertex v;
                v.Position = {static_cast<float>(x), 0.0f, static_cast<float>(z)};
                v.Normal = {0.0f, 1.0f, 0.0f};
                v.TexCoord = {static_cast<float>(x) / segs, static_cast<float>(z) / segs};
                v.LightmapUV = v.TexCoord;
                v.Color = {1.0f, 1.0f, 1.0f};
                v.Tangent = {1.0f, 0.0f, 0.0f, 1.0f};
                verts.push_back(v);
            }
        }
        for (int z = 0; z < segs; ++z) {
            for (int x = 0; x < segs; ++x) {
                const uint32_t i0 = static_cast<uint32_t>(z * (segs + 1) + x);
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + static_cast<uint32_t>(segs + 1);
                const uint32_t i3 = i2 + 1;
                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);
                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }
        FStaticSubmesh sub;
        sub.Name = "Grid";
        sub.IndexOffset = 0;
        sub.IndexCount = static_cast<uint32_t>(indices.size());
        sub.VertexOffset = 0;
        sub.VertexCount = static_cast<uint32_t>(verts.size());
        sub.MaterialSlotIndex = 0;
        mesh->GetSubmeshes().push_back(sub);
        FStaticMaterialSlot slot;
        slot.SlotName = "M_Grid";
        slot.DefaultMaterialPath = "Materials/M_DefaultPBR.lmat";
        mesh->GetMaterialSlots().push_back(slot);
        mesh->CalculateBounds();
        return mesh;
    }

    bool LodGeometryValid(const UStaticMesh& InMesh, uint32_t InLOD) {
        const auto& submeshes = InMesh.GetLODSubmeshes(InLOD);
        const uint32_t indexCount = InMesh.GetLODIndexCount(InLOD);
        if (indexCount % 3 != 0)
            return false;
        if (InLOD == 0) {
            for (uint32_t idx : InMesh.GetIndices()) {
                if (idx >= InMesh.GetVertices().size())
                    return false;
            }
            for (const auto& v : InMesh.GetVertices()) {
                if (!std::isfinite(v.Position.x) || !std::isfinite(v.Normal.x) || !std::isfinite(v.TexCoord.x) ||
                    !std::isfinite(v.Tangent.x))
                    return false;
            }
        }
        for (const auto& sm : submeshes) {
            if (sm.IndexCount % 3 != 0)
                return false;
            if (sm.MaterialSlotIndex >= InMesh.GetMaterialSlots().size() && !InMesh.GetMaterialSlots().empty())
                return false;
        }
        return InMesh.GetSphereRadius() >= 0.0f &&
               glm::all(glm::lessThanEqual(InMesh.GetBoundsMin(), InMesh.GetBoundsMax()));
    }
} // namespace

TEST_SUITE("Automatic static mesh LOD") {

    TEST_CASE("Screen-height LOD thresholds match configured levels") {
        const FLODSettings settings = FLODSettings::Default();
        CHECK(SelectStaticMeshLOD(0.80f, 0, 5, settings) == 0);
        CHECK(SelectStaticMeshLOD(0.40f, 0, 5, settings) == 1);
        CHECK(SelectStaticMeshLOD(0.15f, 1, 5, settings) == 2);
        CHECK(SelectStaticMeshLOD(0.05f, 2, 5, settings) == 3);
        CHECK(SelectStaticMeshLOD(0.01f, 3, 5, settings) == 4);
    }

    TEST_CASE("Hysteresis holds LOD when hovering a threshold") {
        const FLODSettings settings = FLODSettings::Default();
        const float leaveLod0 = settings.Levels[0].MinScreenHeight * (1.0f - settings.Hysteresis);
        CHECK(SelectStaticMeshLOD(leaveLod0 + 0.001f, 0, 5, settings) == 0);
        CHECK(SelectStaticMeshLOD(leaveLod0 - 0.02f, 0, 5, settings) == 1);
        const float enterLod0 = settings.Levels[0].MinScreenHeight * (1.0f + settings.Hysteresis);
        CHECK(SelectStaticMeshLOD(enterLod0 - 0.001f, 1, 5, settings) == 1);
        CHECK(SelectStaticMeshLOD(enterLod0 + 0.02f, 1, 5, settings) == 0);
    }

    TEST_CASE("Projected screen height grows when closer or larger") {
        const glm::vec3 cam{0.0f, 0.0f, 0.0f};
        const glm::vec3 center{0.0f, 0.0f, -10.0f};
        const float nearH = ComputeProjectedScreenHeight(center, 1.0f, cam, 45.0f);
        const float farH = ComputeProjectedScreenHeight(glm::vec3(0.0f, 0.0f, -40.0f), 1.0f, cam, 45.0f);
        const float bigH = ComputeProjectedScreenHeight(center, 4.0f, cam, 45.0f);
        CHECK(nearH > farH);
        CHECK(bigH > nearH);
    }

    TEST_CASE("Simplifier reduces triangles and keeps material slots") {
        auto mesh = MakeGridMesh(24);
        const uint32_t srcTris = mesh->GetSourceTriangleCount();
        REQUIRE(srcTris > 100);
        FStaticMeshLOD lod;
        REQUIRE(
            FMeshSimplifier::Simplify(mesh->GetVertices(), mesh->GetIndices(), mesh->GetSubmeshes(), 0.30f, 16, lod));
        CHECK(lod.Indices.size() % 3 == 0);
        CHECK(lod.Indices.size() / 3 < srcTris);
        CHECK(lod.Submeshes.size() == mesh->GetSubmeshes().size());
        CHECK(lod.Submeshes[0].MaterialSlotIndex == 0);
        for (uint32_t idx : lod.Indices) {
            CHECK(idx < lod.Vertices.size());
        }
        for (const auto& v : lod.Vertices) {
            CHECK(std::isfinite(v.Position.x));
            CHECK(std::isfinite(v.Normal.x));
            CHECK(std::isfinite(v.Tangent.x));
        }
    }

    TEST_CASE("Tiny and single-triangle meshes fail generation gracefully") {
        auto tri = UStaticMesh::Create("Tiny");
        tri->GetVertices() = {{}, {}, {}};
        tri->GetVertices()[0].Position = {0, 0, 0};
        tri->GetVertices()[1].Position = {1, 0, 0};
        tri->GetVertices()[2].Position = {0, 1, 0};
        tri->GetIndices() = {0, 1, 2};
        FStaticSubmesh sm;
        sm.IndexCount = 3;
        sm.VertexCount = 3;
        tri->GetSubmeshes().push_back(sm);
        tri->CalculateBounds();
        tri->BuildAutomaticLODs();
        CHECK(tri->GetLODCount() == 1);

        FStaticMeshLOD lod;
        CHECK_FALSE(
            FMeshSimplifier::Simplify(tri->GetVertices(), tri->GetIndices(), tri->GetSubmeshes(), 0.05f, 16, lod));
    }

    TEST_CASE("BuildAutomaticLODs produces strictly fewer triangles per level") {
        auto mesh = MakeGridMesh(32);
        mesh->BuildAutomaticLODs();
        REQUIRE(mesh->GetLODCount() >= 2);
        CHECK(LodGeometryValid(*mesh, 0));
        uint32_t prev = mesh->GetLODTriangleCount(0);
        for (uint32_t i = 1; i < mesh->GetLODCount(); ++i) {
            CHECK(LodGeometryValid(*mesh, i));
            const uint32_t tris = mesh->GetLODTriangleCount(i);
            CHECK(tris < prev);
            CHECK(tris > 0);
            prev = tris;
        }
    }

    TEST_CASE("Generated LODs roundtrip through .lmesh v5 cache") {
        auto mesh = MakeGridMesh(20);
        mesh->BuildAutomaticLODs();
        REQUIRE(mesh->GetLODCount() >= 2);
        const uint32_t lodCount = mesh->GetLODCount();
        const uint32_t lod1Tris = mesh->GetLODTriangleCount(1);
        const std::string path = "build/temp_lod_cache.lmesh";
        REQUIRE(mesh->SaveToFile(path));

        auto loaded = UStaticMesh::Create("LoadedLOD");
        REQUIRE(loaded->LoadFromFile(path));
        CHECK(loaded->GetLODCount() == lodCount);
        CHECK(loaded->GetLODTriangleCount(1) == lod1Tris);
        CHECK(loaded->GetLODSettingsHash() == FLODSettings::Default().Hash());
        CHECK(loaded->GetMaterialSlots()[0].SlotName == "M_Grid");
        fs::remove(path);
    }

    TEST_CASE("GPU resources exist for each generated LOD") {
        auto& gl = TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());
        auto mesh = MakeGridMesh(16);
        mesh->BuildAutomaticLODs();
        mesh->CreateGPUResources();
        REQUIRE(mesh->GetVertexArray() != nullptr);
        for (uint32_t i = 0; i < mesh->GetLODCount(); ++i)
            CHECK(mesh->GetLODVertexArray(i) != nullptr);
    }
}
