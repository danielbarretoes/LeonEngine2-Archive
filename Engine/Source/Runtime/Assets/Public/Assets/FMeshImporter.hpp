#pragma once

#include "Core/Base.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Renderer/FMaterial.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Assets/USkeleton.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/UAnimSequence.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon {

    struct FExtractedMaterial {
        std::string Name;
        glm::vec3 BaseColor{1.0f};
        float Metallic = 0.0f;
        float Roughness = 0.5f;
        float AO = 1.0f;
        float NormalScale = 1.0f;
        glm::vec3 EmissiveColor{0.0f};
        float EmissiveIntensity = 0.0f;
        EAlphaMode AlphaMode = EAlphaMode::Opaque;
        float AlphaCutoff = 0.5f;
        bool bDoubleSided = false;

        // Texture file names or paths referenced by FBX
        std::string DiffuseTextureName;
        std::string NormalTextureName;
        std::string RoughnessTextureName;
        std::string MetallicTextureName;
        std::string AOTextureName;
        std::string EmissiveTextureName;
    };

    struct FMeshImportSettings {
        bool bGenerateTangents = true;
        bool bGenerateNormalsIfMissing = true;
        bool bFlipUVs = false;
        bool bExtractMaterials = true;
        float ScaleFactor = 1.0f; // 1.0 = auto-detect / standard
        /** When set, animation FBX files retarget onto this skeleton instead of extracting a new one. */
        TRef<USkeleton> SharedSkeleton;
    };

    struct FMeshImportResult {
        TRef<UStaticMesh> StaticMesh = nullptr;
        TRef<USkeleton> Skeleton = nullptr;
        TRef<USkeletalMesh> SkeletalMesh = nullptr;
        std::vector<TRef<UAnimSequence>> Animations;
        std::vector<TRef<UStaticMesh>> SeparateMeshes;
        std::vector<FExtractedMaterial> ExtractedMaterials;
        std::vector<std::string> ReferencedTextureNames;
        std::vector<std::string> Errors;
        std::vector<std::string> Warnings;

        bool HasErrors() const { return !Errors.empty(); }
        bool IsSkeletal() const { return SkeletalMesh != nullptr || Skeleton != nullptr; }
    };

    class FMeshImporter {
    public:
        /** Import FBX file and produce UStaticMesh and extracted materials */
        static bool ImportFBX(const std::string& InSourcePath, const FMeshImportSettings& InSettings,
                              FMeshImportResult& OutResult);

        /** Resolve texture filenames against a directory of raw texture files */
        static std::string ResolveTextureReference(const std::string& InMaterialName, const std::string& InSlotType,
                                                   const std::vector<std::string>& InAvailableTextureFiles);
    };

} // namespace Leon
