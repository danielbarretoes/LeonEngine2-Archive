#pragma once

#include "Core/Base.hpp"

#include <string>

namespace Leon {

    class UStaticMesh;
    struct FNativeTextureData;
    struct FNativeHDRData;

    /**
     * Export native Leon assets back to authoring formats (inverse of AssetTool import).
     *
     * Meshes:  .lmesh  → .obj / .fbx
     * Textures:.ltex  → .png / .tga / .jpg / .jpeg / .bmp
     * HDR:     .lhdr  → .hdr / .exr
     */
    class FAssetExporter {
    public:
        static bool ExportStaticMeshToOBJ(const UStaticMesh& InMesh, const std::string& InOutPath);
        static bool ExportStaticMeshToFBX(const UStaticMesh& InMesh, const std::string& InOutPath);

        static bool ExportTextureToPNG(const FNativeTextureData& InTex, const std::string& InOutPath);
        static bool ExportTextureToTGA(const FNativeTextureData& InTex, const std::string& InOutPath);
        static bool ExportTextureToJPG(const FNativeTextureData& InTex, const std::string& InOutPath, int InQuality = 90);
        static bool ExportTextureToBMP(const FNativeTextureData& InTex, const std::string& InOutPath);

        static bool ExportHDRToRadiance(const FNativeHDRData& InHdr, const std::string& InOutPath);
        static bool ExportHDRToEXR(const FNativeHDRData& InHdr, const std::string& InOutPath);

        /** Load native file and export; destination extension selects format. */
        static bool ExportNativeFile(const std::string& InNativePath, const std::string& InOutPath);
    };

} // namespace Leon
