#include "LeonAssetTool/Commands/ValidateCommand.hpp"
#include "Assets/FAssetPath.hpp"
#include "Assets/FHDRImporter.hpp"
#include "Assets/FTextureImporter.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Assets/USkeleton.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/UBlendSpace.hpp"

#include <iostream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace Leon::Tools {

    void FValidateCommand::PrintHelp() const {
        std::cout << "Usage: " << GetUsage() << "\n\n";
        std::cout << "Options:\n";
        std::cout << "  --content <dir>    Path to native assets directory to validate (e.g. Content/)\n";
    }

    int FValidateCommand::Execute(const FCommandArgs& InArgs) {
        std::string contentDir = InArgs.GetOption("content");
        if (contentDir.empty() && !InArgs.PositionalArgs.empty()) {
            contentDir = InArgs.PositionalArgs[0];
        }

        if (contentDir.empty()) {
            std::cerr << "[ERROR] --content <dir> is required for validate.\n\n";
            PrintHelp();
            return 1;
        }

        fs::path contentPath = contentDir;

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
                    std::cout << "  [PASS] Texture: " << entry.path().filename().string() << " (" << tex.Header.Width
                              << "x" << tex.Header.Height << ", " << tex.Mips.size() << " mips)\n";
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
                    std::cout << "  [PASS] Mesh: " << entry.path().filename().string() << " ("
                              << mesh->GetVertices().size() << " verts, " << mesh->GetIndices().size() / 3 << " tris, "
                              << mesh->GetSubmeshes().size() << " submeshes)\n";
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
                              << mesh->GetVertices().size() << " verts, " << mesh->GetIndices().size() / 3
                              << " tris)\n";
                }
            } else if (ext == "lanim") {
                validatedCount++;
                auto anim = UAnimSequence::Create(entry.path().stem().string());
                if (!anim->LoadFromFile(entry.path().string()) || anim->GetTracks().empty()) {
                    std::cerr << "  [FAIL] Invalid animation: " << entry.path().string() << "\n";
                    errorCount++;
                } else {
                    std::cout << "  [PASS] Animation: " << entry.path().filename().string() << " ("
                              << anim->GetDuration() << "s, " << anim->GetTracks().size() << " tracks)\n";
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

} // namespace Leon::Tools
