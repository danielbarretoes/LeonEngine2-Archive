#include "Assets/UStaticMesh.hpp"
#include "Assets/FMeshSimplifier.hpp"
#include "Core/FLog.hpp"
#include "Assets/UAssetManager.hpp"
#include "RHI/IRenderDriver.hpp"

#include <algorithm>
#include <cstdint>
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

    TRef<FVertexArray> UStaticMesh::UploadGeometry(const std::vector<FStaticMeshVertex>& InVertices,
                                                   const std::vector<uint32_t>& InIndices) const {
        if (InVertices.empty() || InIndices.empty() || !FRenderDriverRegistry::GetActiveDriver())
            return nullptr;
        auto va = FVertexArray::Create();
        if (!va)
            return nullptr;
        TRef<FVertexBuffer> vertexBuffer =
            FVertexBuffer::Create(reinterpret_cast<const float*>(InVertices.data()),
                                  static_cast<uint32_t>(InVertices.size() * sizeof(FStaticMeshVertex)));
        vertexBuffer->SetLayout(MakeCanonicalMeshLayout());
        va->AddVertexBuffer(vertexBuffer);
        TRef<FIndexBuffer> indexBuffer =
            FIndexBuffer::Create(InIndices.data(), static_cast<uint32_t>(InIndices.size()));
        va->SetIndexBuffer(indexBuffer);
        return va;
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

        VertexArray = UploadGeometry(Vertices, Indices);
        for (auto& lod : ReducedLODs)
            lod.VertexArray = UploadGeometry(lod.Vertices, lod.Indices);
    }

    TRef<FVertexArray> UStaticMesh::GetLODVertexArray(uint32_t InLOD) const {
        if (InLOD == 0)
            return VertexArray;
        const uint32_t reduced = InLOD - 1;
        if (reduced >= ReducedLODs.size())
            return VertexArray;
        return ReducedLODs[reduced].VertexArray ? ReducedLODs[reduced].VertexArray : VertexArray;
    }

    const std::vector<FStaticSubmesh>& UStaticMesh::GetLODSubmeshes(uint32_t InLOD) const {
        if (InLOD == 0 || InLOD - 1 >= ReducedLODs.size())
            return Submeshes;
        return ReducedLODs[InLOD - 1].Submeshes;
    }

    uint32_t UStaticMesh::GetLODIndexCount(uint32_t InLOD) const {
        if (InLOD == 0 || InLOD - 1 >= ReducedLODs.size())
            return static_cast<uint32_t>(Indices.size());
        return static_cast<uint32_t>(ReducedLODs[InLOD - 1].Indices.size());
    }

    void UStaticMesh::BuildAutomaticLODs(const FLODSettings& InSettings) {
        ReducedLODs.clear();
        LODSettingsHash = InSettings.Hash();
        if (!InSettings.bGenerateLODs || Vertices.empty() || Indices.size() < 3)
            return;

        const uint32_t srcTris = static_cast<uint32_t>(Indices.size() / 3);
        for (size_t i = 1; i < InSettings.Levels.size() && ReducedLODs.size() + 1 < kMaxStaticMeshLODCount; ++i) {
            const FLODLevel& level = InSettings.Levels[i];
            FStaticMeshLOD lod;
            if (!FMeshSimplifier::Simplify(Vertices, Indices, Submeshes, level.TriangleRatio,
                                           InSettings.MinTriangleCount, lod))
                break;
            if (lod.Indices.size() / 3 >= srcTris)
                break;
            if (!ReducedLODs.empty() && lod.Indices.size() >= ReducedLODs.back().Indices.size())
                break;
            ReducedLODs.push_back(std::move(lod));
        }
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

    bool UStaticMesh::WriteLODBlob(std::ostream& InFile, const FStaticMeshLOD& InLOD) const {
        InFile.write(reinterpret_cast<const char*>(&InLOD.TriangleRatio), sizeof(InLOD.TriangleRatio));
        const uint32_t vertexCount = static_cast<uint32_t>(InLOD.Vertices.size());
        const uint32_t indexCount = static_cast<uint32_t>(InLOD.Indices.size());
        const uint32_t submeshCount = static_cast<uint32_t>(InLOD.Submeshes.size());
        InFile.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
        InFile.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
        InFile.write(reinterpret_cast<const char*>(&submeshCount), sizeof(submeshCount));
        for (const auto& sm : InLOD.Submeshes) {
            const uint32_t nameLen = static_cast<uint32_t>(sm.Name.length());
            InFile.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
            if (nameLen > 0)
                InFile.write(sm.Name.data(), nameLen);
            InFile.write(reinterpret_cast<const char*>(&sm.IndexOffset), sizeof(sm.IndexOffset));
            InFile.write(reinterpret_cast<const char*>(&sm.IndexCount), sizeof(sm.IndexCount));
            InFile.write(reinterpret_cast<const char*>(&sm.VertexOffset), sizeof(sm.VertexOffset));
            InFile.write(reinterpret_cast<const char*>(&sm.VertexCount), sizeof(sm.VertexCount));
            InFile.write(reinterpret_cast<const char*>(&sm.MaterialSlotIndex), sizeof(sm.MaterialSlotIndex));
            InFile.write(reinterpret_cast<const char*>(&sm.LocalTransform), sizeof(glm::mat4));
            InFile.write(reinterpret_cast<const char*>(&sm.BoundsMin), sizeof(glm::vec3));
            InFile.write(reinterpret_cast<const char*>(&sm.BoundsMax), sizeof(glm::vec3));
        }
        if (!InLOD.Vertices.empty()) {
            InFile.write(reinterpret_cast<const char*>(InLOD.Vertices.data()),
                         static_cast<std::streamsize>(InLOD.Vertices.size() * sizeof(FStaticMeshVertex)));
        }
        if (!InLOD.Indices.empty()) {
            InFile.write(reinterpret_cast<const char*>(InLOD.Indices.data()),
                         static_cast<std::streamsize>(InLOD.Indices.size() * sizeof(uint32_t)));
        }
        return InFile.good();
    }

    bool UStaticMesh::ReadLODBlob(std::istream& InFile, FStaticMeshLOD& OutLOD) {
        OutLOD = {};
        InFile.read(reinterpret_cast<char*>(&OutLOD.TriangleRatio), sizeof(OutLOD.TriangleRatio));
        uint32_t vertexCount = 0, indexCount = 0, submeshCount = 0;
        InFile.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
        InFile.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
        InFile.read(reinterpret_cast<char*>(&submeshCount), sizeof(submeshCount));
        if (!InFile.good())
            return false;
        OutLOD.Submeshes.resize(submeshCount);
        for (uint32_t i = 0; i < submeshCount; ++i) {
            uint32_t nameLen = 0;
            InFile.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
            if (nameLen > 0) {
                OutLOD.Submeshes[i].Name.resize(nameLen);
                InFile.read(&OutLOD.Submeshes[i].Name[0], nameLen);
            }
            InFile.read(reinterpret_cast<char*>(&OutLOD.Submeshes[i].IndexOffset),
                        sizeof(OutLOD.Submeshes[i].IndexOffset));
            InFile.read(reinterpret_cast<char*>(&OutLOD.Submeshes[i].IndexCount),
                        sizeof(OutLOD.Submeshes[i].IndexCount));
            InFile.read(reinterpret_cast<char*>(&OutLOD.Submeshes[i].VertexOffset),
                        sizeof(OutLOD.Submeshes[i].VertexOffset));
            InFile.read(reinterpret_cast<char*>(&OutLOD.Submeshes[i].VertexCount),
                        sizeof(OutLOD.Submeshes[i].VertexCount));
            InFile.read(reinterpret_cast<char*>(&OutLOD.Submeshes[i].MaterialSlotIndex),
                        sizeof(OutLOD.Submeshes[i].MaterialSlotIndex));
            InFile.read(reinterpret_cast<char*>(&OutLOD.Submeshes[i].LocalTransform), sizeof(glm::mat4));
            InFile.read(reinterpret_cast<char*>(&OutLOD.Submeshes[i].BoundsMin), sizeof(glm::vec3));
            InFile.read(reinterpret_cast<char*>(&OutLOD.Submeshes[i].BoundsMax), sizeof(glm::vec3));
        }
        OutLOD.Vertices.resize(vertexCount);
        if (vertexCount > 0) {
            InFile.read(reinterpret_cast<char*>(OutLOD.Vertices.data()),
                        static_cast<std::streamsize>(vertexCount * sizeof(FStaticMeshVertex)));
        }
        OutLOD.Indices.resize(indexCount);
        if (indexCount > 0) {
            InFile.read(reinterpret_cast<char*>(OutLOD.Indices.data()),
                        static_cast<std::streamsize>(indexCount * sizeof(uint32_t)));
        }
        return InFile.good();
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

        uint8_t uniqueUV = bHasUniqueLightmapUV ? 1 : 0;
        file.write(reinterpret_cast<const char*>(&uniqueUV), sizeof(uniqueUV));

        const uint32_t lodHash = LODSettingsHash != 0 ? LODSettingsHash : FLODSettings::Default().Hash();
        const uint32_t reducedCount = static_cast<uint32_t>(ReducedLODs.size());
        file.write(reinterpret_cast<const char*>(&lodHash), sizeof(lodHash));
        file.write(reinterpret_cast<const char*>(&reducedCount), sizeof(reducedCount));
        for (const auto& lod : ReducedLODs) {
            if (!WriteLODBlob(file, lod))
                return false;
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
        if (version < LMESH_VERSION_V1 || version > LMESH_VERSION) {
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
        if (!file.good() || vertexCount > kMaxCookedMeshVertices || indexCount > kMaxCookedMeshVertices * 3u ||
            submeshCount > 100000 || materialSlotCount > 100000) {
            LE_CORE_ERROR("UStaticMesh: Invalid counts in \"{0}\"", InFilePath);
            return false;
        }

        file.read(reinterpret_cast<char*>(&BoundsMin), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&BoundsMax), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&SphereCenter), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&SphereRadius), sizeof(float));

        // Submeshes
        Submeshes.resize(submeshCount);
        for (uint32_t i = 0; i < submeshCount; ++i) {
            uint32_t nameLen = 0;
            file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
            if (!file.good() || nameLen > kMaxCookedNameLen) {
                LE_CORE_ERROR("UStaticMesh: Invalid submesh name length in \"{0}\"", InFilePath);
                return false;
            }
            if (nameLen > 0) {
                Submeshes[i].Name.resize(nameLen);
                file.read(&Submeshes[i].Name[0], nameLen);
            }
            file.read(reinterpret_cast<char*>(&Submeshes[i].IndexOffset), sizeof(Submeshes[i].IndexOffset));
            file.read(reinterpret_cast<char*>(&Submeshes[i].IndexCount), sizeof(Submeshes[i].IndexCount));
            file.read(reinterpret_cast<char*>(&Submeshes[i].VertexOffset), sizeof(Submeshes[i].VertexOffset));
            file.read(reinterpret_cast<char*>(&Submeshes[i].VertexCount), sizeof(Submeshes[i].VertexCount));
            file.read(reinterpret_cast<char*>(&Submeshes[i].MaterialSlotIndex), sizeof(Submeshes[i].MaterialSlotIndex));
            file.read(reinterpret_cast<char*>(&Submeshes[i].LocalTransform), sizeof(glm::mat4));
            file.read(reinterpret_cast<char*>(&Submeshes[i].BoundsMin), sizeof(glm::vec3));
            file.read(reinterpret_cast<char*>(&Submeshes[i].BoundsMax), sizeof(glm::vec3));
        }

        // Material Slots
        MaterialSlots.resize(materialSlotCount);
        for (uint32_t i = 0; i < materialSlotCount; ++i) {
            uint32_t slotNameLen = 0;
            file.read(reinterpret_cast<char*>(&slotNameLen), sizeof(slotNameLen));
            if (!file.good() || slotNameLen > kMaxCookedNameLen)
                return false;
            if (slotNameLen > 0) {
                MaterialSlots[i].SlotName.resize(slotNameLen);
                file.read(&MaterialSlots[i].SlotName[0], slotNameLen);
            }
            uint32_t matPathLen = 0;
            file.read(reinterpret_cast<char*>(&matPathLen), sizeof(matPathLen));
            if (!file.good() || matPathLen > kMaxCookedNameLen)
                return false;
            if (matPathLen > 0) {
                MaterialSlots[i].DefaultMaterialPath.resize(matPathLen);
                file.read(&MaterialSlots[i].DefaultMaterialPath[0], matPathLen);
            }
        }

        // Vertices
        Vertices.resize(vertexCount);
        if (vertexCount > 0) {
            auto convertTB = [](FStaticMeshVertex& dst, const glm::vec3& t, const glm::vec3& b) {
                dst.Tangent = PackTangent(t, dst.Normal, b);
            };
            if (version == LMESH_VERSION_V1) {
                std::vector<FStaticMeshVertexV1> disk(vertexCount);
                file.read(reinterpret_cast<char*>(disk.data()),
                          static_cast<std::streamsize>(vertexCount * sizeof(FStaticMeshVertexV1)));
                for (uint32_t i = 0; i < vertexCount; ++i) {
                    Vertices[i].Position = disk[i].Position;
                    Vertices[i].Normal = disk[i].Normal;
                    Vertices[i].TexCoord = disk[i].TexCoord;
                    Vertices[i].Color = disk[i].Color;
                    Vertices[i].LightmapUV = disk[i].TexCoord;
                    convertTB(Vertices[i], disk[i].Tangent, disk[i].Bitangent);
                }
            } else if (version == LMESH_VERSION_V2) {
                std::vector<FStaticMeshVertexV2> disk(vertexCount);
                file.read(reinterpret_cast<char*>(disk.data()),
                          static_cast<std::streamsize>(vertexCount * sizeof(FStaticMeshVertexV2)));
                for (uint32_t i = 0; i < vertexCount; ++i) {
                    Vertices[i].Position = disk[i].Position;
                    Vertices[i].Normal = disk[i].Normal;
                    Vertices[i].TexCoord = disk[i].TexCoord;
                    Vertices[i].Color = disk[i].Color;
                    Vertices[i].LightmapUV = disk[i].LightmapUV;
                    convertTB(Vertices[i], disk[i].Tangent, disk[i].Bitangent);
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

        bHasUniqueLightmapUV = false;
        if (version >= LMESH_VERSION_V4) {
            uint8_t uniqueUV = 0;
            file.read(reinterpret_cast<char*>(&uniqueUV), sizeof(uniqueUV));
            bHasUniqueLightmapUV = uniqueUV != 0;
        }

        ReducedLODs.clear();
        LODSettingsHash = 0;
        if (version >= 5) {
            uint32_t lodHash = 0;
            uint32_t reducedCount = 0;
            file.read(reinterpret_cast<char*>(&lodHash), sizeof(lodHash));
            file.read(reinterpret_cast<char*>(&reducedCount), sizeof(reducedCount));
            LODSettingsHash = lodHash;
            reducedCount = std::min(reducedCount, kMaxStaticMeshLODCount);
            ReducedLODs.resize(reducedCount);
            for (uint32_t i = 0; i < reducedCount; ++i) {
                if (!ReadLODBlob(file, ReducedLODs[i])) {
                    ReducedLODs.clear();
                    break;
                }
            }
        }
        if (ReducedLODs.empty() || LODSettingsHash != FLODSettings::Default().Hash())
            BuildAutomaticLODs();

        AssetPath = InFilePath;
        return file.good();
    }

} // namespace Leon
