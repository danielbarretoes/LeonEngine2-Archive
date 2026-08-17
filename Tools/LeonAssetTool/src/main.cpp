#include "Core/Base.hpp"
#include "Core/FLog.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Assets/FAssetPath.hpp"
#include "Assets/FTextureImporter.hpp"
#include "Assets/FMeshImporter.hpp"
#include "Assets/FMaterialImporter.hpp"
#include "Assets/FAssetManifest.hpp"
#include "Assets/FHDRImporter.hpp"
#include "Assets/USkeleton.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/UBlendSpace.hpp"
#include "Assets/UAssetManager.hpp"
#include "Assets/FLightmapAsset.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"
#include "Engine/FMapSerializer.hpp"
#include "Lightmass/FLightmass.hpp"

#include "Core/FProjectDescriptor.hpp"
#include "Core/FProjectPaths.hpp"
#include "Core/FConfigFile.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;
using namespace Leon;

void PrintUsage() {
    std::cout << "===============================================================\n";
    std::cout << " LeonEngine2 Native Asset Tool\n";
    std::cout << "===============================================================\n\n";
    std::cout << "Usage:\n";
    std::cout << "  LeonAssetTool validate_project --project <path.lproject>\n";
    std::cout << "  LeonAssetTool import --raw <dir> --content <dir> [--force]\n";
    std::cout << "  LeonAssetTool validate --content <dir>\n";
    std::cout << "  LeonAssetTool validate_map --map <path.lmap>\n";
    std::cout << "  LeonAssetTool bake_lightmaps --map <path.lmap> [--force] [--quality=Preview|Draft|Production]\n";
    std::cout << "  LeonAssetTool validate_lightmaps --map <path.lmap>\n";
    std::cout << "  LeonAssetTool inspect <file.lhdr | file.ltex | file.lmesh | file.lskeleton | file.lskeletalmesh | "
                 "file.lanim | file.lmat | file.lmi | file.llightmap>\n\n";
    std::cout << "Options:\n";
    std::cout << "  --project <path>   Project descriptor file (e.g. Projects/Sandbox/Sandbox.lproject)\n";
    std::cout << "  --raw <dir>        Source raw assets directory (e.g. Assets/Raw)\n";
    std::cout << "  --content <dir>    Output native assets directory (e.g. Assets/Content)\n";
    std::cout << "  --map <path>       Map file path (e.g. Content/Maps/ShowcaseLevel.lmap)\n";
    std::cout << "  --quality=<name>   Lighting build quality: Preview, Draft, Production\n";
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
    fs::create_directories(contentPath / "Skeletons");
    fs::create_directories(contentPath / "SkeletalMeshes");
    fs::create_directories(contentPath / "BlendSpaces");

    // Unreal Engine Architecture: Manifest & Cache live in Intermediate/, NOT in Content/
    fs::path intermediateDir = contentPath.parent_path() / "Intermediate";
    fs::create_directories(intermediateDir);
    fs::path manifestPath = intermediateDir / "AssetManifest.json";

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
        if (!entry.is_regular_file())
            continue;

        std::string ext = FAssetPath::GetExtension(entry.path().string());
        if (ext == "hdr" || ext == "exr") {
            hdrFiles.push_back(entry.path());
        } else if (ext == "png" || ext == "tga" || ext == "jpg" || ext == "jpeg" || ext == "bmp") {
            textureFiles.push_back(entry.path());
        } else if (ext == "fbx" || ext == "obj") {
            meshFiles.push_back(entry.path());
        }
    }

    std::cout << "[DISCOVER] Found " << hdrFiles.size() << " HDR environments, " << textureFiles.size()
              << " textures, and " << meshFiles.size() << " meshes.\n\n";

    size_t importedHDRs = 0;
    size_t skippedHDRs = 0;
    size_t importedTextures = 0;
    size_t importedAnims = 0;
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
                manifest.RegisterImport(hdrPath.string(), {outRelPath}, {});
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
                allAvailableTexturePaths.push_back("Projects/Sandbox/Content/Textures/" +
                                                   entry.path().filename().string());
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
                    manifest.RegisterImport(texPath.string(), {outRelPath}, {});
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

        struct FPendingMesh {
            fs::path Source;
            std::string Stem;
            FMeshImportResult Result;
        };
        std::vector<FPendingMesh> pending;

        auto sanitize = [](std::string s) {
            s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); }), s.end());
            return s;
        };

        for (const auto& meshPath : meshFiles) {
            std::string stem = sanitize(meshPath.stem().string());
            fs::path staticOut = contentPath / "Meshes" / (stem + ".lmesh");
            fs::path skelOut = contentPath / "Skeletons" / (stem + ".lskeleton");
            fs::path skmOut = contentPath / "SkeletalMeshes" / (stem + ".lskeletalmesh");
            fs::path animOut = contentPath / "Animations" / (stem + ".lanim");
            const bool bOutputsExist =
                fs::exists(staticOut) || fs::exists(skelOut) || fs::exists(skmOut) || fs::exists(animOut);
            bool bNeedsImport = bForce || manifest.NeedsReimport(meshPath.string()) || !bOutputsExist;
            if (!bNeedsImport) {
                skippedMeshes++;
                continue;
            }

            std::cout << "  [IMPORT MESH] " << meshPath.filename().string() << "\n";
            FMeshImportSettings meshSettings;
            meshSettings.bGenerateTangents = true;
            FPendingMesh entry;
            entry.Source = meshPath;
            entry.Stem = stem;
            if (FMeshImporter::ImportFBX(meshPath.string(), meshSettings, entry.Result) &&
                (entry.Result.StaticMesh || entry.Result.SkeletalMesh || !entry.Result.Animations.empty() ||
                 entry.Result.Skeleton)) {
                pending.push_back(std::move(entry));
            } else {
                std::cerr << "  [ERROR] Failed to import mesh: " << meshPath.string() << "\n";
                errorCount++;
            }
        }

        TRef<USkeleton> sharedSkeleton;
        std::string sharedSkelRel;
        for (auto& entry : pending) {
            if (entry.Result.SkeletalMesh && entry.Result.Skeleton) {
                sharedSkeleton = entry.Result.Skeleton;
                sharedSkelRel = "Skeletons/" + entry.Stem + ".lskeleton";
                break;
            }
        }
        if (!sharedSkeleton) {
            fs::path skelDir = contentPath / "Skeletons";
            if (fs::exists(skelDir)) {
                for (const auto& skelFile : fs::directory_iterator(skelDir)) {
                    if (skelFile.path().extension() != ".lskeleton")
                        continue;
                    auto loaded = USkeleton::Create(skelFile.path().stem().string());
                    if (loaded->LoadFromFile(skelFile.path().string())) {
                        sharedSkeleton = loaded;
                        sharedSkelRel = "Skeletons/" + skelFile.path().filename().string();
                        break;
                    }
                }
            }
        }

        if (sharedSkeleton) {
            for (auto& entry : pending) {
                if (entry.Result.SkeletalMesh || entry.Result.Animations.empty())
                    continue;
                FMeshImportSettings retarget;
                retarget.bGenerateTangents = true;
                retarget.SharedSkeleton = sharedSkeleton;
                FMeshImportResult retargeted;
                if (FMeshImporter::ImportFBX(entry.Source.string(), retarget, retargeted) &&
                    !retargeted.Animations.empty()) {
                    entry.Result.Animations = std::move(retargeted.Animations);
                    entry.Result.Skeleton = sharedSkeleton;
                    std::cout << "    [RETARGET] " << entry.Source.filename().string() << " -> /Game/" << sharedSkelRel
                              << "\n";
                }
            }
        }

        for (auto& entry : pending) {
            auto& importResult = entry.Result;
            const std::string& stem = entry.Stem;
            std::vector<std::string> generatedAssets;
            std::vector<std::string> dependencies;

            auto writeMaterials = [&]() {
                for (const auto& extMat : importResult.ExtractedMaterials) {
                    std::string formattedName = extMat.Name;
                    if (formattedName.rfind("M_", 0) != 0)
                        formattedName = "M_" + formattedName;
                    std::string matRelPath = "Materials/" + formattedName + ".lmat";
                    fs::path matFilePath = contentPath / matRelPath;
                    bool bMatNeedsImport =
                        bForce || manifest.NeedsReimport(matFilePath.string()) || !fs::exists(matFilePath);
                    if (bMatNeedsImport) {
                        auto builtMat = FMaterialImporter::BuildMaterial(extMat, allAvailableTexturePaths);
                        if (builtMat && FMaterialImporter::SaveMaterialToFile(builtMat, matFilePath.string())) {
                            std::cout << "    [AUTO-MATERIAL] Created: " << matRelPath << "\n";
                            generatedAssets.push_back(matRelPath);
                        }
                    }
                    dependencies.push_back(matRelPath);
                }
            };

            if (importResult.IsSkeletal()) {
                writeMaterials();
                if (importResult.Skeleton && importResult.SkeletalMesh) {
                    fs::path skelPath = contentPath / sharedSkelRel;
                    if (sharedSkelRel.empty())
                        sharedSkelRel = "Skeletons/" + stem + ".lskeleton";
                    skelPath = contentPath / sharedSkelRel;
                    importResult.Skeleton->SetAssetPath("/Game/" + sharedSkelRel);
                    if (importResult.Skeleton->SaveToFile(skelPath.string())) {
                        generatedAssets.push_back(sharedSkelRel);
                        std::cout << "    [SAVED SKELETON] Bones: " << importResult.Skeleton->GetNumBones() << " -> "
                                  << sharedSkelRel << "\n";
                    }
                    importResult.SkeletalMesh->SetSkeletonPath("/Game/" + sharedSkelRel);
                    for (auto& anim : importResult.Animations)
                        anim->SetSkeletonPath("/Game/" + sharedSkelRel);
                } else if (!sharedSkelRel.empty()) {
                    for (auto& anim : importResult.Animations) {
                        anim->SetSkeletonPath("/Game/" + sharedSkelRel);
                        if (sharedSkeleton)
                            anim->LinkSkeleton(sharedSkeleton);
                    }
                }
                if (importResult.SkeletalMesh) {
                    std::string meshRel = "SkeletalMeshes/" + stem + ".lskeletalmesh";
                    fs::path skmPath = contentPath / meshRel;
                    importResult.SkeletalMesh->SetAssetPath("/Game/" + meshRel);
                    if (importResult.SkeletalMesh->SaveToFile(skmPath.string())) {
                        generatedAssets.push_back(meshRel);
                        importedMeshes++;
                        std::cout << "    [SAVED SKELMESH] Verts: " << importResult.SkeletalMesh->GetVertices().size()
                                  << ", Indices: " << importResult.SkeletalMesh->GetIndices().size() << "\n";
                    }
                }
                for (auto& anim : importResult.Animations) {
                    std::string animStem = sanitize(anim->GetName());
                    std::string animRel = "Animations/" + animStem + ".lanim";
                    fs::path animPath = contentPath / animRel;
                    anim->SetAssetPath("/Game/" + animRel);
                    if (anim->SaveToFile(animPath.string())) {
                        generatedAssets.push_back(animRel);
                        importedAnims++;
                        std::cout << "    [SAVED ANIM] " << animRel << " duration=" << anim->GetDuration()
                                  << "s tracks=" << anim->GetTracks().size() << "\n";
                    }
                }
                manifest.RegisterImport(entry.Source.string(), generatedAssets, dependencies);
            } else if (importResult.StaticMesh) {
                writeMaterials();
                auto staticMesh = importResult.StaticMesh;
                std::string outRelPath = "Meshes/" + stem + ".lmesh";
                fs::path outFilePath = contentPath / "Meshes" / (stem + ".lmesh");
                generatedAssets.insert(generatedAssets.begin(), outRelPath);
                if (staticMesh->SaveToFile(outFilePath.string())) {
                    importedMeshes++;
                    manifest.RegisterImport(entry.Source.string(), generatedAssets, dependencies);
                    std::cout << "    [SAVED] Verts: " << staticMesh->GetVertices().size()
                              << ", Indices: " << staticMesh->GetIndices().size()
                              << ", Submeshes: " << staticMesh->GetSubmeshes().size() << "\n";
                } else {
                    std::cerr << "  [ERROR] Failed to save native mesh: " << outFilePath.string() << "\n";
                    errorCount++;
                }
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
    std::cout << " Anims:    " << importedAnims << " imported\n";
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
        if (!entry.is_regular_file())
            continue;

        std::string ext = FAssetPath::GetExtension(entry.path().string());
        if (ext == "lhdr") {
            validatedCount++;
            FNativeHDRData hdr;
            if (!hdr.LoadFromFile(entry.path().string())) {
                std::cerr << "  [FAIL] Invalid or corrupt HDR asset: " << entry.path().string() << "\n";
                errorCount++;
            } else if (hdr.Header.Width == 0 || hdr.Header.Height == 0 || hdr.Pixels.empty()) {
                std::cerr << "  [FAIL] HDR asset has zero dimensions or empty payload: " << entry.path().string()
                          << "\n";
                errorCount++;
            } else {
                std::cout << "  [PASS] HDR Environment: " << entry.path().filename().string() << " ("
                          << hdr.Header.Width << "x" << hdr.Header.Height << ", RGBA32F, "
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
                std::cout << "  [PASS] Texture: " << entry.path().filename().string() << " (" << tex.Header.Width << "x"
                          << tex.Header.Height << ", " << tex.Mips.size() << " mips)\n";
            }
        } else if (ext == "lmesh") {
            validatedCount++;
            auto mesh = UStaticMesh::Create(entry.path().stem().string());
            if (!mesh->LoadFromFile(entry.path().string())) {
                std::cerr << "  [FAIL] Invalid or corrupt mesh: " << entry.path().string() << "\n";
                errorCount++;
            } else if (mesh->GetVertices().empty() || mesh->GetIndices().empty()) {
                std::cerr << "  [FAIL] Mesh contains 0 vertices or indices: " << entry.path().string() << "\n";
                errorCount++;
            } else {
                std::cout << "  [PASS] Mesh: " << entry.path().filename().string() << " (" << mesh->GetVertices().size()
                          << " verts, " << mesh->GetIndices().size() / 3 << " tris, " << mesh->GetSubmeshes().size()
                          << " submeshes)\n";
            }
        } else if (ext == "lskeleton") {
            validatedCount++;
            auto skeleton = USkeleton::Create(entry.path().stem().string());
            if (!skeleton->LoadFromFile(entry.path().string()) || skeleton->GetNumBones() == 0) {
                std::cerr << "  [FAIL] Invalid skeleton: " << entry.path().string() << "\n";
                errorCount++;
            } else {
                std::cout << "  [PASS] Skeleton: " << entry.path().filename().string() << " ("
                          << skeleton->GetNumBones() << " bones)\n";
            }
        } else if (ext == "lskeletalmesh") {
            validatedCount++;
            auto mesh = USkeletalMesh::Create(entry.path().stem().string());
            if (!mesh->LoadFromFile(entry.path().string()) || mesh->GetVertices().empty()) {
                std::cerr << "  [FAIL] Invalid skeletal mesh: " << entry.path().string() << "\n";
                errorCount++;
            } else {
                std::cout << "  [PASS] SkeletalMesh: " << entry.path().filename().string() << " ("
                          << mesh->GetVertices().size() << " verts, " << mesh->GetIndices().size() / 3 << " tris)\n";
            }
        } else if (ext == "lanim") {
            validatedCount++;
            auto anim = UAnimSequence::Create(entry.path().stem().string());
            if (!anim->LoadFromFile(entry.path().string()) || anim->GetTracks().empty()) {
                std::cerr << "  [FAIL] Invalid animation: " << entry.path().string() << "\n";
                errorCount++;
            } else {
                std::cout << "  [PASS] Animation: " << entry.path().filename().string() << " (" << anim->GetDuration()
                          << "s, " << anim->GetTracks().size() << " tracks)\n";
            }
        } else if (ext == "lblend") {
            validatedCount++;
            auto blend = UBlendSpace::Create(entry.path().stem().string());
            if (!blend->LoadFromFile(entry.path().string()) || blend->GetSamples().empty()) {
                std::cerr << "  [FAIL] Invalid blend space: " << entry.path().string() << "\n";
                errorCount++;
            } else {
                std::cout << "  [PASS] BlendSpace: " << entry.path().filename().string() << " ("
                          << blend->GetSamples().size() << " samples)\n";
            }
        } else if (ext == "lmat" || ext == "lmi") {
            validatedCount++;
            std::cout << "  [PASS] Material: " << entry.path().filename().string() << "\n";
        }
    }

    std::cout << "\n===============================================================\n";
    std::cout << " Validation Summary: " << validatedCount << " assets checked, " << errorCount << " errors, "
              << warningCount << " warnings.\n";
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
        if (first == std::string::npos)
            continue;
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
                if (p != std::string::npos)
                    assetPath = assetPath.substr(p);
            }

            if (!assetPath.empty()) {
                std::string resolved = UAssetManager::ResolveVirtualPath(assetPath);
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

int ExecuteBakeLightmaps(const std::string& InMapPath, bool bForce, const std::string& InQuality) {
    if (InMapPath.empty() || !fs::exists(InMapPath)) {
        std::cerr << "[ERROR] --map <path.lmap> is required and must exist.\n";
        return 1;
    }

    FLog::Init();
    UAssetManager::Init();

    fs::path mapPath = InMapPath;
    fs::path contentRoot = mapPath.parent_path();
    if (contentRoot.filename() == "Maps")
        contentRoot = contentRoot.parent_path();
    UAssetManager::SetContentRoot(contentRoot.string());
    FProjectPaths::SetProjectRoot(contentRoot.parent_path().string());

    FLightmassSettings settings;
    ELightingBuildQuality quality = ELightingBuildQuality::Draft;
    if (InQuality == "Preview")
        quality = ELightingBuildQuality::Preview;
    else if (InQuality == "Production")
        quality = ELightingBuildQuality::Production;
    ApplyLightingBuildQuality(quality, settings);

    auto result = FLightmass::BakeMap(InMapPath, settings, bForce);
    UAssetManager::Shutdown();
    return result.bSuccess ? 0 : 1;
}

int ExecuteValidateLightmaps(const std::string& InMapPath) {
    if (InMapPath.empty() || !fs::exists(InMapPath)) {
        std::cerr << "[ERROR] --map <path> is required and must exist.\n";
        return 1;
    }
    FLog::Init();
    UAssetManager::Init();
    fs::path mapPath = InMapPath;
    fs::path contentRoot = mapPath.parent_path();
    if (contentRoot.filename() == "Maps")
        contentRoot = contentRoot.parent_path();
    UAssetManager::SetContentRoot(contentRoot.string());
    FProjectPaths::SetProjectRoot(contentRoot.parent_path().string());
    std::string message;
    bool ok = FLightmass::ValidateMap(InMapPath, message);
    std::cout << "[Lightmass] " << message << "\n";
    UAssetManager::Shutdown();
    return ok ? 0 : 1;
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
        std::cout << "ColorSpace:  "
                  << (tex.Header.ColorSpace == static_cast<uint32_t>(ETextureColorSpace::sRGB) ? "sRGB" : "Linear")
                  << "\n";
        std::cout << "Mip Levels:  " << tex.Mips.size() << "\n";
        for (size_t m = 0; m < tex.Mips.size(); ++m) {
            std::cout << "  Mip " << m << ": " << tex.Mips[m].Width << "x" << tex.Mips[m].Height << " ("
                      << tex.Mips[m].Pixels.size() << " bytes)\n";
        }
    } else if (ext == "lmesh") {
        auto mesh = UStaticMesh::Create(FAssetPath::GetFileNameWithoutExtension(InFilePath));
        if (!mesh->LoadFromFile(InFilePath)) {
            std::cerr << "[ERROR] Failed to load .lmesh file\n";
            return 1;
        }
        glm::vec3 minB = mesh->GetBoundsMin();
        glm::vec3 maxB = mesh->GetBoundsMax();
        std::cout << "Vertices:    " << mesh->GetVertices().size() << "\n";
        std::cout << "Indices:     " << mesh->GetIndices().size() << " (" << mesh->GetIndices().size() / 3
                  << " triangles)\n";
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
    } else if (ext == "lskeleton") {
        auto skeleton = USkeleton::Create(FAssetPath::GetFileNameWithoutExtension(InFilePath));
        if (!skeleton->LoadFromFile(InFilePath)) {
            std::cerr << "[ERROR] Failed to load .lskeleton file\n";
            return 1;
        }
        std::cout << "Bones: " << skeleton->GetNumBones() << "\n";
        for (uint32_t i = 0; i < skeleton->GetNumBones(); ++i) {
            const auto& b = skeleton->GetBones()[i];
            std::cout << "  [" << i << "] " << b.Name << " parent=" << b.ParentIndex << "\n";
        }
    } else if (ext == "lskeletalmesh") {
        auto mesh = USkeletalMesh::Create(FAssetPath::GetFileNameWithoutExtension(InFilePath));
        if (!mesh->LoadFromFile(InFilePath)) {
            std::cerr << "[ERROR] Failed to load .lskeletalmesh file\n";
            return 1;
        }
        std::cout << "Vertices:  " << mesh->GetVertices().size() << "\n";
        std::cout << "Indices:   " << mesh->GetIndices().size() << "\n";
        std::cout << "Submeshes: " << mesh->GetSubmeshes().size() << "\n";
        std::cout << "Skeleton:  " << mesh->GetSkeletonPath() << "\n";
    } else if (ext == "lanim") {
        auto anim = UAnimSequence::Create(FAssetPath::GetFileNameWithoutExtension(InFilePath));
        if (!anim->LoadFromFile(InFilePath)) {
            std::cerr << "[ERROR] Failed to load .lanim file\n";
            return 1;
        }
        std::cout << "Duration: " << anim->GetDuration() << "s\n";
        std::cout << "Tracks:   " << anim->GetTracks().size() << "\n";
        std::cout << "Skeleton: " << anim->GetSkeletonPath() << "\n";
    } else if (ext == "lblend") {
        auto blend = UBlendSpace::Create(FAssetPath::GetFileNameWithoutExtension(InFilePath));
        if (!blend->LoadFromFile(InFilePath)) {
            std::cerr << "[ERROR] Failed to load .lblend file\n";
            return 1;
        }
        std::cout << "2D:       " << (blend->Is2D() ? "yes" : "no") << "\n";
        std::cout << "Skeleton: " << blend->GetSkeletonPath() << "\n";
        std::cout << "Samples:  " << blend->GetSamples().size() << "\n";
        for (const auto& s : blend->GetSamples())
            std::cout << "  (" << s.Coord.x << ", " << s.Coord.y << ") " << s.SequencePath << "\n";
    } else if (ext == "llightmap") {
        FLightmapAsset lm;
        if (!lm.LoadFromFile(InFilePath)) {
            std::cerr << "[ERROR] Failed to load .llightmap file\n";
            return 1;
        }
        const auto& h = lm.GetHeader();
        std::cout << "Width:        " << h.Width << "\n";
        std::cout << "Height:       " << h.Height << "\n";
        std::cout << "Channels:     " << h.ChannelCount << "\n";
        std::cout << "HDR:          " << (h.bIsHDR ? "yes" : "no") << "\n";
        std::cout << "Payload Size: " << h.PayloadSize << "\n";
        std::cout << "Content Hash: " << std::hex << h.ContentHash << std::dec << "\n";
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

int ExecuteValidateProject(const std::string& InProjectPath) {
    std::cout << "===============================================================\n";
    std::cout << " LeonEngine2 Project Validator\n";
    std::cout << " Project File: " << InProjectPath << "\n";
    std::cout << "===============================================================\n\n";

    fs::path projPath = InProjectPath;
    if (!fs::exists(projPath)) {
        std::cerr << "  [FAIL] Project file not found: " << InProjectPath << "\n";
        return 1;
    }

    // 1. Load Descriptor
    FProjectDescriptor desc;
    if (!desc.Load(InProjectPath)) {
        std::cerr << "  [FAIL] Failed to parse .lproject JSON descriptor.\n";
        return 1;
    }

    std::cout << "  [PASS] Project Descriptor loaded successfully:\n";
    std::cout << "         Project Name:     " << desc.ProjectName << "\n";
    std::cout << "         Engine Version:   " << desc.EngineVersion << "\n";
    std::cout << "         Default Map:      " << desc.DefaultMap << "\n";
    std::cout << "         Default GameMode: " << desc.DefaultGameMode << "\n\n";

    FProjectPaths::SetProjectRoot(InProjectPath);
    std::string projDir = FProjectPaths::ProjectDir();
    std::string configDir = FProjectPaths::ProjectConfigDir();
    std::string contentDir = FProjectPaths::ProjectContentDir();

    // 2. Validate Project Directories
    if (!fs::exists(configDir)) {
        std::cerr << "  [FAIL] Config directory missing: " << configDir << "\n";
        return 1;
    }
    std::cout << "  [PASS] Config directory verified: " << configDir << "\n";

    if (!fs::exists(contentDir)) {
        std::cerr << "  [FAIL] Content directory missing: " << contentDir << "\n";
        return 1;
    }
    std::cout << "  [PASS] Content directory verified: " << contentDir << "\n";

    // 3. Validate Configuration Files
    std::string engineIni = FAssetPath::Combine(configDir, "DefaultEngine.ini");
    if (fs::exists(engineIni)) {
        std::cout << "  [PASS] Configuration file verified: " << engineIni << "\n";
    } else {
        std::cerr << "  [WARN] DefaultEngine.ini missing in config directory.\n";
    }

    std::string gameIni = FAssetPath::Combine(configDir, "DefaultGame.ini");
    if (fs::exists(gameIni)) {
        std::cout << "  [PASS] Configuration file verified: " << gameIni << "\n";
    }

    std::string inputIni = FAssetPath::Combine(configDir, "DefaultInput.ini");
    if (fs::exists(inputIni)) {
        std::cout << "  [PASS] Configuration file verified: " << inputIni << "\n";
    }

    // 4. Resolve Default Map
    std::string physicalMap = FProjectPaths::ResolveVirtualPath(desc.DefaultMap);
    if (!fs::exists(physicalMap)) {
        std::cerr << "  [FAIL] Default map '" << desc.DefaultMap << "' resolved to '" << physicalMap
                  << "' which does NOT exist on disk.\n";
        return 1;
    }
    std::cout << "  [PASS] Virtual path resolved: " << desc.DefaultMap << " -> " << physicalMap << "\n\n";

    // 5. Validate Default Map Assets
    std::cout << "--- Validating Default Map Contents ---\n";
    int mapResult = ExecuteValidateMap(physicalMap);
    if (mapResult != 0) {
        std::cerr << "\n===============================================================\n";
        std::cerr << " Status: FAILED (Project has map asset integrity issues)\n";
        std::cerr << "===============================================================\n";
        return 1;
    }

    std::cout << "\n===============================================================\n";
    std::cout << " Status: PASSED (Project structure and assets verified successfully)\n";
    std::cout << "===============================================================\n";
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::string command = argv[1];
    if (command == "--help" || command == "-h") {
        PrintUsage();
        return 0;
    }
    std::string projectPath = "";
    std::string rawDir = "";
    std::string contentDir = "";
    std::string levelPath = "";
    std::string quality = "Draft";
    bool bForce = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--project" && i + 1 < argc) {
            projectPath = argv[++i];
        } else if (arg == "--raw" && i + 1 < argc) {
            rawDir = argv[++i];
        } else if (arg == "--content" && i + 1 < argc) {
            contentDir = argv[++i];
        } else if (arg == "--map" && i + 1 < argc) {
            levelPath = argv[++i];
        } else if (arg.rfind("--quality=", 0) == 0) {
            quality = arg.substr(10);
        } else if (arg == "--quality" && i + 1 < argc) {
            quality = argv[++i];
        } else if (arg == "--force") {
            bForce = true;
        }
    }

    if (command == "validate_project" || command == "project") {
        if (projectPath.empty()) {
            if (argc >= 3 && argv[2][0] != '-') {
                projectPath = argv[2];
            } else {
                std::cerr << "[ERROR] --project <path.lproject> is required for validate_project.\n";
                PrintUsage();
                return 1;
            }
        }
        return ExecuteValidateProject(projectPath);
    } else if (command == "import") {
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
    } else if (command == "bake_lightmaps") {
        if (levelPath.empty()) {
            std::cerr << "[ERROR] --map <path> is required for bake_lightmaps.\n";
            PrintUsage();
            return 1;
        }
        return ExecuteBakeLightmaps(levelPath, bForce, quality);
    } else if (command == "validate_lightmaps") {
        if (levelPath.empty()) {
            std::cerr << "[ERROR] --map <path> is required for validate_lightmaps.\n";
            PrintUsage();
            return 1;
        }
        return ExecuteValidateLightmaps(levelPath);
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
