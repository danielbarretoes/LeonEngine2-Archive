#pragma once

#include <cctype>
#include <filesystem>
#include <string>

namespace leon::editor {

/// Map import sources (.obj / .fbx / .gltf / .glb) to the cooked sibling `.lmesh` when present.
/// Already-`.lmesh` paths are returned unchanged. Empty if nothing loadable exists.
[[nodiscard]] inline std::string ResolveCookedStaticMeshPath(const std::string& path) {
    namespace fs = std::filesystem;
    if (path.empty()) {
        return {};
    }
    const fs::path p(path);
    std::string ext = p.extension().string();
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (ext == ".lmesh") {
        return p.generic_string();
    }
    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") {
        const fs::path cooked = p.parent_path() / (p.stem().string() + ".lmesh");
        std::error_code ec;
        if (fs::exists(cooked, ec) && !ec) {
            return cooked.generic_string();
        }
        return {};
    }
    return {};
}

[[nodiscard]] inline bool IsStaticMeshSourceExt(const std::string& extLower) {
    return extLower == ".obj" || extLower == ".fbx" || extLower == ".gltf" || extLower == ".glb";
}

} // namespace leon::editor
