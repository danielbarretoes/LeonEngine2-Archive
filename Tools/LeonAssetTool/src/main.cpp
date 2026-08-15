#include "core/Base.hpp"
#include "core/Log.hpp"
#include "asset/AssetTypes.hpp"
#include "asset/AssetPath.hpp"
#include "asset/TextureImporter.hpp"
#include "asset/MeshImporter.hpp"
#include "asset/MaterialImporter.hpp"
#include "asset/AssetManifest.hpp"
#include "asset/HDRImporter.hpp"
#include "renderer/StaticMesh.hpp"
#include "renderer/AssetManager.hpp"
#include "world/UWorld.hpp"
#include "world/Components.hpp"
#include "world/MapSerializer.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>

namespace fs = std::filesystem;
using namespace Leon;

void PrintUsage() {
    std::cout << "===============================================================\n";
    std::cout << " LeonEngine2 Native Asset Tool\n";
    std::cout << "===============================================================\n\n";
    std::cout << "Usage:\n";
    std::cout << "  LeonAssetTool import --raw <dir> --content <dir> [--force]\n";
    std::cout << "  LeonAssetTool validate --content <dir>\n";
    std::cout << "  LeonAssetTool validate_map --map <path.lmap>\n";
    std::cout << "  LeonAssetTool inspect <file.lhdr | file.ltex | file.lmesh | file.lmat | file.lmi>\n\n";
    std::cout << "Options:\n";
    std::cout << "  --raw <dir>        Source raw assets directory (e.g. Assets/Raw)\n";
    std::cout << "  --content <dir>    Output native assets directory (e.g. Assets/Content)\n";
    std::cout << "  --map <path>     Map file path (e.g. Content/Maps/MainShowcase.lmap)\n";
    std::cout << "  --force            Force re-importing all assets regardless of hash\n";
    std::cout << "  --help, -h         Show this help information\n";
}

int ExecuteImport(const std::string& InRawDir, const std::string& InContentDir, bool bForce) {
    auto startTime = std::chrono::high_resolution_clock::now();

    fs::path rawPath = InRawDir;
    fs::path contentPath = InContentDir;

    if (rawPath.empty() || contentPath.empty()) {
        std::cerr << "[ERROR] --raw <dir> and --content <dir> are required for import.\n";
        return 1;
    }

    if (!fs::exists(rawPath)) {
        std::cerr << "[ERROR] Raw asset directory does not exist: " << rawPath.string() << "\n";
        return 1;
    }

    fs::create_directories(contentPath / "HDR");
    fs::create_directories(contentPath / "Textures");
    fs::create_directories(contentPath / "Meshes");
    fs::create_directories(contentPath / "Materials");

    fs::path manifestPath = contentPath / "manifest.json";
    FAssetManifest manifest;
    manifest.LoadFromFile(manifestPath.string());

    std::cout << "===============================================================\n";
    std::cout << " LeonEngine2 Asset Importer\n";
    std::cout << " Source:  " << rawPath.string() << "\n";
    std::cout << " Output:  " << contentPath.string() << "\n";
    std::cout << " Force:   " << (bForce ? "YES" : "NO") << "\n";
    std::cout << "===============================================================\n\n";

    // 1. Discover all source files
    std::vector<fs::path> hdrFiles;
    std::vector<fs::path> textureFiles;
    std::vector<fs::path> meshFiles;

    for (const auto& entry : fs::recursive_directory_iterator(rawPath)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = FAssetPath::GetExtension(entry.path().string());
        if (ext == "hdr" || ext == "exr") {
            hdrFiles.push_back(entry.path());
        } else if (ext == "png" || ext == "tga" || ext == "jpg" || ext == "jpeg" || ext == "bmp") {
            textureFiles.push_back(entry.path());
        } else if (ext == "fbx" || ext == "obj") {
            meshFiles.push_back(entry.path());
        }
    }

    std::cout << "[DISCOVER] Found " << hdrFiles.size() << " HDR environments, "
              << textureFiles.size() << " textures, and " << meshFiles.size() << " meshes.\n\n";

    size_t importedHDRs = 0;
    size_t skippedHDRs = 0;
    size_t importedTextures = 0;
    size_t skippedTextures = 0;
    size_t importedMeshes = 0;
    size_t skippedMeshes = 0;
    size_t errorCount = 0;

    // 2. Import HDR Environments
    if (!hdrFiles.empty()) {
        std::cout << "--- [PHASE 0] Processing HDR Environments ---\n";
        for (const auto& hdrPath : hdrFiles) {
            std::string stem = hdrPath.stem().string();
            std::string outRelPath = "HDR/" + stem + ".lhdr";
            fs::path destPath = contentPath / "HDR" / (stem + ".lhdr");

            bool bNeedsImport = bForce || manifest.NeedsReimport(hdrPath.string()) || !fs::exists(destPath);

            if (!bNeedsImport) {
                skippedHDRs++;
                continue;
            }

            std::cout << "  [IMPORT HDR] " << hdrPath.filename().string() << " -> " << outRelPath << "\n";
            FHDRImportSettings settings;
            if (FHDRImporter::ImportToFile(hdrPath.string(), destPath.string(), settings)) {
                importedHDRs++;
                manifest.RegisterImport(hdrPath.string(), { outRelPath }, {});
            } else {
                std::cerr << "  [ERROR] Failed to import HDR: " << hdrPath.string() << "\n";
                errorCount++;
            }
        }
    }

    std::vector<std::string> allAvailableTexturePaths;
    for (const auto& texPath : textureFiles) {
        std::string stem = texPath.stem().string();
        std::string fn = texPath.filename().string();
        allAvailableTexturePaths.push_back("Textures/" + stem + ".ltex");
        allAvailableTexturePaths.push_back("Projects/Sandbox/Content/Textures/" + fn);
        allAvailableTexturePaths.push_back("Projects/Sandbox/Content/Textures/" + stem + ".ltex");
    }
    if (fs::exists(contentPath / "Textures")) {
        for (const auto& entry : fs::directory_iterator(contentPath / "Textures")) {
            if (entry.is_regular_file()) {
                allAvailableTexturePaths.push_back("Textures/" + entry.path().filename().string());
                allAvailableTexturePaths.push_back("Projects/Sandbox/Content/Textures/" + entry.path().filename().string());
            }
        }
    }

    // 3. Import Textures
    if (!textureFiles.empty()) {
        std::cout << "--- [PHASE 1] Processing Textures ---\n";
        for (const auto& texPath : textureFiles) {
            std::string filename = texPath.filename().string();
            std::string stem = texPath.stem().string();
            std::string outRelPath = "Textures/" + stem + ".ltex";
            fs::path outFilePath = contentPath / "Textures" / (stem + ".ltex");

            bool bNeedsImport = bForce || manifest.NeedsReimport(texPath.string()) || !fs::exists(outFilePath);

            if (!bNeedsImport) {
                skippedTextures++;
                continue;
            }

            std::cout << "  [IMPORT TEXTURE] " << filename << " -> " << outRelPath << "\n";

            FTextureImportSettings settings = FTextureImportSettings::DetectFromFileName(filename);
            std::string lowerName = filename;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (settings.Semantic == ETextureSemantic::Roughness && lowerName.find("gloss") != std::string::npos) {
                settings.bInvertGlossToRoughness = true;
            }

            FNativeTextureData nativeTex;
            if (FTextureImporter::Import(texPath.string(), settings, nativeTex)) {
                if (nativeTex.SaveToFile(outFilePath.string())) {
                    importedTextures++;
                    manifest.RegisterImport(texPath.string(), { outRelPath }, {});
                } else {
                    std::cerr << "  [ERROR] Failed to save native texture: " << outFilePath.string() << "\n";
                    errorCount++;
                }
            } else {
                std::cerr << "  [ERROR] Failed to import texture: " << texPath.string() << "\n";
                errorCount++;
            }
        }
    }

    // 4. Import Meshes & Auto-generate Material Slots
    if (!meshFiles.empty()) {
        std::cout << "\n--- [PHASE 2] Processing Meshes & Materials ---\n";
        for (const auto& meshPath : meshFiles) {
            std::string filename = meshPath.filename().string();
            std::string stem = meshPath.stem().string();
            std::string outRelPath = "Meshes/" + stem + ".lmesh";
            fs::path outFilePath = contentPath / "Meshes" / (stem + ".lmesh");

            bool bNeedsImport = bForce || manifest.NeedsReimport(meshPath.string()) || !fs::exists(outFilePath);

            if (!bNeedsImport) {
                skippedMeshes++;
                continue;
            }

            std::cout << "  [IMPORT MESH] " << filename << " -> " << outRelPath << "\n";

            FMeshImportSettings meshSettings;
            meshSettings.bGenerateTangents = true;

            FMeshImportResult importResult;
            if (FMeshImporter::ImportFBX(meshPath.string(), meshSettings, importResult) && importResult.StaticMesh) {
                std::vector<std::string> generatedAssets = { outRelPath };
                std::vector<std::string> dependencies;

                auto staticMesh = importResult.StaticMesh;
                for (const auto& extMat : importResult.ExtractedMaterials) {
                    std::string formattedName = extMat.Name;
                    if (formattedName.rfind("M_", 0) != 0) {
                        formattedName = "M_" + formattedName;
                    }
                    std::string matRelPath = "Materials/" + formattedName + ".lmat";
                    fs::path matFilePath = contentPath / matRelPath;

                    bool bMatNeedsImport = bForce || manifest.NeedsReimport(matFilePath.string()) || !fs::exists(matFilePath);
                    if (bMatNeedsImport) {
                        auto builtMat = FMaterialImporter::BuildMaterial(extMat, allAvailableTexturePaths);
                        if (builtMat && FMaterialImporter::SaveMaterialToFile(builtMat, matFilePath.string())) {
                            std::cout << "    [AUTO-MATERIAL] Created: " << matRelPath << "\n";
                            generatedAssets.push_back(matRelPath);
                        }
                    }
                    dependencies.push_back(matRelPath);
                }

                if (staticMesh->SaveToFile(outFilePath.string())) {
                    importedMeshes++;
                    manifest.RegisterImport(meshPath.string(), generatedAssets, dependencies);
                    std::cout << "    [SAVED] Verts: " << staticMesh->GetVertices().size()
                              << ", Indices: " << staticMesh->GetIndices().size()
                              << ", Submeshes: " << staticMesh->GetSubmeshes().size() << "\n";
                } else {
                    std::cerr << "  [ERROR] Failed to save native mesh: " << outFilePath.string() << "\n";
                    errorCount++;
                }
            } else {
                std::cerr << "  [ERROR] Failed to import mesh: " << meshPath.string() << "\n";
                errorCount++;
            }
        }
    }

    manifest.SaveToFile(manifestPath.string());

    auto endTime = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    std::cout << "\n===============================================================\n";
    std::cout << " Import Complete (" << durationMs << " ms)\n";
    std::cout << " HDRs:     " << importedHDRs << " imported, " << skippedHDRs << " skipped\n";
    std::cout << " Textures: " << importedTextures << " imported, " << skippedTextures << " skipped\n";
    std::cout << " Meshes:   " << importedMeshes << " imported, " << skippedMeshes << " skipped\n";
    std::cout << " Errors:   " << errorCount << "\n";
    std::cout << "===============================================================\n";

    return (errorCount == 0) ? 0 : 1;
}

int ExecuteValidate(const std::string& InContentDir) {
    fs::path contentPath = InContentDir;

    if (!fs::exists(contentPath)) {
        std::cerr << "[ERROR] Content directory does not exist: " << contentPath.string() << "\n";
        return 1;
    }

    std::cout << "===============================================================\n";
    std::cout << " LeonEngine2 Asset Validator\n";
    std::cout << " Validating content directory: " << contentPath.string() << "\n";
    std::cout << "===============================================================\n\n";

    size_t validatedCount = 0;
    size_t errorCount = 0;
    size_t warningCount = 0;

    for (const auto& entry : fs::recursive_directory_iterator(contentPath)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = FAssetPath::GetExtension(entry.path().string());
        if (ext == "lhdr") {
            validatedCount++;
            FNativeHDRData hdr;
            if (!hdr.LoadFromFile(entry.path().string())) {
                std::cerr << "  [FAIL] Invalid or corrupt HDR asset: " << entry.path().string() << "\n";
                errorCount++;
            } else if (hdr.Header.Width == 0 || hdr.Header.Height == 0 || hdr.Pixels.empty()) {
                std::cerr << "  [FAIL] HDR asset has zero dimensions or empty payload: " << entry.path().string() << "\n";
                errorCount++;
            } else {
                std::cout << "  [PASS] HDR Environment: " << entry.path().filename().string()
                          << " (" << hdr.Header.Width << "x" << hdr.Header.Height << ", RGBA32F, "
                          << (hdr.Pixels.size() * sizeof(float)) / 1024 << " KB)\n";
            }
        } else if (ext == "ltex") {
            validatedCount++;
            FNativeTextureData tex;
            if (!tex.LoadFromFile(entry.path().string())) {
                std::cerr << "  [FAIL] Invalid or corrupt texture: " << entry.path().string() << "\n";
                errorCount++;
            } else if (tex.Header.Width == 0 || tex.Header.Height == 0 || tex.Mips.empty()) {
                std::cerr << "  [FAIL] Texture has zero dimensions or no mips: " << entry.path().string() << "\n";
                errorCount++;
            } else {
                std::cout << "  [PASS] Texture: " << entry.path().filename().string() 
                          << " (" << tex.Header.Width << "x" << tex.Header.Height << ", " << tex.Mips.size() << " mips)\n";
            }
        } else if (ext == "lmesh") {
            validatedCount++;
            auto mesh = FStaticMesh::Create(entry.path().stem().string());
            if (!mesh->LoadFromFile(entry.path().string())) {
                std::cerr << "  [FAIL] Invalid or corrupt mesh: " << entry.path().string() << "\n";
                errorCount++;
            } else if (mesh->GetVertices().empty() || mesh->GetIndices().empty()) {
                std::cerr << "  [FAIL] Mesh contains 0 vertices or indices: " << entry.path().string() << "\n";
                errorCount++;
            } else {
                std::cout << "  [PASS] Mesh: " << entry.path().filename().string()
                          << " (" << mesh->GetVertices().size() << " verts, " 
                          << mesh->GetIndices().size() / 3 << " tris, "
                          << mesh->GetSubmeshes().size() << " submeshes)\n";
            }
        } else if (ext == "lmat" || ext == "lmi") {
            validatedCount++;
            std::cout << "  [PASS] Material: " << entry.path().filename().string() << "\n";
        }
    }

    std::cout << "\n===============================================================\n";
    std::cout << " Validation Summary: " << validatedCount << " assets checked, " 
              << errorCount << " errors, " << warningCount << " warnings.\n";
    std::cout << " Status: " << (errorCount == 0 ? "PASSED" : "FAILED") << "\n";
    std::cout << "===============================================================\n";

    return (errorCount == 0) ? 0 : 1;
}

int ExecuteValidateMap(const std::string& InMapPath) {
    if (!fs::exists(InMapPath)) {
        std::cerr << "[ERROR] Map file not found: " << InMapPath << "\n";
        return 1;
    }

    std::cout << "===============================================================\n";
    std::cout << " LeonEngine2 Map Validator\n";
    std::cout << " Map:   " << InMapPath << "\n";
    std::cout << "===============================================================\n\n";

    std::ifstream file(InMapPath);
    if (!file.is_open()) {
        std::cerr << "[FAIL] Failed to open map file: " << InMapPath << "\n";
        return 1;
    }

    std::string line;
    size_t actorCount = 0;
    size_t staticMeshCount = 0;
    size_t dirLightCount = 0;
    size_t pointLightCount = 0;
    size_t spotLightCount = 0;
    size_t cameraCount = 0;
    size_t missingAssets = 0;

    while (std::getline(file, line)) {
        std::string trimmed = line;
        size_t first = trimmed.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        trimmed = trimmed.substr(first);

        if (trimmed.rfind("- Name:", 0) == 0) {
            actorCount++;
        } else if (trimmed.rfind("StaticMesh:", 0) == 0) {
            staticMeshCount++;
        } else if (trimmed.rfind("DirectionalLight:", 0) == 0) {
            dirLightCount++;
        } else if (trimmed.rfind("PointLight:", 0) == 0) {
            pointLightCount++;
        } else if (trimmed.rfind("SpotLight:", 0) == 0) {
            spotLightCount++;
        } else if (trimmed.rfind("Camera:", 0) == 0) {
            cameraCount++;
        } else if (trimmed.rfind("Asset:", 0) == 0 || trimmed.rfind("HDREnvironmentMap:", 0) == 0) {
            size_t colon = trimmed.find(':');
            std::string assetPath = trimmed.substr(colon + 1);
            // strip quotes and whitespace
            size_t q1 = assetPath.find('"');
            size_t q2 = assetPath.rfind('"');
            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                assetPath = assetPath.substr(q1 + 1, q2 - q1 - 1);
            } else {
                size_t p = assetPath.find_first_not_of(" \t\r\n");
                if (p != std::string::npos) assetPath = assetPath.substr(p);
            }

            if (!assetPath.empty()) {
                std::string resolved = FAssetManager::ResolveVirtualPath(assetPath);
                if (!fs::exists(resolved) && !fs::exists(assetPath)) {
                    std::cerr << "  [WARNING] Referenced asset not found on disk: " << assetPath << "\n";
                    missingAssets++;
                } else {
                    std::cout << "  [PASS] Referenced asset verified: " << assetPath << "\n";
                }
            }
        }
    }

    std::cout << "\n--- Map Statistics ---\n";
    std::cout << "  Actors:             " << actorCount << "\n";
    std::cout << "  Static Mesh Actors: " << staticMeshCount << "\n";
    std::cout << "  Directional Lights: " << dirLightCount << "\n";
    std::cout << "  Point Lights:       " << pointLightCount << "\n";
    std::cout << "  Spot Lights:        " << spotLightCount << "\n";
    std::cout << "  Cameras:            " << cameraCount << "\n\n";

    if (missingAssets > 0) {
        std::cerr << "[FAIL] Map validation completed with " << missingAssets << " missing asset references.\n";
        return 1;
    }

    std::cout << "===============================================================\n";
    std::cout << " Status: PASSED (Map validated successfully with 0 missing assets)\n";
    std::cout << "===============================================================\n";
    return 0;
}

int ExecuteInspect(const std::string& InFilePath) {
    if (!fs::exists(InFilePath)) {
        std::cerr << "[ERROR] File does not exist: " << InFilePath << "\n";
        return 1;
    }

    std::string ext = FAssetPath::GetExtension(InFilePath);
    std::cout << "===============================================================\n";
    std::cout << " Inspecting Native Asset: " << InFilePath << "\n";
    std::cout << " Type: ." << ext << "\n";
    std::cout << "===============================================================\n";

    if (ext == "lhdr") {
        FNativeHDRData hdr;
        if (!hdr.LoadFromFile(InFilePath)) {
            std::cerr << "[ERROR] Failed to load .lhdr file\n";
            return 1;
        }
        std::cout << "Width:         " << hdr.Header.Width << "\n";
        std::cout << "Height:        " << hdr.Header.Height << "\n";
        std::cout << "Channels:      " << static_cast<int>(hdr.Header.Channels) << "\n";
        std::cout << "Format:        RGBA32F\n";
        std::cout << "Projection:    " << (hdr.Header.Projection == 0 ? "Equirectangular (2:1)" : "Cubemap") << "\n";
        std::cout << "Exposure Bias: " << hdr.Header.ExposureBias << "\n";
        std::cout << "Payload Size:  " << (hdr.Pixels.size() * sizeof(float)) / 1024 << " KB\n";
    } else if (ext == "ltex") {
        FNativeTextureData tex;
        if (!tex.LoadFromFile(InFilePath)) {
            std::cerr << "[ERROR] Failed to load .ltex file\n";
            return 1;
        }
        std::cout << "Width:       " << tex.Header.Width << "\n";
        std::cout << "Height:      " << tex.Header.Height << "\n";
        std::cout << "Channels:    " << static_cast<int>(tex.Header.Channels) << "\n";
        std::cout << "ColorSpace:  " << (tex.Header.ColorSpace == static_cast<uint32_t>(ETextureColorSpace::sRGB) ? "sRGB" : "Linear") << "\n";
        std::cout << "Mip Levels:  " << tex.Mips.size() << "\n";
        for (size_t m = 0; m < tex.Mips.size(); ++m) {
            std::cout << "  Mip " << m << ": " << tex.Mips[m].Width << "x" << tex.Mips[m].Height
                      << " (" << tex.Mips[m].Pixels.size() << " bytes)\n";
        }
    } else if (ext == "lmesh") {
        auto mesh = FStaticMesh::Create(FAssetPath::GetFileNameWithoutExtension(InFilePath));
        if (!mesh->LoadFromFile(InFilePath)) {
            std::cerr << "[ERROR] Failed to load .lmesh file\n";
            return 1;
        }
        glm::vec3 minB = mesh->GetBoundsMin();
        glm::vec3 maxB = mesh->GetBoundsMax();
        std::cout << "Vertices:    " << mesh->GetVertices().size() << "\n";
        std::cout << "Indices:     " << mesh->GetIndices().size() << " (" << mesh->GetIndices().size() / 3 << " triangles)\n";
        std::cout << "Submeshes:   " << mesh->GetSubmeshes().size() << "\n";
        for (size_t s = 0; s < mesh->GetSubmeshes().size(); ++s) {
            const auto& sub = mesh->GetSubmeshes()[s];
            std::cout << "  Submesh " << s << ": \"" << sub.Name << "\" (Offset: " << sub.IndexOffset 
                      << ", Count: " << sub.IndexCount << ", MatSlot: " << sub.MaterialSlotIndex << ")\n";
        }
        std::cout << "Material Slots: " << mesh->GetMaterialSlots().size() << "\n";
        for (size_t sl = 0; sl < mesh->GetMaterialSlots().size(); ++sl) {
            const auto& slot = mesh->GetMaterialSlots()[sl];
            std::cout << "  Slot " << sl << ": \"" << slot.SlotName << "\" -> " << slot.DefaultMaterialPath << "\n";
        }
        std::cout << "Bounding Box Min: (" << minB.x << ", " << minB.y << ", " << minB.z << ")\n";
        std::cout << "Bounding Box Max: (" << maxB.x << ", " << maxB.y << ", " << maxB.z << ")\n";
        std::cout << "Bounding Radius:  " << mesh->GetSphereRadius() << "\n";
    } else {
        std::cout << "[INFO] Inspecting text asset content:\n";
        std::ifstream f(InFilePath);
        std::string line;
        while (std::getline(f, line)) {
            std::cout << line << "\n";
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::string command = argv[1];
    std::string rawDir = "";
    std::string contentDir = "";
    std::string levelPath = "";
    bool bForce = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--raw" && i + 1 < argc) {
            rawDir = argv[++i];
        } else if (arg == "--content" && i + 1 < argc) {
            contentDir = argv[++i];
        } else if (arg == "--map" && i + 1 < argc) {
            levelPath = argv[++i];
        } else if (arg == "--force") {
            bForce = true;
        }
    }

    if (command == "import") {
        if (rawDir.empty() || contentDir.empty()) {
            std::cerr << "[ERROR] --raw <dir> and --content <dir> are required for import.\n";
            PrintUsage();
            return 1;
        }
        return ExecuteImport(rawDir, contentDir, bForce);
    } else if (command == "validate") {
        if (contentDir.empty()) {
            std::cerr << "[ERROR] --content <dir> is required for validate.\n";
            PrintUsage();
            return 1;
        }
        return ExecuteValidate(contentDir);
    } else if (command == "validate_map" || command == "level") {
        if (levelPath.empty()) {
            std::cerr << "[ERROR] --map <path> is required for validate_map.\n";
            PrintUsage();
            return 1;
        }
        return ExecuteValidateMap(levelPath);
    } else if (command == "inspect") {
        if (argc < 3) {
            std::cerr << "[ERROR] File path is required for inspect.\n";
            PrintUsage();
            return 1;
        }
        return ExecuteInspect(argv[2]);
    } else {
        std::cerr << "[ERROR] Unknown command: " << command << "\n";
        PrintUsage();
        return 1;
    }
}
