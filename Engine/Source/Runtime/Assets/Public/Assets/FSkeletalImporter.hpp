#pragma once

#include "Assets/FMeshImporter.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/USkeleton.hpp"
#include "Assets/USkeletalMesh.hpp"

struct ufbx_scene;

namespace Leon {

    /**
     * Canonical FBX skeletal extract (same ufbx load as static meshes).
     * Called from FMeshImporter when the scene has skin deformers or bone animation.
     */
    class FSkeletalImporter {
    public:
        static bool ImportFromScene(ufbx_scene* InScene, const FMeshImportSettings& InSettings,
                                    const std::string& InSourcePath, FMeshImportResult& OutResult);

        static bool ImportAnimationsFromScene(ufbx_scene* InScene, const TRef<USkeleton>& InSkeleton,
                                              const std::string& InSourcePath, FMeshImportResult& OutResult);

        static void ImportSkeletalGeometry(ufbx_scene* InScene, const FMeshImportSettings& InSettings,
                                           const TRef<USkeleton>& InSkeleton, FMeshImportResult& OutResult);
    };

} // namespace Leon
