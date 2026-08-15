#pragma once

#include "core/Base.hpp"
#include "asset/AssetTypes.hpp"
#include "renderer/Buffer.hpp"
#include "renderer/Material.hpp"
#include "renderer/MaterialInstance.hpp"
#include "renderer/VertexArray.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon {

    constexpr uint32_t LMESH_MAGIC = 0x48534D4C; // 'LMESH' in little-endian
    constexpr uint32_t LMESH_VERSION = 1;

    struct FStaticMeshVertex {
        glm::vec3 Position{0.0f};
        glm::vec3 Normal{0.0f, 1.0f, 0.0f};
        glm::vec2 TexCoord{0.0f};
        glm::vec3 Tangent{1.0f, 0.0f, 0.0f};
        glm::vec3 Bitangent{0.0f, 0.0f, 1.0f};
        glm::vec3 Color{1.0f};
    };

    struct FStaticSubmesh {
        std::string Name;
        uint32_t IndexOffset = 0;
        uint32_t IndexCount = 0;
        uint32_t VertexOffset = 0;
        uint32_t VertexCount = 0;
        uint32_t MaterialSlotIndex = 0;
        glm::mat4 LocalTransform{1.0f};
        glm::vec3 BoundsMin{0.0f};
        glm::vec3 BoundsMax{0.0f};
    };

    struct FStaticMaterialSlot {
        std::string SlotName;
        std::string DefaultMaterialPath; // Virtual path to .lmat or .lmi
        TRef<FMaterialInstance> MaterialInstance = nullptr;
    };

    class FStaticMesh : public std::enable_shared_from_this<FStaticMesh> {
    public:
        FStaticMesh(const std::string& InName = "StaticMesh");
        ~FStaticMesh() = default;

        static TRef<FStaticMesh> Create(const std::string& InName = "StaticMesh");

        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& InName) { m_Name = InName; }

        const std::string& GetAssetPath() const { return m_AssetPath; }
        void SetAssetPath(const std::string& InPath) { m_AssetPath = InPath; }

        const FUUID& GetUUID() const { return m_UUID; }
        void SetUUID(const FUUID& InUUID) { m_UUID = InUUID; }

        const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
        const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }
        const glm::vec3& GetSphereCenter() const { return m_SphereCenter; }
        float GetSphereRadius() const { return m_SphereRadius; }

        void SetBounds(const glm::vec3& InMin, const glm::vec3& InMax, const glm::vec3& InSphereCenter,
                       float InSphereRadius) {
            m_BoundsMin = InMin;
            m_BoundsMax = InMax;
            m_SphereCenter = InSphereCenter;
            m_SphereRadius = InSphereRadius;
        }

        // Geometry Access
        const std::vector<FStaticMeshVertex>& GetVertices() const { return m_Vertices; }
        std::vector<FStaticMeshVertex>& GetVertices() { return m_Vertices; }

        const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
        std::vector<uint32_t>& GetIndices() { return m_Indices; }

        const std::vector<FStaticSubmesh>& GetSubmeshes() const { return m_Submeshes; }
        std::vector<FStaticSubmesh>& GetSubmeshes() { return m_Submeshes; }

        const std::vector<FStaticMaterialSlot>& GetMaterialSlots() const { return m_MaterialSlots; }
        std::vector<FStaticMaterialSlot>& GetMaterialSlots() { return m_MaterialSlots; }

        TRef<FVertexArray> GetVertexArray() const { return m_VertexArray; }

        /** Allocate GPU VertexArray, VertexBuffer, IndexBuffer and upload geometry */
        void CreateGPUResources();

        /** Compute bounding box and bounding sphere from vertices */
        void CalculateBounds();

        /** Binary Serialization (.lmesh) */
        bool SaveToFile(const std::string& InFilePath) const;
        bool LoadFromFile(const std::string& InFilePath);

    private:
        std::string m_Name;
        std::string m_AssetPath;
        FUUID m_UUID;

        glm::vec3 m_BoundsMin{0.0f};
        glm::vec3 m_BoundsMax{0.0f};
        glm::vec3 m_SphereCenter{0.0f};
        float m_SphereRadius = 0.0f;

        std::vector<FStaticMeshVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        std::vector<FStaticSubmesh> m_Submeshes;
        std::vector<FStaticMaterialSlot> m_MaterialSlots;

        TRef<FVertexArray> m_VertexArray;
    };

} // namespace Leon
