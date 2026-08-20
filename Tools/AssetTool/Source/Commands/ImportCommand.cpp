#include "AssetTool/Commands/ImportCommand.hpp"
#include "Core/Base.hpp"
#include "Core/FLog.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Assets/FAssetPath.hpp"
#include "Assets/FTextureImporter.hpp"
#include "Assets/FMeshImporter.hpp"
#include "Assets/FMaterialImporter.hpp"
#include "Assets/FAssetManifest.hpp"
#include "Assets/FHDRImporter.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Assets/USkeleton.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/UBlendSpace.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace Leon::Tools {

    void FImportCommand::PrintHelp() const {
        std::cout << "Usage: " << GetUsage() << "\n\n";
        std::cout << "Options:\n";
        std::cout << "  --raw <dir>        Source raw assets directory (e.g. Assets/Raw)\n";
        std::cout << "  --content <dir>    Output native assets directory (e.g. Assets/Content)\n";
        std::cout << "  --force            Force re-importing all assets regardless of content hash\n";
    }

    int FImportCommand::Execute(const FCommandArgs& InArgs) {
        std::string rawDir = InArgs.GetOption("raw");
        std::string contentDir = InArgs.GetOption("content");
        bool bForce = InArgs.HasFlag("force");

        if (rawDir.empty() || contentDir.empty()) {
            std::cerr << "[ERROR] --raw <dir> and --content <dir> are required for import.\n\n";
            PrintHelp();
            return 1;
        }

        fs::path rawPath = rawDir;
        fs::path contentPath = contentDir;

        if (!fs::exists(rawPath)) {
            std::cerr << "[ERROR] Raw asset directory does not exist: " << rawPath.string() << "\n";
            return 1;
        }

        auto startTime = std::chrono::high_resolution_clock::now();

        fs::create_directories(contentPath / "HDR");
        fs::create_directories(contentPath / "Textures");
        fs::create_directories(contentPath / "Meshes");
        fs::create_directories(contentPath / "Materials");
        fs::create_directories(contentPath / "Skeletons");
        fs::create_directories(contentPath / "SkeletalMeshes");
        fs::create_directories(contentPath / "BlendSpaces");

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
                    (entry.Result.StaticMesh || !entry.Result.SeparateMeshes.empty() || entry.Result.SkeletalMesh ||
                     !entry.Result.Animations.empty() || entry.Result.Skeleton)) {
                    pending.push_back(std::move(entry));
                } else {
                    std::cerr << "  [ERROR] Failed to import mesh: " << meshPath.string() << "\n";
                    errorCount++;
                }
            }

            TRef<USkeleton> sharedSkeleton;
            std::string sharedSkelRel;
            std::string sharedOwnerStem;

            auto tryLoadSkeletonFile = [&](const fs::path& InPath, const std::string& InRel) -> bool {
                if (!fs::exists(InPath))
                    return false;
                auto loaded = USkeleton::Create(InPath.stem().string());
                if (!loaded->LoadFromFile(InPath.string()))
                    return false;
                sharedSkeleton = loaded;
                sharedSkelRel = InRel;
                sharedOwnerStem = InPath.stem().string();
                return true;
            };

            if (!tryLoadSkeletonFile(contentPath / "Skeletons" / "YBot.lskeleton", "Skeletons/YBot.lskeleton")) {
                for (auto& entry : pending) {
                    if (entry.Stem == "YBot" && entry.Result.SkeletalMesh && entry.Result.Skeleton) {
                        sharedSkeleton = entry.Result.Skeleton;
                        sharedSkelRel = "Skeletons/YBot.lskeleton";
                        sharedOwnerStem = "YBot";
                        break;
                    }
                }
            }
            if (!sharedSkeleton) {
                for (auto& entry : pending) {
                    if (entry.Result.SkeletalMesh && entry.Result.Skeleton) {
                        sharedSkeleton = entry.Result.Skeleton;
                        sharedSkelRel = "Skeletons/" + entry.Stem + ".lskeleton";
                        sharedOwnerStem = entry.Stem;
                        break;
                    }
                }
            }
            if (!sharedSkeleton) {
                fs::path skelDir = contentPath / "Skeletons";
                if (fs::exists(skelDir)) {
                    for (const auto& skelFile : fs::directory_iterator(skelDir)) {
                        if (skelFile.path().extension() != ".lskeleton")
                            continue;
                        if (tryLoadSkeletonFile(skelFile.path(), "Skeletons/" + skelFile.path().filename().string()))
                            break;
                    }
                }
            }

            if (sharedSkeleton) {
                for (auto& entry : pending) {
                    if (!entry.Result.SkeletalMesh && !entry.Result.Animations.empty()) {
                        FMeshImportSettings retarget;
                        retarget.bGenerateTangents = true;
                        retarget.SharedSkeleton = sharedSkeleton;
                        FMeshImportResult retargeted;
                        if (FMeshImporter::ImportFBX(entry.Source.string(), retarget, retargeted) &&
                            !retargeted.Animations.empty()) {
                            entry.Result.Animations = std::move(retargeted.Animations);
                            entry.Result.Skeleton = sharedSkeleton;
                            std::cout << "    [RETARGET ANIM] " << entry.Source.filename().string() << " -> /Game/"
                                      << sharedSkelRel << "\n";
                        }
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
                    if (importResult.SkeletalMesh && importResult.ExtractedMaterials.empty()) {
                        FExtractedMaterial synth;
                        synth.Name = stem;
                        importResult.ExtractedMaterials.push_back(synth);
                        auto& slots = importResult.SkeletalMesh->GetMaterialSlots();
                        if (slots.empty()) {
                            FSkeletalMaterialSlot slot;
                            slot.SlotName = stem;
                            slots.push_back(slot);
                        } else {
                            slots[0].SlotName = stem;
                        }
                        slots[0].DefaultMaterialPath = "Materials/M_" + stem + ".lmat";
                    }
                    writeMaterials();
                    if (importResult.Skeleton && importResult.SkeletalMesh) {
                        const std::string meshSkelRel = "Skeletons/" + stem + ".lskeleton";
                        fs::path skelPath = contentPath / meshSkelRel;
                        importResult.Skeleton->SetAssetPath("/Game/" + meshSkelRel);
                        if (importResult.Skeleton->SaveToFile(skelPath.string())) {
                            generatedAssets.push_back(meshSkelRel);
                            std::cout << "    [SAVED SKELETON] Bones: " << importResult.Skeleton->GetNumBones()
                                      << " -> " << meshSkelRel << "\n";
                        }
                        importResult.SkeletalMesh->SetSkeletonPath("/Game/" + meshSkelRel);
                        importResult.SkeletalMesh->SetSkeleton(importResult.Skeleton);
                        for (auto& anim : importResult.Animations)
                            anim->SetSkeletonPath("/Game/" + meshSkelRel);
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
                        importResult.SkeletalMesh->SetName(stem);
                        importResult.SkeletalMesh->SetAssetPath("/Game/" + meshRel);
                        if (importResult.SkeletalMesh->SaveToFile(skmPath.string())) {
                            generatedAssets.push_back(meshRel);
                            importedMeshes++;
                            std::cout << "    [SAVED SKELMESH] Verts: "
                                      << importResult.SkeletalMesh->GetVertices().size()
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
                } else if (importResult.StaticMesh || !importResult.SeparateMeshes.empty()) {
                    writeMaterials();
                    auto saveOne = [&](const TRef<UStaticMesh>& mesh, const std::string& fileStem) {
                        std::string outRelPath = "Meshes/" + fileStem + ".lmesh";
                        fs::path outFilePath = contentPath / "Meshes" / (fileStem + ".lmesh");
                        generatedAssets.push_back(outRelPath);
                        mesh->SetName(fileStem);
                        mesh->SetAssetPath("/Game/" + outRelPath);
                        if (mesh->SaveToFile(outFilePath.string())) {
                            importedMeshes++;
                            std::cout << "    [SAVED] " << outRelPath << " Verts: " << mesh->GetVertices().size()
                                      << ", Indices: " << mesh->GetIndices().size()
                                      << ", Submeshes: " << mesh->GetSubmeshes().size()
                                      << ", LODs: " << mesh->GetLODCount() << "\n";
                            return true;
                        }
                        std::cerr << "  [ERROR] Failed to save native mesh: " << outFilePath.string() << "\n";
                        errorCount++;
                        return false;
                    };
                    if (importResult.SeparateMeshes.size() > 1) {
                        for (auto& piece : importResult.SeparateMeshes)
                            saveOne(piece, sanitize(piece->GetName()));
                        fs::path partsPath = contentPath / "Meshes" / (stem + ".parts.json");
                        std::ofstream parts(partsPath);
                        parts << "{\n  \"stem\": \"" << stem << "\",\n  \"meshes\": [\n";
                        for (size_t i = 0; i < importResult.SeparateMeshes.size(); ++i) {
                            const auto& piece = importResult.SeparateMeshes[i];
                            const glm::vec3 mn = piece->GetBoundsMin();
                            const glm::vec3 mx = piece->GetBoundsMax();
                            std::string mat = "Materials/M_DefaultPBR.lmat";
                            if (!piece->GetMaterialSlots().empty() &&
                                !piece->GetMaterialSlots()[0].DefaultMaterialPath.empty())
                                mat = piece->GetMaterialSlots()[0].DefaultMaterialPath;
                            if (!mat.empty() && mat[0] != '/')
                                mat = "/Game/" + mat;
                            parts << "    {\"name\": \"" << piece->GetName() << "\", \"asset\": \""
                                  << piece->GetAssetPath() << "\", \"material\": \"" << mat << "\", \"min\": [" << mn.x
                                  << ", " << mn.y << ", " << mn.z << "], \"max\": [" << mx.x << ", " << mx.y << ", "
                                  << mx.z << "]}";
                            parts << (i + 1 < importResult.SeparateMeshes.size() ? ",\n" : "\n");
                        }
                        parts << "  ]\n}\n";
                        std::cout << "    [PARTS] " << partsPath.filename().string() << "\n";
                    } else if (importResult.StaticMesh) {
                        saveOne(importResult.StaticMesh, stem);
                    }
                    manifest.RegisterImport(entry.Source.string(), generatedAssets, dependencies);
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

} // namespace Leon::Tools
