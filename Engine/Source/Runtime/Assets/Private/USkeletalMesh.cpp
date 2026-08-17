#include "Assets/USkeletalMesh.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FLog.hpp"
#include "RHI/IRenderDriver.hpp"

#include <algorithm>
#include <fstream>
#include <limits>

namespace Leon {

    namespace {
        void WriteString(std::ostream& Out, const std::string& InStr) {
            uint32_t len = static_cast<uint32_t>(InStr.size());
            Out.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len > 0)
                Out.write(InStr.data(), len);
        }

        bool ReadString(std::istream& In, std::string& OutStr) {
            uint32_t len = 0;
            In.read(reinterpret_cast<char*>(&len), sizeof(len));
            if (!In || len > 1024 * 1024)
                return false;
            OutStr.assign(len, '\0');
            if (len > 0)
                In.read(OutStr.data(), len);
            return static_cast<bool>(In);
        }
    } // namespace

    USkeletalMesh::USkeletalMesh(const std::string& InName) : Name(InName), UUID(FUUID::FromPath(InName)) {}

    TRef<USkeletalMesh> USkeletalMesh::Create(const std::string& InName) {
        return MakeRef<USkeletalMesh>(InName);
    }

    void USkeletalMesh::SetSkeleton(const TRef<USkeleton>& InSkeleton) {
        Skeleton = InSkeleton;
        if (Skeleton) {
            SkeletonUUID = Skeleton->GetUUID();
            if (SkeletonPath.empty())
                SkeletonPath = Skeleton->GetAssetPath();
        }
    }

    TRef<FMaterialInstance>
    ResolveSkeletalSubmeshMaterial(USkeletalMesh& InMesh, const FSkeletalSubmesh& InSubmesh,
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

    void USkeletalMesh::CreateGPUResources() {
        if (Vertices.empty() || Indices.empty()) {
            LE_CORE_WARN("USkeletalMesh: Cannot create GPU resources for empty mesh \"{0}\"", Name);
            return;
        }
        if (!FRenderDriverRegistry::GetActiveDriver()) {
            LE_CORE_WARN("USkeletalMesh: No RenderDriver — skipping GPU upload for \"{0}\"", Name);
            return;
        }

        VertexArray = FVertexArray::Create();
        if (!VertexArray)
            return;

        TRef<FVertexBuffer> vertexBuffer =
            FVertexBuffer::Create(reinterpret_cast<const float*>(Vertices.data()),
                                  static_cast<uint32_t>(Vertices.size() * sizeof(FSkinnedMeshVertex)));
        vertexBuffer->SetLayout(MakeSkinnedMeshLayout());
        VertexArray->AddVertexBuffer(vertexBuffer);

        TRef<FIndexBuffer> indexBuffer = FIndexBuffer::Create(Indices.data(), static_cast<uint32_t>(Indices.size()));
        VertexArray->SetIndexBuffer(indexBuffer);
    }

    void USkeletalMesh::CalculateBounds() {
        if (Vertices.empty()) {
            BoundsMin = BoundsMax = SphereCenter = glm::vec3(0.0f);
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

    bool USkeletalMesh::SaveToFile(const std::string& InFilePath) const {
        std::ofstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("USkeletalMesh: Failed to open \"{0}\" for writing", InFilePath);
            return false;
        }

        uint32_t magic = LSKELETALMESH_MAGIC;
        uint32_t version = LSKELETALMESH_VERSION;
        uint32_t vertexCount = static_cast<uint32_t>(Vertices.size());
        uint32_t indexCount = static_cast<uint32_t>(Indices.size());
        uint32_t submeshCount = static_cast<uint32_t>(Submeshes.size());
        uint32_t materialSlotCount = static_cast<uint32_t>(MaterialSlots.size());

        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        file.write(reinterpret_cast<const char*>(&UUID.High), sizeof(UUID.High));
        file.write(reinterpret_cast<const char*>(&UUID.Low), sizeof(UUID.Low));
        file.write(reinterpret_cast<const char*>(&SkeletonUUID.High), sizeof(SkeletonUUID.High));
        file.write(reinterpret_cast<const char*>(&SkeletonUUID.Low), sizeof(SkeletonUUID.Low));
        WriteString(file, Name);
        WriteString(file, SkeletonPath);
        file.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
        file.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
        file.write(reinterpret_cast<const char*>(&submeshCount), sizeof(submeshCount));
        file.write(reinterpret_cast<const char*>(&materialSlotCount), sizeof(materialSlotCount));
        file.write(reinterpret_cast<const char*>(&BoundsMin), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&BoundsMax), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&SphereCenter), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&SphereRadius), sizeof(float));

        for (const auto& sm : Submeshes) {
            WriteString(file, sm.Name);
            file.write(reinterpret_cast<const char*>(&sm.IndexOffset), sizeof(sm.IndexOffset));
            file.write(reinterpret_cast<const char*>(&sm.IndexCount), sizeof(sm.IndexCount));
            file.write(reinterpret_cast<const char*>(&sm.VertexOffset), sizeof(sm.VertexOffset));
            file.write(reinterpret_cast<const char*>(&sm.VertexCount), sizeof(sm.VertexCount));
            file.write(reinterpret_cast<const char*>(&sm.MaterialSlotIndex), sizeof(sm.MaterialSlotIndex));
            file.write(reinterpret_cast<const char*>(&sm.LocalTransform), sizeof(glm::mat4));
            file.write(reinterpret_cast<const char*>(&sm.BoundsMin), sizeof(glm::vec3));
            file.write(reinterpret_cast<const char*>(&sm.BoundsMax), sizeof(glm::vec3));
        }
        for (const auto& slot : MaterialSlots) {
            WriteString(file, slot.SlotName);
            WriteString(file, slot.DefaultMaterialPath);
        }
        if (!Vertices.empty())
            file.write(reinterpret_cast<const char*>(Vertices.data()), Vertices.size() * sizeof(FSkinnedMeshVertex));
        if (!Indices.empty())
            file.write(reinterpret_cast<const char*>(Indices.data()), Indices.size() * sizeof(uint32_t));

        uint32_t bindCount = static_cast<uint32_t>(InverseBindPoses.size());
        file.write(reinterpret_cast<const char*>(&bindCount), sizeof(bindCount));
        if (bindCount > 0)
            file.write(reinterpret_cast<const char*>(InverseBindPoses.data()), bindCount * sizeof(glm::mat4));
        return file.good();
    }

    bool USkeletalMesh::LoadFromFile(const std::string& InFilePath) {
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("USkeletalMesh: Failed to open \"{0}\" for reading", InFilePath);
            return false;
        }

        uint32_t magic = 0, version = 0, vertexCount = 0, indexCount = 0, submeshCount = 0, materialSlotCount = 0;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (magic != LSKELETALMESH_MAGIC || (version != 1 && version != LSKELETALMESH_VERSION)) {
            LE_CORE_ERROR("USkeletalMesh: Invalid magic/version in \"{0}\"", InFilePath);
            return false;
        }
        file.read(reinterpret_cast<char*>(&UUID.High), sizeof(UUID.High));
        file.read(reinterpret_cast<char*>(&UUID.Low), sizeof(UUID.Low));
        file.read(reinterpret_cast<char*>(&SkeletonUUID.High), sizeof(SkeletonUUID.High));
        file.read(reinterpret_cast<char*>(&SkeletonUUID.Low), sizeof(SkeletonUUID.Low));
        if (!ReadString(file, Name) || !ReadString(file, SkeletonPath))
            return false;
        file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
        file.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
        file.read(reinterpret_cast<char*>(&submeshCount), sizeof(submeshCount));
        file.read(reinterpret_cast<char*>(&materialSlotCount), sizeof(materialSlotCount));
        file.read(reinterpret_cast<char*>(&BoundsMin), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&BoundsMax), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&SphereCenter), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&SphereRadius), sizeof(float));
        if (!file || vertexCount > 50'000'000)
            return false;

        Submeshes.resize(submeshCount);
        for (auto& sm : Submeshes) {
            if (!ReadString(file, sm.Name))
                return false;
            file.read(reinterpret_cast<char*>(&sm.IndexOffset), sizeof(sm.IndexOffset));
            file.read(reinterpret_cast<char*>(&sm.IndexCount), sizeof(sm.IndexCount));
            file.read(reinterpret_cast<char*>(&sm.VertexOffset), sizeof(sm.VertexOffset));
            file.read(reinterpret_cast<char*>(&sm.VertexCount), sizeof(sm.VertexCount));
            file.read(reinterpret_cast<char*>(&sm.MaterialSlotIndex), sizeof(sm.MaterialSlotIndex));
            file.read(reinterpret_cast<char*>(&sm.LocalTransform), sizeof(sm.LocalTransform));
            file.read(reinterpret_cast<char*>(&sm.BoundsMin), sizeof(glm::vec3));
            file.read(reinterpret_cast<char*>(&sm.BoundsMax), sizeof(glm::vec3));
        }
        MaterialSlots.resize(materialSlotCount);
        for (auto& slot : MaterialSlots) {
            if (!ReadString(file, slot.SlotName) || !ReadString(file, slot.DefaultMaterialPath))
                return false;
        }
        Vertices.resize(vertexCount);
        if (vertexCount > 0)
            file.read(reinterpret_cast<char*>(Vertices.data()), vertexCount * sizeof(FSkinnedMeshVertex));
        Indices.resize(indexCount);
        if (indexCount > 0)
            file.read(reinterpret_cast<char*>(Indices.data()), indexCount * sizeof(uint32_t));

        InverseBindPoses.clear();
        if (version >= 2) {
            uint32_t bindCount = 0;
            file.read(reinterpret_cast<char*>(&bindCount), sizeof(bindCount));
            if (!file || bindCount > 4096)
                return false;
            InverseBindPoses.resize(bindCount);
            if (bindCount > 0)
                file.read(reinterpret_cast<char*>(InverseBindPoses.data()), bindCount * sizeof(glm::mat4));
        }

        AssetPath = InFilePath;
        if (!SkeletonPath.empty())
            Skeleton = UAssetManager::GetSkeleton(SkeletonPath);
        return file.good();
    }

} // namespace Leon
