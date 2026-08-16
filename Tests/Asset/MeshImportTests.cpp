#include <doctest/doctest.h>
#include "Assets/UStaticMesh.hpp"
#include "Assets/UAssetManager.hpp"
#include "Assets/FMeshImporter.hpp"
#include "Assets/FLightmapUV.hpp"
#include "GPU/HeadlessGLContext.hpp"
#include "Renderer/FVertexLayout.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace Leon;

TEST_SUITE("StaticMesh & .lmesh Binary Format Tests") {

    TEST_CASE("StaticMesh - Binary Serialization (.lmesh) Roundtrip") {
        auto mesh = UStaticMesh::Create("TestCube");

        // Populate mock vertices
        FStaticMeshVertex v0{ glm::vec3(-1.0f), glm::vec3(0,1,0), glm::vec2(0,0), glm::vec4(1,0,0,1), glm::vec3(1.0f), glm::vec2(0.0f) };
        FStaticMeshVertex v1{ glm::vec3(1.0f),  glm::vec3(0,1,0), glm::vec2(1,1), glm::vec4(1,0,0,1), glm::vec3(1.0f), glm::vec2(1.0f) };
        FStaticMeshVertex v2{ glm::vec3(1.0f, -1.0f, 1.0f), glm::vec3(0,1,0), glm::vec2(1,0), glm::vec4(1,0,0,1), glm::vec3(1.0f), glm::vec2(1.0f, 0.0f) };
        mesh->GetVertices() = { v0, v1, v2 };
        mesh->GetIndices() = { 0, 1, 2 };

        FStaticSubmesh sub;
        sub.Name = "CubePart_0";
        sub.IndexOffset = 0;
        sub.IndexCount = 3;
        sub.VertexOffset = 0;
        sub.MaterialSlotIndex = 0;
        mesh->GetSubmeshes().push_back(sub);

        FStaticMaterialSlot slot;
        slot.SlotName = "M_TestMaterial";
        slot.DefaultMaterialPath = "Materials/M_TestMaterial.lmat";
        mesh->GetMaterialSlots().push_back(slot);

        mesh->CalculateBounds();

        std::string tempPath = "build/temp_test.lmesh";
        CHECK(mesh->SaveToFile(tempPath));

        auto loadedMesh = UStaticMesh::Create("LoadedCube");
        CHECK(loadedMesh->LoadFromFile(tempPath));
        CHECK(loadedMesh->GetVertices().size() == 3);
        CHECK(loadedMesh->GetIndices().size() == 3);
        CHECK(loadedMesh->GetSubmeshes().size() == 1);
        CHECK(loadedMesh->GetSubmeshes()[0].Name == "CubePart_0");
        CHECK(loadedMesh->GetSubmeshes()[0].IndexCount == 3);
        CHECK(loadedMesh->GetMaterialSlots().size() == 1);
        CHECK(loadedMesh->GetMaterialSlots()[0].SlotName == "M_TestMaterial");
        CHECK(loadedMesh->GetSphereRadius() > 0.0f);

        fs::remove(tempPath);
    }

    TEST_CASE("StaticMesh - Vertex Layout Stride") {
        CHECK(sizeof(FStaticMeshVertex) == 68);
        CHECK(sizeof(FStaticMeshVertexV1) == 68);
    }

    TEST_CASE("ResolveStaticSubmeshMaterial prefers MaterialOverrides (planar/geometry shared path)") {
        // Material creation loads PBR_Lit — needs a valid GL context
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping.");
            return;
        }

        auto mesh = UStaticMesh::Create("OverrideResolve");
        FStaticSubmesh body;
        body.Name = "Body";
        body.MaterialSlotIndex = 0;
        FStaticSubmesh windows;
        windows.Name = "Windows";
        windows.MaterialSlotIndex = 1;
        mesh->GetSubmeshes() = {body, windows};

        FStaticMaterialSlot slot0;
        slot0.SlotName = "DefaultBody";
        mesh->GetMaterialSlots().push_back(slot0);

        auto redBody = UAssetManager::GetDefaultMaterial()->CreateInstance("RedBody");
        redBody->SetAlbedoColor(glm::vec3(0.85f, 0.05f, 0.05f));
        auto warmWindow = UAssetManager::GetDefaultMaterial()->CreateInstance("WarmWindow");
        warmWindow->SetAlbedoColor(glm::vec3(1.0f, 0.85f, 0.45f));
        warmWindow->SetEmissiveColor(glm::vec3(1.0f, 0.85f, 0.45f));
        warmWindow->SetEmissiveIntensity(6.0f);

        std::vector<TRef<FMaterialInstance>> overrides = {redBody, warmWindow};

        auto resolvedBody = ResolveStaticSubmeshMaterial(*mesh, body, overrides);
        auto resolvedWindows = ResolveStaticSubmeshMaterial(*mesh, windows, overrides);

        REQUIRE(resolvedBody != nullptr);
        REQUIRE(resolvedWindows != nullptr);
        CHECK(resolvedBody.get() == redBody.get());
        CHECK(resolvedWindows.get() == warmWindow.get());
        CHECK(resolvedBody->GetAlbedoColor().r > 0.8f);
        CHECK(resolvedWindows->GetEmissiveIntensity() == doctest::Approx(6.0f));

        // Empty overrides → engine default (must not return nullptr — planar pass would draw untextured)
        auto fallback = ResolveStaticSubmeshMaterial(*mesh, body, {});
        REQUIRE(fallback != nullptr);
        CHECK(fallback->GetAlbedoColor().r == doctest::Approx(1.0f));
    }

    TEST_CASE("GenerateLengyelTangents overwrites dummy tangents from UV0") {
        std::vector<FCanonicalMeshVertex> verts(4);
        verts[0].Position = {0, 0, 0};
        verts[0].Normal = {0, 1, 0};
        verts[0].TexCoord = {0, 0};
        verts[0].Tangent = {0, 0, 1, 1};
        verts[1].Position = {1, 0, 0};
        verts[1].Normal = {0, 1, 0};
        verts[1].TexCoord = {1, 0};
        verts[1].Tangent = {0, 0, 1, 1};
        verts[2].Position = {1, 0, 1};
        verts[2].Normal = {0, 1, 0};
        verts[2].TexCoord = {1, 1};
        verts[2].Tangent = {0, 0, 1, 1};
        verts[3].Position = {0, 0, 1};
        verts[3].Normal = {0, 1, 0};
        verts[3].TexCoord = {0, 1};
        verts[3].Tangent = {0, 0, 1, 1};
        std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

        GenerateLengyelTangents(verts, indices);
        CHECK(verts[0].Tangent.x == doctest::Approx(1.0f).epsilon(0.05f));
        CHECK(std::abs(verts[0].Tangent.z) < 0.1f);
        CHECK(verts[0].Tangent.x != doctest::Approx(0.0f));
    }

    TEST_CASE("HasLightmapUV is false until GenerateBoxPackedLightmapUVs") {
        auto mesh = UStaticMesh::Create("UV0Only");
        FStaticMeshVertex v{};
        v.TexCoord = {0.5f, 0.25f};
        v.LightmapUV = v.TexCoord;
        mesh->GetVertices() = {v, v, v};
        mesh->GetIndices() = {0, 1, 2};
        CHECK_FALSE(FLightmapUV::HasLightmapUV(*mesh));
        FLightmapUV::GenerateBoxPackedLightmapUVs(*mesh);
        CHECK(FLightmapUV::HasLightmapUV(*mesh));
    }

    TEST_CASE("GetMaterialInstance always creates a unique instance per .lmat path") {
        fs::path dir = fs::path("build") / "MatInstUnique";
        fs::create_directories(dir);
        fs::path lmat = dir / "M_Unique.lmat";
        {
            std::ofstream out(lmat);
            out << "Material:\n  Type: \"PBR_Lit\"\n  AlbedoColor: [0.2, 0.4, 0.8]\n  Metallic: 0.0\n  "
                   "Roughness: 0.5\n  AO: 1.0\n";
        }
        auto a = UAssetManager::GetMaterialInstance(lmat.string());
        auto b = UAssetManager::GetMaterialInstance(lmat.string());
        REQUIRE(a);
        REQUIRE(b);
        CHECK(a.get() != b.get());
        CHECK(a->GetParent().get() == b->GetParent().get());
    }
}
