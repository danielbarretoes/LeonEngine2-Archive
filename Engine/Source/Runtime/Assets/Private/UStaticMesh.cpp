#include "Assets/UStaticMesh.hpp"
#include "Core/FLog.hpp"
#include "Assets/UAssetManager.hpp"
#include "RHI/IRenderDriver.hpp"

#include <algorithm>
#include <fstream>
#include <limits>

namespace Leon {

    UStaticMesh::UStaticMesh(const std::string& InName) : Name(InName), UUID(FUUID::FromPath(InName)) {}

    TRef<UStaticMesh> UStaticMesh::Create(const std::string& InName) {
        return MakeRef<UStaticMesh>(InName);
    }

    TRef<FMaterialInstance>
    ResolveStaticSubmeshMaterial(UStaticMesh& InMesh, const FStaticSubmesh& InSubmesh,
                                 const std::vector<TRef<FMaterialInstance>>& InMaterialOverrides) {
        if (InSubmesh.MaterialSlotIndex < InMaterialOverrides.size() &&
            InMaterialOverrides[InSubmesh.MaterialSlotIndex]) {
            return InMaterialOverrides[InSubmesh.MaterialSlotIndex];
        }

        if (InSubmesh.MaterialSlotIndex < InMesh.GetMaterialSlots().size()) {
            auto& slot = InMesh.GetMaterialSlots()[InSubmesh.MaterialSlotIndex];
            if (!slot.MaterialInstance && !slot.DefaultMaterialPath.empty()) {
                slot.MaterialInstance = UAssetManager::GetMaterialInstance(slot.DefaultMaterialPath);
            }
            if (slot.MaterialInstance)
                return slot.MaterialInstance;
        }

        return UAssetManager::GetDefaultMaterial()->CreateInstance();
    }

    void UStaticMesh::CreateGPUResources() {
        if (Vertices.empty() || Indices.empty()) {
            LE_CORE_WARN("UStaticMesh: Cannot create GPU resources for empty mesh \"{0}\"", Name);
            return;
        }
        if (!FRenderDriverRegistry::GetActiveDriver()) {
            LE_CORE_WARN("UStaticMesh: No RenderDriver — skipping GPU upload for \"{0}\"", Name);
            return;
        }

        VertexArray = FVertexArray::Create();
        if (!VertexArray)
            return;

        TRef<FVertexBuffer> vertexBuffer =
            FVertexBuffer::Create(reinterpret_cast<const float*>(Vertices.data()),
                                  static_cast<uint32_t>(Vertices.size() * sizeof(FStaticMeshVertex)));

        vertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"},
                                 {EShaderDataType::Float3, "aNormal"},
                                 {EShaderDataType::Float2, "aTexCoord"},
                                 {EShaderDataType::Float2, "aLightmapUV"},
                                 {EShaderDataType::Float3, "aTangent"},
                                 {EShaderDataType::Float3, "aBitangent"},
                                 {EShaderDataType::Float3, "aColor"}});
        VertexArray->AddVertexBuffer(vertexBuffer);

        TRef<FIndexBuffer> indexBuffer =
            FIndexBuffer::Create(Indices.data(), static_cast<uint32_t>(Indices.size()));
        VertexArray->SetIndexBuffer(indexBuffer);
    }

    void UStaticMesh::CalculateBounds() {
        if (Vertices.empty()) {
            BoundsMin = glm::vec3(0.0f);
            BoundsMax = glm::vec3(0.0f);
            SphereCenter = glm::vec3(0.0f);
            SphereRadius = 0.0f;
            return;
        }

        glm::vec3 minP(std::numeric_limits<float>::max());
        glm::vec3 maxP(-std::numeric_limits<float>::max());

        for (const auto& v : Vertices) {
            minP = glm::min(minP, v.Position);
            maxP = glm::max(maxP, v.Position);
        }

        BoundsMin = minP;
        BoundsMax = maxP;
        SphereCenter = (minP + maxP) * 0.5f;

        float maxDistSq = 0.0f;
        for (const auto& v : Vertices) {
            float distSq = glm::dot(v.Position - SphereCenter, v.Position - SphereCenter);
            maxDistSq = std::max(maxDistSq, distSq);
        }
        SphereRadius = std::sqrt(maxDistSq);

        // Update bounds for each submesh
        for (auto& submesh : Submeshes) {
            if (submesh.VertexCount == 0)
                continue;
            glm::vec3 sMin(std::numeric_limits<float>::max());
            glm::vec3 sMax(-std::numeric_limits<float>::max());

            uint32_t endV =
                std::min(static_cast<uint32_t>(Vertices.size()), submesh.VertexOffset + submesh.VertexCount);
            for (uint32_t i = submesh.VertexOffset; i < endV; ++i) {
                sMin = glm::min(sMin, Vertices[i].Position);
                sMax = glm::max(sMax, Vertices[i].Position);
            }
            submesh.BoundsMin = sMin;
            submesh.BoundsMax = sMax;
        }
    }

    bool UStaticMesh::SaveToFile(const std::string& InFilePath) const {
        std::ofstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("UStaticMesh: Failed to open \"{0}\" for writing", InFilePath);
            return false;
        }

        // Header
        uint32_t magic = LMESH_MAGIC;
        uint32_t version = LMESH_VERSION;
        uint64_t uuidHigh = UUID.High;
        uint64_t uuidLow = UUID.Low;
        uint32_t vertexCount = static_cast<uint32_t>(Vertices.size());
        uint32_t indexCount = static_cast<uint32_t>(Indices.size());
        uint32_t submeshCount = static_cast<uint32_t>(Submeshes.size());
        uint32_t materialSlotCount = static_cast<uint32_t>(MaterialSlots.size());

        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        file.write(reinterpret_cast<const char*>(&uuidHigh), sizeof(uuidHigh));
        file.write(reinterpret_cast<const char*>(&uuidLow), sizeof(uuidLow));
        file.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
        file.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
        file.write(reinterpret_cast<const char*>(&submeshCount), sizeof(submeshCount));
        file.write(reinterpret_cast<const char*>(&materialSlotCount), sizeof(materialSlotCount));

        file.write(reinterpret_cast<const char*>(&BoundsMin), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&BoundsMax), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&SphereCenter), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&SphereRadius), sizeof(float));

        // Submeshes
        for (const auto& sm : Submeshes) {
            uint32_t nameLen = static_cast<uint32_t>(sm.Name.length());
            file.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
            if (nameLen > 0) {
                file.write(sm.Name.data(), nameLen);
            }
            file.write(reinterpret_cast<const char*>(&sm.IndexOffset), sizeof(sm.IndexOffset));
            file.write(reinterpret_cast<const char*>(&sm.IndexCount), sizeof(sm.IndexCount));
            file.write(reinterpret_cast<const char*>(&sm.VertexOffset), sizeof(sm.VertexOffset));
            file.write(reinterpret_cast<const char*>(&sm.VertexCount), sizeof(sm.VertexCount));
            file.write(reinterpret_cast<const char*>(&sm.MaterialSlotIndex), sizeof(sm.MaterialSlotIndex));
            file.write(reinterpret_cast<const char*>(&sm.LocalTransform), sizeof(glm::mat4));
            file.write(reinterpret_cast<const char*>(&sm.BoundsMin), sizeof(glm::vec3));
            file.write(reinterpret_cast<const char*>(&sm.BoundsMax), sizeof(glm::vec3));
        }

        // Material Slots
        for (const auto& slot : MaterialSlots) {
            uint32_t slotNameLen = static_cast<uint32_t>(slot.SlotName.length());
            file.write(reinterpret_cast<const char*>(&slotNameLen), sizeof(slotNameLen));
            if (slotNameLen > 0) {
                file.write(slot.SlotName.data(), slotNameLen);
            }
            uint32_t matPathLen = static_cast<uint32_t>(slot.DefaultMaterialPath.length());
            file.write(reinterpret_cast<const char*>(&matPathLen), sizeof(matPathLen));
            if (matPathLen > 0) {
                file.write(slot.DefaultMaterialPath.data(), matPathLen);
            }
        }

        // Vertices
        if (!Vertices.empty()) {
            file.write(reinterpret_cast<const char*>(Vertices.data()), Vertices.size() * sizeof(FStaticMeshVertex));
        }

        // Indices
        if (!Indices.empty()) {
            file.write(reinterpret_cast<const char*>(Indices.data()), Indices.size() * sizeof(uint32_t));
        }

        return file.good();
    }

    bool UStaticMesh::LoadFromFile(const std::string& InFilePath) {
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("UStaticMesh: Failed to open \"{0}\" for reading", InFilePath);
            return false;
        }

        uint32_t magic = 0, version = 0;
        uint64_t uuidHigh = 0, uuidLow = 0;
        uint32_t vertexCount = 0, indexCount = 0, submeshCount = 0, materialSlotCount = 0;

        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (!file.good() || magic != LMESH_MAGIC) {
            LE_CORE_ERROR("UStaticMesh: Invalid magic in \"{0}\"", InFilePath);
            return false;
        }
        if (version != LMESH_VERSION && version != LMESH_VERSION_V1) {
            LE_CORE_ERROR("UStaticMesh: Unsupported version {0} in \"{1}\"", version, InFilePath);
            return false;
        }

        file.read(reinterpret_cast<char*>(&uuidHigh), sizeof(uuidHigh));
        file.read(reinterpret_cast<char*>(&uuidLow), sizeof(uuidLow));
        UUID = FUUID(uuidHigh, uuidLow);

        file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
        file.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
        file.read(reinterpret_cast<char*>(&submeshCount), sizeof(submeshCount));
        file.read(reinterpret_cast<char*>(&materialSlotCount), sizeof(materialSlotCount));

        file.read(reinterpret_cast<char*>(&BoundsMin), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&BoundsMax), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&SphereCenter), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&SphereRadius), sizeof(float));

        // Submeshes
        Submeshes.resize(submeshCount);
        for (uint32_t i = 0; i < submeshCount; ++i) {
            uint32_t nameLen = 0;
            file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
            if (nameLen > 0) {
                Submeshes[i].Name.resize(nameLen);
                file.read(&Submeshes[i].Name[0], nameLen);
            }
            file.read(reinterpret_cast<char*>(&Submeshes[i].IndexOffset), sizeof(Submeshes[i].IndexOffset));
            file.read(reinterpret_cast<char*>(&Submeshes[i].IndexCount), sizeof(Submeshes[i].IndexCount));
            file.read(reinterpret_cast<char*>(&Submeshes[i].VertexOffset), sizeof(Submeshes[i].VertexOffset));
            file.read(reinterpret_cast<char*>(&Submeshes[i].VertexCount), sizeof(Submeshes[i].VertexCount));
            file.read(reinterpret_cast<char*>(&Submeshes[i].MaterialSlotIndex),
                      sizeof(Submeshes[i].MaterialSlotIndex));
            file.read(reinterpret_cast<char*>(&Submeshes[i].LocalTransform), sizeof(glm::mat4));
            file.read(reinterpret_cast<char*>(&Submeshes[i].BoundsMin), sizeof(glm::vec3));
            file.read(reinterpret_cast<char*>(&Submeshes[i].BoundsMax), sizeof(glm::vec3));
        }

        // Material Slots
        MaterialSlots.resize(materialSlotCount);
        for (uint32_t i = 0; i < materialSlotCount; ++i) {
            uint32_t slotNameLen = 0;
            file.read(reinterpret_cast<char*>(&slotNameLen), sizeof(slotNameLen));
            if (slotNameLen > 0) {
                MaterialSlots[i].SlotName.resize(slotNameLen);
                file.read(&MaterialSlots[i].SlotName[0], slotNameLen);
            }
            uint32_t matPathLen = 0;
            file.read(reinterpret_cast<char*>(&matPathLen), sizeof(matPathLen));
            if (matPathLen > 0) {
                MaterialSlots[i].DefaultMaterialPath.resize(matPathLen);
                file.read(&MaterialSlots[i].DefaultMaterialPath[0], matPathLen);
            }
        }

        // Vertices
        Vertices.resize(vertexCount);
        if (vertexCount > 0) {
            if (version == LMESH_VERSION_V1) {
                std::vector<FStaticMeshVertexV1> legacy(vertexCount);
                file.read(reinterpret_cast<char*>(legacy.data()),
                          static_cast<std::streamsize>(vertexCount * sizeof(FStaticMeshVertexV1)));
                for (uint32_t i = 0; i < vertexCount; ++i) {
                    Vertices[i].Position = legacy[i].Position;
                    Vertices[i].Normal = legacy[i].Normal;
                    Vertices[i].TexCoord = legacy[i].TexCoord;
                    Vertices[i].LightmapUV = legacy[i].TexCoord; // fallback until Lightmass regenerates
                    Vertices[i].Tangent = legacy[i].Tangent;
                    Vertices[i].Bitangent = legacy[i].Bitangent;
                    Vertices[i].Color = legacy[i].Color;
                }
            } else {
                file.read(reinterpret_cast<char*>(Vertices.data()),
                          static_cast<std::streamsize>(vertexCount * sizeof(FStaticMeshVertex)));
            }
        }

        // Indices
        Indices.resize(indexCount);
        if (indexCount > 0) {
            file.read(reinterpret_cast<char*>(Indices.data()), indexCount * sizeof(uint32_t));
        }

        AssetPath = InFilePath;
        return file.good();
    }

} // namespace Leon
