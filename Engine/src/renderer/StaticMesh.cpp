#include "renderer/StaticMesh.hpp"
#include "core/Log.hpp"

#include <algorithm>
#include <fstream>
#include <limits>

namespace Leon {

    FStaticMesh::FStaticMesh(const std::string& InName) : m_Name(InName), m_UUID(FUUID::FromPath(InName)) {}

    TRef<FStaticMesh> FStaticMesh::Create(const std::string& InName) {
        return MakeRef<FStaticMesh>(InName);
    }

    void FStaticMesh::CreateGPUResources() {
        if (m_Vertices.empty() || m_Indices.empty()) {
            LE_CORE_WARN("FStaticMesh: Cannot create GPU resources for empty mesh \"{0}\"", m_Name);
            return;
        }

        m_VertexArray = FVertexArray::Create();

        TRef<FVertexBuffer> vertexBuffer =
            FVertexBuffer::Create(reinterpret_cast<const float*>(m_Vertices.data()),
                                  static_cast<uint32_t>(m_Vertices.size() * sizeof(FStaticMeshVertex)));

        vertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"},
                                 {EShaderDataType::Float3, "aNormal"},
                                 {EShaderDataType::Float2, "aTexCoord"},
                                 {EShaderDataType::Float3, "aTangent"},
                                 {EShaderDataType::Float3, "aBitangent"},
                                 {EShaderDataType::Float3, "aColor"}});
        m_VertexArray->AddVertexBuffer(vertexBuffer);

        TRef<FIndexBuffer> indexBuffer =
            FIndexBuffer::Create(m_Indices.data(), static_cast<uint32_t>(m_Indices.size()));
        m_VertexArray->SetIndexBuffer(indexBuffer);
    }

    void FStaticMesh::CalculateBounds() {
        if (m_Vertices.empty()) {
            m_BoundsMin = glm::vec3(0.0f);
            m_BoundsMax = glm::vec3(0.0f);
            m_SphereCenter = glm::vec3(0.0f);
            m_SphereRadius = 0.0f;
            return;
        }

        glm::vec3 minP(std::numeric_limits<float>::max());
        glm::vec3 maxP(-std::numeric_limits<float>::max());

        for (const auto& v : m_Vertices) {
            minP = glm::min(minP, v.Position);
            maxP = glm::max(maxP, v.Position);
        }

        m_BoundsMin = minP;
        m_BoundsMax = maxP;
        m_SphereCenter = (minP + maxP) * 0.5f;

        float maxDistSq = 0.0f;
        for (const auto& v : m_Vertices) {
            float distSq = glm::dot(v.Position - m_SphereCenter, v.Position - m_SphereCenter);
            maxDistSq = std::max(maxDistSq, distSq);
        }
        m_SphereRadius = std::sqrt(maxDistSq);

        // Update bounds for each submesh
        for (auto& submesh : m_Submeshes) {
            if (submesh.VertexCount == 0)
                continue;
            glm::vec3 sMin(std::numeric_limits<float>::max());
            glm::vec3 sMax(-std::numeric_limits<float>::max());

            uint32_t endV =
                std::min(static_cast<uint32_t>(m_Vertices.size()), submesh.VertexOffset + submesh.VertexCount);
            for (uint32_t i = submesh.VertexOffset; i < endV; ++i) {
                sMin = glm::min(sMin, m_Vertices[i].Position);
                sMax = glm::max(sMax, m_Vertices[i].Position);
            }
            submesh.BoundsMin = sMin;
            submesh.BoundsMax = sMax;
        }
    }

    bool FStaticMesh::SaveToFile(const std::string& InFilePath) const {
        std::ofstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("FStaticMesh: Failed to open \"{0}\" for writing", InFilePath);
            return false;
        }

        // Header
        uint32_t magic = LMESH_MAGIC;
        uint32_t version = LMESH_VERSION;
        uint64_t uuidHigh = m_UUID.High;
        uint64_t uuidLow = m_UUID.Low;
        uint32_t vertexCount = static_cast<uint32_t>(m_Vertices.size());
        uint32_t indexCount = static_cast<uint32_t>(m_Indices.size());
        uint32_t submeshCount = static_cast<uint32_t>(m_Submeshes.size());
        uint32_t materialSlotCount = static_cast<uint32_t>(m_MaterialSlots.size());

        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        file.write(reinterpret_cast<const char*>(&uuidHigh), sizeof(uuidHigh));
        file.write(reinterpret_cast<const char*>(&uuidLow), sizeof(uuidLow));
        file.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
        file.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
        file.write(reinterpret_cast<const char*>(&submeshCount), sizeof(submeshCount));
        file.write(reinterpret_cast<const char*>(&materialSlotCount), sizeof(materialSlotCount));

        file.write(reinterpret_cast<const char*>(&m_BoundsMin), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&m_BoundsMax), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&m_SphereCenter), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&m_SphereRadius), sizeof(float));

        // Submeshes
        for (const auto& sm : m_Submeshes) {
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
        for (const auto& slot : m_MaterialSlots) {
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
        if (!m_Vertices.empty()) {
            file.write(reinterpret_cast<const char*>(m_Vertices.data()), m_Vertices.size() * sizeof(FStaticMeshVertex));
        }

        // Indices
        if (!m_Indices.empty()) {
            file.write(reinterpret_cast<const char*>(m_Indices.data()), m_Indices.size() * sizeof(uint32_t));
        }

        return file.good();
    }

    bool FStaticMesh::LoadFromFile(const std::string& InFilePath) {
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("FStaticMesh: Failed to open \"{0}\" for reading", InFilePath);
            return false;
        }

        uint32_t magic = 0, version = 0;
        uint64_t uuidHigh = 0, uuidLow = 0;
        uint32_t vertexCount = 0, indexCount = 0, submeshCount = 0, materialSlotCount = 0;

        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (!file.good() || magic != LMESH_MAGIC) {
            LE_CORE_ERROR("FStaticMesh: Invalid magic in \"{0}\"", InFilePath);
            return false;
        }
        if (version != LMESH_VERSION) {
            LE_CORE_ERROR("FStaticMesh: Unsupported version {0} in \"{1}\"", version, InFilePath);
            return false;
        }

        file.read(reinterpret_cast<char*>(&uuidHigh), sizeof(uuidHigh));
        file.read(reinterpret_cast<char*>(&uuidLow), sizeof(uuidLow));
        m_UUID = FUUID(uuidHigh, uuidLow);

        file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
        file.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
        file.read(reinterpret_cast<char*>(&submeshCount), sizeof(submeshCount));
        file.read(reinterpret_cast<char*>(&materialSlotCount), sizeof(materialSlotCount));

        file.read(reinterpret_cast<char*>(&m_BoundsMin), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&m_BoundsMax), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&m_SphereCenter), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&m_SphereRadius), sizeof(float));

        // Submeshes
        m_Submeshes.resize(submeshCount);
        for (uint32_t i = 0; i < submeshCount; ++i) {
            uint32_t nameLen = 0;
            file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
            if (nameLen > 0) {
                m_Submeshes[i].Name.resize(nameLen);
                file.read(&m_Submeshes[i].Name[0], nameLen);
            }
            file.read(reinterpret_cast<char*>(&m_Submeshes[i].IndexOffset), sizeof(m_Submeshes[i].IndexOffset));
            file.read(reinterpret_cast<char*>(&m_Submeshes[i].IndexCount), sizeof(m_Submeshes[i].IndexCount));
            file.read(reinterpret_cast<char*>(&m_Submeshes[i].VertexOffset), sizeof(m_Submeshes[i].VertexOffset));
            file.read(reinterpret_cast<char*>(&m_Submeshes[i].VertexCount), sizeof(m_Submeshes[i].VertexCount));
            file.read(reinterpret_cast<char*>(&m_Submeshes[i].MaterialSlotIndex),
                      sizeof(m_Submeshes[i].MaterialSlotIndex));
            file.read(reinterpret_cast<char*>(&m_Submeshes[i].LocalTransform), sizeof(glm::mat4));
            file.read(reinterpret_cast<char*>(&m_Submeshes[i].BoundsMin), sizeof(glm::vec3));
            file.read(reinterpret_cast<char*>(&m_Submeshes[i].BoundsMax), sizeof(glm::vec3));
        }

        // Material Slots
        m_MaterialSlots.resize(materialSlotCount);
        for (uint32_t i = 0; i < materialSlotCount; ++i) {
            uint32_t slotNameLen = 0;
            file.read(reinterpret_cast<char*>(&slotNameLen), sizeof(slotNameLen));
            if (slotNameLen > 0) {
                m_MaterialSlots[i].SlotName.resize(slotNameLen);
                file.read(&m_MaterialSlots[i].SlotName[0], slotNameLen);
            }
            uint32_t matPathLen = 0;
            file.read(reinterpret_cast<char*>(&matPathLen), sizeof(matPathLen));
            if (matPathLen > 0) {
                m_MaterialSlots[i].DefaultMaterialPath.resize(matPathLen);
                file.read(&m_MaterialSlots[i].DefaultMaterialPath[0], matPathLen);
            }
        }

        // Vertices
        m_Vertices.resize(vertexCount);
        if (vertexCount > 0) {
            file.read(reinterpret_cast<char*>(m_Vertices.data()), vertexCount * sizeof(FStaticMeshVertex));
        }

        // Indices
        m_Indices.resize(indexCount);
        if (indexCount > 0) {
            file.read(reinterpret_cast<char*>(m_Indices.data()), indexCount * sizeof(uint32_t));
        }

        m_AssetPath = InFilePath;
        return file.good();
    }

} // namespace Leon
