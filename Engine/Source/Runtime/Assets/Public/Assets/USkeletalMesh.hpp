#pragma once

#include "Core/Base.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Assets/FAnimTypes.hpp"
#include "Assets/USkeleton.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Assets/FSkeletalMeshSocket.hpp"
#include "Renderer/FVertexLayout.hpp"
#include "RHI/FVertexArray.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon {

    using FSkeletalSubmesh = FStaticSubmesh;
    using FSkeletalMaterialSlot = FStaticMaterialSlot;

    class USkeletalMesh : public std::enable_shared_from_this<USkeletalMesh> {
    public:
        USkeletalMesh(const std::string& InName = "SkeletalMesh");
        static TRef<USkeletalMesh> Create(const std::string& InName = "SkeletalMesh");

        const std::string& GetName() const { return Name; }
        void SetName(const std::string& InName) { Name = InName; }

        const std::string& GetAssetPath() const { return AssetPath; }
        void SetAssetPath(const std::string& InPath) { AssetPath = InPath; }

        const FUUID& GetUUID() const { return UUID; }
        void SetUUID(const FUUID& InUUID) { UUID = InUUID; }

        const std::string& GetSkeletonPath() const { return SkeletonPath; }
        void SetSkeletonPath(const std::string& InPath) { SkeletonPath = InPath; }

        TRef<USkeleton> GetSkeleton() const { return Skeleton; }
        void SetSkeleton(const TRef<USkeleton>& InSkeleton);

        void AddSocket(const FSkeletalMeshSocket& InSocket) { Sockets.push_back(InSocket); }
        const std::vector<FSkeletalMeshSocket>& GetSockets() const { return Sockets; }
        const FSkeletalMeshSocket* FindSocket(const std::string& InName) const;

        /**
         * Optional per-mesh inverse-bind matrices (same length as skeleton bones).
         * Enables multiple skinned meshes to share one USkeleton / anim set while keeping
         * their own bind pose (Mixamo retarget / different character proportions).
         */
        std::vector<glm::mat4>& GetInverseBindPoses() { return InverseBindPoses; }
        const std::vector<glm::mat4>& GetInverseBindPoses() const { return InverseBindPoses; }
        bool HasMeshBindPoses() const { return !InverseBindPoses.empty(); }

        const glm::vec3& GetBoundsMin() const { return BoundsMin; }
        const glm::vec3& GetBoundsMax() const { return BoundsMax; }
        const glm::vec3& GetSphereCenter() const { return SphereCenter; }
        float GetSphereRadius() const { return SphereRadius; }

        void SetBounds(const glm::vec3& InMin, const glm::vec3& InMax, const glm::vec3& InSphereCenter,
                       float InSphereRadius) {
            BoundsMin = InMin;
            BoundsMax = InMax;
            SphereCenter = InSphereCenter;
            SphereRadius = InSphereRadius;
        }

        std::vector<FSkinnedMeshVertex>& GetVertices() { return Vertices; }
        const std::vector<FSkinnedMeshVertex>& GetVertices() const { return Vertices; }

        std::vector<uint32_t>& GetIndices() { return Indices; }
        const std::vector<uint32_t>& GetIndices() const { return Indices; }

        std::vector<FSkeletalSubmesh>& GetSubmeshes() { return Submeshes; }
        const std::vector<FSkeletalSubmesh>& GetSubmeshes() const { return Submeshes; }

        std::vector<FSkeletalMaterialSlot>& GetMaterialSlots() { return MaterialSlots; }
        const std::vector<FSkeletalMaterialSlot>& GetMaterialSlots() const { return MaterialSlots; }

        TRef<FVertexArray> GetVertexArray() const { return VertexArray; }

        void CreateGPUResources();
        void CalculateBounds();

        EAssetForwardAxis GetAssetForwardAxis() const { return AssetForwardAxis; }
        void SetAssetForwardAxis(EAssetForwardAxis InAxis) { AssetForwardAxis = InAxis; }

        bool SaveToFile(const std::string& InFilePath) const;
        bool LoadFromFile(const std::string& InFilePath);

    private:
        std::string Name;
        std::string AssetPath;
        std::string SkeletonPath;
        FUUID UUID;
        FUUID SkeletonUUID;

        glm::vec3 BoundsMin{0.0f};
        glm::vec3 BoundsMax{0.0f};
        glm::vec3 SphereCenter{0.0f};
        float SphereRadius = 0.0f;

        TRef<USkeleton> Skeleton;
        std::vector<glm::mat4> InverseBindPoses;
        std::vector<FSkeletalMeshSocket> Sockets;
        std::vector<FSkinnedMeshVertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<FSkeletalSubmesh> Submeshes;
        std::vector<FSkeletalMaterialSlot> MaterialSlots;
        TRef<FVertexArray> VertexArray;
        EAssetForwardAxis AssetForwardAxis = EAssetForwardAxis::SourcePosZ;
    };

    TRef<FMaterialInstance>
    ResolveSkeletalSubmeshMaterial(USkeletalMesh& InMesh, const FSkeletalSubmesh& InSubmesh,
                                   const std::vector<TRef<FMaterialInstance>>& InMaterialOverrides);

} // namespace Leon
