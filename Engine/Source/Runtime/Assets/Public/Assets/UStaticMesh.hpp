#pragma once

#include "Core/Base.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Assets/FLODSettings.hpp"
#include "RHI/FBuffer.hpp"
#include "Renderer/FMaterial.hpp"
#include "Renderer/FMaterialInstance.hpp"
#include "Renderer/FVertexLayout.hpp"
#include "RHI/FVertexArray.hpp"

#include <glm/glm.hpp>
#include <iosfwd>
#include <string>
#include <vector>

namespace Leon {

    using FStaticMeshVertex = FCanonicalMeshVertex;

    /** On-disk vertex layout for .lmesh version 1 (no LightmapUV, separate bitangent). */
    struct FStaticMeshVertexV1 {
        glm::vec3 Position{0.0f};
        glm::vec3 Normal{0.0f, 1.0f, 0.0f};
        glm::vec2 TexCoord{0.0f};
        glm::vec3 Tangent{1.0f, 0.0f, 0.0f};
        glm::vec3 Bitangent{0.0f, 0.0f, 1.0f};
        glm::vec3 Color{1.0f};
    };

    /** On-disk vertex layout for .lmesh version 2 (LightmapUV before TBN). */
    struct FStaticMeshVertexV2 {
        glm::vec3 Position{0.0f};
        glm::vec3 Normal{0.0f, 1.0f, 0.0f};
        glm::vec2 TexCoord{0.0f};
        glm::vec2 LightmapUV{0.0f};
        glm::vec3 Tangent{1.0f, 0.0f, 0.0f};
        glm::vec3 Bitangent{0.0f, 0.0f, 1.0f};
        glm::vec3 Color{1.0f};
    };

    constexpr uint32_t LMESH_MAGIC = 0x48534D4C; // 'LMESH' in little-endian
    constexpr uint32_t LMESH_VERSION = 5;
    constexpr uint32_t LMESH_VERSION_V1 = 1;
    constexpr uint32_t LMESH_VERSION_V2 = 2;
    constexpr uint32_t LMESH_VERSION_V3 = 3;
    constexpr uint32_t LMESH_VERSION_V4 = 4;

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

    /** Reduced static-mesh LOD (LOD1+). LOD0 is Vertices/Indices/Submeshes on UStaticMesh. */
    struct FStaticMeshLOD {
        std::vector<FStaticMeshVertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<FStaticSubmesh> Submeshes;
        TRef<FVertexArray> VertexArray;
        float TriangleRatio = 1.0f;
    };

    class UStaticMesh : public std::enable_shared_from_this<UStaticMesh> {
    public:
        UStaticMesh(const std::string& InName = "StaticMesh");
        ~UStaticMesh() = default;

        static TRef<UStaticMesh> Create(const std::string& InName = "StaticMesh");

        const std::string& GetName() const { return Name; }
        void SetName(const std::string& InName) { Name = InName; }

        const std::string& GetAssetPath() const { return AssetPath; }
        void SetAssetPath(const std::string& InPath) { AssetPath = InPath; }

        const FUUID& GetUUID() const { return UUID; }
        void SetUUID(const FUUID& InUUID) { UUID = InUUID; }

        const glm::vec3& GetBoundsMin() const { return BoundsMin; }
        const glm::vec3& GetBoundsMax() const { return BoundsMax; }
        const glm::vec3& GetSphereCenter() const { return SphereCenter; }
        float GetSphereRadius() const { return SphereRadius; }

        /** True only when GenerateBoxPackedLightmapUVs has written unique UV1. */
        bool HasUniqueLightmapUV() const { return bHasUniqueLightmapUV; }
        void SetHasUniqueLightmapUV(bool bInValue) { bHasUniqueLightmapUV = bInValue; }

        void SetBounds(const glm::vec3& InMin, const glm::vec3& InMax, const glm::vec3& InSphereCenter,
                       float InSphereRadius) {
            BoundsMin = InMin;
            BoundsMax = InMax;
            SphereCenter = InSphereCenter;
            SphereRadius = InSphereRadius;
        }

        // Geometry Access
        const std::vector<FStaticMeshVertex>& GetVertices() const { return Vertices; }
        std::vector<FStaticMeshVertex>& GetVertices() { return Vertices; }

        const std::vector<uint32_t>& GetIndices() const { return Indices; }
        std::vector<uint32_t>& GetIndices() { return Indices; }

        const std::vector<FStaticSubmesh>& GetSubmeshes() const { return Submeshes; }
        std::vector<FStaticSubmesh>& GetSubmeshes() { return Submeshes; }

        const std::vector<FStaticMaterialSlot>& GetMaterialSlots() const { return MaterialSlots; }
        std::vector<FStaticMaterialSlot>& GetMaterialSlots() { return MaterialSlots; }

        TRef<FVertexArray> GetVertexArray() const { return VertexArray; }

        uint32_t GetLODCount() const { return 1u + static_cast<uint32_t>(ReducedLODs.size()); }
        uint32_t GetLODSettingsHash() const { return LODSettingsHash; }
        TRef<FVertexArray> GetLODVertexArray(uint32_t InLOD) const;
        const std::vector<FStaticSubmesh>& GetLODSubmeshes(uint32_t InLOD) const;
        uint32_t GetLODIndexCount(uint32_t InLOD) const;
        uint32_t GetSourceTriangleCount() const { return static_cast<uint32_t>(Indices.size() / 3); }
        uint32_t GetLODTriangleCount(uint32_t InLOD) const { return GetLODIndexCount(InLOD) / 3; }

        /** Build reduced LODs from current source geometry. Safe on tiny/degenerate meshes. */
        void BuildAutomaticLODs(const FLODSettings& InSettings = FLODSettings::Default());

        /** Allocate GPU VertexArray, FVertexBuffer, FIndexBuffer and upload geometry */
        void CreateGPUResources();

        /** Compute bounding box and bounding sphere from vertices */
        void CalculateBounds();

        /** Binary Serialization (.lmesh) */
        bool SaveToFile(const std::string& InFilePath) const;
        bool LoadFromFile(const std::string& InFilePath);

    private:
        std::string Name;
        std::string AssetPath;
        FUUID UUID;

        glm::vec3 BoundsMin{0.0f};
        glm::vec3 BoundsMax{0.0f};
        glm::vec3 SphereCenter{0.0f};
        float SphereRadius = 0.0f;

        std::vector<FStaticMeshVertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<FStaticSubmesh> Submeshes;
        std::vector<FStaticMaterialSlot> MaterialSlots;

        TRef<FVertexArray> VertexArray;
        bool bHasUniqueLightmapUV = false;
        std::vector<FStaticMeshLOD> ReducedLODs;
        uint32_t LODSettingsHash = 0;

        TRef<FVertexArray> UploadGeometry(const std::vector<FStaticMeshVertex>& InVertices,
                                          const std::vector<uint32_t>& InIndices) const;
        bool WriteLODBlob(std::ostream& InFile, const FStaticMeshLOD& InLOD) const;
        bool ReadLODBlob(std::istream& InFile, FStaticMeshLOD& OutLOD);
    };

    /**
     * Resolves the material instance for a static submesh:
     * component MaterialOverrides → mesh slot default → engine default material.
     * Shared by geometry and planar-reflection passes so reflections keep albedo/textures.
     */
    TRef<FMaterialInstance>
    ResolveStaticSubmeshMaterial(UStaticMesh& InMesh, const FStaticSubmesh& InSubmesh,
                                 const std::vector<TRef<FMaterialInstance>>& InMaterialOverrides);

} // namespace Leon
