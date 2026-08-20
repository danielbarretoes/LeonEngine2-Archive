#include "LeonAssetTool/Commands/InspectCommand.hpp"
#include "Assets/FAssetPath.hpp"
#include "Assets/FHDRImporter.hpp"
#include "Assets/FTextureImporter.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Assets/USkeleton.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/UBlendSpace.hpp"
#include "Assets/FLightmapAsset.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace Leon::Tools {

    void FInspectCommand::PrintHelp() const {
        std::cout << "Usage: " << GetUsage() << "\n\n";
        std::cout << "Supported Formats:\n";
        std::cout << "  .lhdr          HDR environment map\n";
        std::cout << "  .ltex          Native mipmapped 2D texture\n";
        std::cout << "  .lmesh         Static mesh geometry & material slots\n";
        std::cout << "  .lskeleton     Bone hierarchy & bind poses\n";
        std::cout << "  .lskeletalmesh Skinned mesh vertices & weights\n";
        std::cout << "  .lanim         Keyframed skeletal animation sequence\n";
        std::cout << "  .lblend        Directional blend space\n";
        std::cout << "  .llightmap     Baked surface radiometry lightmap\n";
        std::cout << "  .lmat / .lmi   PBR material / material instance\n";
    }

    int FInspectCommand::Execute(const FCommandArgs& InArgs) {
        std::string filePath = InArgs.GetOption("file");
        if (filePath.empty() && !InArgs.PositionalArgs.empty()) {
            filePath = InArgs.PositionalArgs[0];
        }

        if (filePath.empty()) {
            std::cerr << "[ERROR] File path is required for inspect.\n\n";
            PrintHelp();
            return 1;
        }

        if (!fs::exists(filePath)) {
            std::cerr << "[ERROR] File does not exist: " << filePath << "\n";
            return 1;
        }

        std::string ext = FAssetPath::GetExtension(filePath);
        std::cout << "===============================================================\n";
        std::cout << " Inspecting Native Asset: " << filePath << "\n";
        std::cout << " Type: ." << ext << "\n";
        std::cout << "===============================================================\n";

        if (ext == "lhdr") {
            FNativeHDRData hdr;
            if (!hdr.LoadFromFile(filePath)) {
                std::cerr << "[ERROR] Failed to load .lhdr file\n";
                return 1;
            }
            std::cout << "Width:         " << hdr.Header.Width << "\n";
            std::cout << "Height:        " << hdr.Header.Height << "\n";
            std::cout << "Channels:      " << static_cast<int>(hdr.Header.Channels) << "\n";
            std::cout << "Format:        RGBA32F\n";
            std::cout << "Projection:    " << (hdr.Header.Projection == 0 ? "Equirectangular (2:1)" : "Cubemap")
                      << "\n";
            std::cout << "Exposure Bias: " << hdr.Header.ExposureBias << "\n";
            std::cout << "Payload Size:  " << (hdr.Pixels.size() * sizeof(float)) / 1024 << " KB\n";
        } else if (ext == "ltex") {
            FNativeTextureData tex;
            if (!tex.LoadFromFile(filePath)) {
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
            auto mesh = UStaticMesh::Create(FAssetPath::GetFileNameWithoutExtension(filePath));
            if (!mesh->LoadFromFile(filePath)) {
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
            auto skeleton = USkeleton::Create(FAssetPath::GetFileNameWithoutExtension(filePath));
            if (!skeleton->LoadFromFile(filePath)) {
                std::cerr << "[ERROR] Failed to load .lskeleton file\n";
                return 1;
            }
            std::cout << "Bones: " << skeleton->GetNumBones() << "\n";
            for (uint32_t i = 0; i < skeleton->GetNumBones(); ++i) {
                const auto& b = skeleton->GetBones()[i];
                std::cout << "  [" << i << "] " << b.Name << " parent=" << b.ParentIndex << "\n";
            }
        } else if (ext == "lskeletalmesh") {
            auto mesh = USkeletalMesh::Create(FAssetPath::GetFileNameWithoutExtension(filePath));
            if (!mesh->LoadFromFile(filePath)) {
                std::cerr << "[ERROR] Failed to load .lskeletalmesh file\n";
                return 1;
            }
            std::cout << "Vertices:  " << mesh->GetVertices().size() << "\n";
            std::cout << "Indices:   " << mesh->GetIndices().size() << "\n";
            std::cout << "Submeshes: " << mesh->GetSubmeshes().size() << "\n";
            std::cout << "Skeleton:  " << mesh->GetSkeletonPath() << "\n";
        } else if (ext == "lanim") {
            auto anim = UAnimSequence::Create(FAssetPath::GetFileNameWithoutExtension(filePath));
            if (!anim->LoadFromFile(filePath)) {
                std::cerr << "[ERROR] Failed to load .lanim file\n";
                return 1;
            }
            std::cout << "Duration: " << anim->GetDuration() << "s\n";
            std::cout << "Tracks:   " << anim->GetTracks().size() << "\n";
            std::cout << "Skeleton: " << anim->GetSkeletonPath() << "\n";
        } else if (ext == "lblend") {
            auto blend = UBlendSpace::Create(FAssetPath::GetFileNameWithoutExtension(filePath));
            if (!blend->LoadFromFile(filePath)) {
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
            if (!lm.LoadFromFile(filePath)) {
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
            std::ifstream f(filePath);
            std::string line;
            while (std::getline(f, line)) {
                std::cout << line << "\n";
            }
        }

        return 0;
    }

} // namespace Leon::Tools
