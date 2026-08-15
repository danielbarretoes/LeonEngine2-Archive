#pragma once

#include "core/Base.hpp"
#include "asset/AssetTypes.hpp"
#include "asset/MeshImporter.hpp"
#include "renderer/Material.hpp"
#include "renderer/MaterialInstance.hpp"

#include <string>
#include <vector>

namespace Leon {

    class FMaterialImporter {
    public:
        /** Build native FMaterial from extracted material and available textures */
        static TRef<FMaterial> BuildMaterial(const FExtractedMaterial& InExtracted,
                                             const std::vector<std::string>& InAvailableTextureVirtualPaths);

        /** Save native FMaterial to .lmat on disk */
        static bool SaveMaterialToFile(const TRef<FMaterial>& InMaterial, const std::string& InDestinationLMatPath);

        /** Save sparse FMaterialInstance to .lmi on disk */
        static bool SaveMaterialInstanceToFile(const TRef<FMaterialInstance>& InInstance,
                                               const std::string& InDestinationLMiPath,
                                               const std::string& InParentVirtualPath);

        /** Resolve texture virtual path from material name, slot type, and available texture list */
        static std::string FindMatchingTexture(const std::string& InMaterialName, const std::string& InSlotKeyword1,
                                               const std::string& InSlotKeyword2,
                                               const std::vector<std::string>& InAvailableTextureVirtualPaths);
    };

} // namespace Leon
