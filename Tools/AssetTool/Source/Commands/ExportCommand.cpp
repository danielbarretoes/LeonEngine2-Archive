#include "AssetTool/Commands/ExportCommand.hpp"
#include "Assets/FAssetExporter.hpp"
#include "Assets/FAssetPath.hpp"
#include "Assets/FHDRImporter.hpp"
#include "Assets/FTextureImporter.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Core/FLog.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace Leon::Tools {

    namespace {

        std::string ToLower(std::string S) {
            std::transform(S.begin(), S.end(), S.begin(),
                           [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
            return S;
        }

        std::string DefaultExtForNative(const std::string& NativeExt, const std::string& Format) {
            const std::string Fmt = ToLower(Format);
            if (NativeExt == "lmesh") {
                if (Fmt == "fbx")
                    return "fbx";
                return "obj";
            }
            if (NativeExt == "ltex") {
                if (Fmt == "tga")
                    return "tga";
                if (Fmt == "bmp")
                    return "bmp";
                if (Fmt == "jpg" || Fmt == "jpeg")
                    return "jpg";
                return "png";
            }
            if (NativeExt == "lhdr") {
                if (Fmt == "exr")
                    return "exr";
                return "hdr";
            }
            return {};
        }

        bool ExportTexture(const FNativeTextureData& Tex, const fs::path& Dest, const std::string& Forced) {
            const std::string Ext = ToLower(Dest.extension().string());
            if (Ext == ".jpg" || Ext == ".jpeg" || Forced == "jpg" || Forced == "jpeg")
                return FAssetExporter::ExportTextureToJPG(Tex, Dest.string());
            if (Ext == ".bmp" || Forced == "bmp")
                return FAssetExporter::ExportTextureToBMP(Tex, Dest.string());
            if (Ext == ".tga" || Forced == "tga")
                return FAssetExporter::ExportTextureToTGA(Tex, Dest.string());
            return FAssetExporter::ExportTextureToPNG(Tex, Dest.string());
        }

        bool ExportOne(const fs::path& NativePath, const fs::path& OutPath, const std::string& Format) {
            const std::string NativeExt = FAssetPath::GetExtension(NativePath.string());
            const std::string Forced = ToLower(Format);
            fs::path Dest = OutPath;

            if (Dest.extension().empty() || Forced != "auto") {
                const std::string Ext = DefaultExtForNative(NativeExt, Forced == "auto" ? "" : Forced);
                if (!Ext.empty())
                    Dest.replace_extension(Ext);
            }

            if (NativeExt == "lmesh") {
                UStaticMesh Mesh;
                if (!Mesh.LoadFromFile(NativePath.string()))
                    return false;
                if (ToLower(Dest.extension().string()) == ".fbx" || Forced == "fbx")
                    return FAssetExporter::ExportStaticMeshToFBX(Mesh, Dest.string());
                return FAssetExporter::ExportStaticMeshToOBJ(Mesh, Dest.string());
            }
            if (NativeExt == "ltex") {
                FNativeTextureData Tex;
                if (!Tex.LoadFromFile(NativePath.string()))
                    return false;
                return ExportTexture(Tex, Dest, Forced);
            }
            if (NativeExt == "lhdr") {
                FNativeHDRData Hdr;
                if (!Hdr.LoadFromFile(NativePath.string()))
                    return false;
                if (ToLower(Dest.extension().string()) == ".exr" || Forced == "exr")
                    return FAssetExporter::ExportHDRToEXR(Hdr, Dest.string());
                return FAssetExporter::ExportHDRToRadiance(Hdr, Dest.string());
            }
            return false;
        }

    } // namespace

    void FExportCommand::PrintHelp() const {
        std::cout << "Usage:\n  " << GetUsage() << "\n\n";
        std::cout << "Bulk (mirrors import):\n";
        std::cout << "  --content <dir>   Native Content root (source)\n";
        std::cout << "  --raw <dir>       Output authoring/Raw directory\n";
        std::cout << "  --force           Overwrite existing files\n";
        std::cout << "  --format <fmt>    auto|obj|fbx|png|tga|jpg|jpeg|bmp|hdr|exr\n\n";
        std::cout << "Single file:\n";
        std::cout << "  --asset <path>    Native .lmesh / .ltex / .lhdr\n";
        std::cout << "  --out <path>      Destination file (extension selects format if auto)\n\n";
        std::cout << "Round-trip with import:\n";
        std::cout << "  .lmesh ↔ .obj / .fbx\n";
        std::cout << "  .ltex  ↔ .png / .tga / .jpg / .jpeg / .bmp\n";
        std::cout << "  .lhdr  ↔ .hdr / .exr\n";
        std::cout << "Defaults (auto): .lmesh→.obj, .ltex→.png, .lhdr→.hdr\n";
    }

    int FExportCommand::Execute(const FCommandArgs& InArgs) {
        const std::string Asset = InArgs.GetOption("asset");
        const std::string OutFile = InArgs.GetOption("out");
        const std::string ContentDir = InArgs.GetOption("content");
        const std::string RawDir = InArgs.GetOption("raw");
        const bool bForce = InArgs.HasFlag("force");
        std::string Format = InArgs.GetOption("format", "auto");
        Format = ToLower(Format);

        if (!Asset.empty()) {
            if (OutFile.empty()) {
                std::cerr << "[ERROR] --out <path> is required with --asset.\n\n";
                PrintHelp();
                return 1;
            }
            if (!fs::exists(Asset)) {
                std::cerr << "[ERROR] Asset not found: " << Asset << "\n";
                return 1;
            }
            if (!ExportOne(Asset, OutFile, Format)) {
                std::cerr << "[ERROR] Export failed: " << Asset << "\n";
                return 1;
            }
            std::cout << "[OK] Exported " << Asset << " -> " << OutFile << "\n";
            return 0;
        }

        if (ContentDir.empty() || RawDir.empty()) {
            std::cerr << "[ERROR] Provide --content + --raw, or --asset + --out.\n\n";
            PrintHelp();
            return 1;
        }

        const fs::path ContentPath = ContentDir;
        const fs::path RawPath = RawDir;
        if (!fs::exists(ContentPath)) {
            std::cerr << "[ERROR] Content directory does not exist: " << ContentPath.string() << "\n";
            return 1;
        }

        fs::create_directories(RawPath / "Meshes");
        fs::create_directories(RawPath / "Textures");
        fs::create_directories(RawPath / "HDR");

        std::vector<fs::path> Files;
        for (const auto& Entry : fs::recursive_directory_iterator(ContentPath)) {
            if (!Entry.is_regular_file())
                continue;
            const std::string Ext = FAssetPath::GetExtension(Entry.path().string());
            if (Ext == "lmesh" || Ext == "ltex" || Ext == "lhdr")
                Files.push_back(Entry.path());
        }

        std::cout << "===============================================================\n";
        std::cout << " LeonEngine2 Asset Exporter\n";
        std::cout << " Source:  " << ContentPath.string() << "\n";
        std::cout << " Output:  " << RawPath.string() << "\n";
        std::cout << " Format:  " << Format << "\n";
        std::cout << " Force:   " << (bForce ? "YES" : "NO") << "\n";
        std::cout << "===============================================================\n\n";
        std::cout << "[DISCOVER] Found " << Files.size() << " native assets.\n\n";

        const auto Start = std::chrono::high_resolution_clock::now();
        size_t Exported = 0;
        size_t Skipped = 0;
        size_t Errors = 0;

        for (const fs::path& Native : Files) {
            const std::string Ext = FAssetPath::GetExtension(Native.string());
            const std::string Stem = Native.stem().string();
            fs::path Rel = fs::relative(Native, ContentPath);
            fs::path DestDir = RawPath;
            if (Ext == "lmesh")
                DestDir /= "Meshes";
            else if (Ext == "ltex")
                DestDir /= "Textures";
            else
                DestDir /= "HDR";

            if (Rel.has_parent_path()) {
                const fs::path Parent = Rel.parent_path();
                const std::string Top = Parent.empty() ? std::string{} : (*Parent.begin()).string();
                if (Top == "Meshes" || Top == "Textures" || Top == "HDR") {
                    fs::path Rest = Parent;
                    if (!Rest.empty()) {
                        auto It = Rest.begin();
                        ++It;
                        fs::path Sub;
                        for (; It != Rest.end(); ++It)
                            Sub /= *It;
                        if (!Sub.empty())
                            DestDir /= Sub;
                    }
                } else if (!Parent.empty()) {
                    DestDir /= Parent;
                }
            }

            const std::string OutExt = DefaultExtForNative(Ext, Format);
            fs::path Dest = DestDir / (Stem + "." + OutExt);

            if (!bForce && fs::exists(Dest)) {
                ++Skipped;
                continue;
            }

            std::cout << "[EXPORT] " << Native.string() << " -> " << Dest.string() << "\n";
            if (ExportOne(Native, Dest, Format)) {
                ++Exported;
            } else {
                ++Errors;
                std::cerr << "  [FAIL] " << Native.string() << "\n";
            }
        }

        const auto End = std::chrono::high_resolution_clock::now();
        const double Sec = std::chrono::duration<double>(End - Start).count();
        std::cout << "\n[SUMMARY] exported=" << Exported << " skipped=" << Skipped << " errors=" << Errors
                  << " in " << std::fixed << std::setprecision(2) << Sec << "s\n";
        return Errors > 0 ? 1 : 0;
    }

} // namespace Leon::Tools
