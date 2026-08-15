#include <doctest/doctest.h>
#include "renderer/StaticMesh.hpp"
#include "asset/MeshImporter.hpp"

#include <filesystem>

namespace fs = std::filesystem;
using namespace Leon;

TEST_SUITE("StaticMesh & .lmesh Binary Format Tests") {

    TEST_CASE("StaticMesh - Binary Serialization (.lmesh) Roundtrip") {
        auto mesh = FStaticMesh::Create("TestCube");

        // Populate mock vertices
        FStaticMeshVertex v0{ glm::vec3(-1.0f), glm::vec3(0,1,0), glm::vec2(0,0), glm::vec3(1,0,0), glm::vec3(0,0,1), glm::vec3(1.0f) };
        FStaticMeshVertex v1{ glm::vec3(1.0f),  glm::vec3(0,1,0), glm::vec2(1,1), glm::vec3(1,0,0), glm::vec3(0,0,1), glm::vec3(1.0f) };
        FStaticMeshVertex v2{ glm::vec3(1.0f, -1.0f, 1.0f), glm::vec3(0,1,0), glm::vec2(1,0), glm::vec3(1,0,0), glm::vec3(0,0,1), glm::vec3(1.0f) };
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

        auto loadedMesh = FStaticMesh::Create("LoadedCube");
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
        CHECK(sizeof(FStaticMeshVertex) == 68); // 3 + 3 + 2 + 3 + 3 + 3 = 17 floats = 68 bytes
    }
}
