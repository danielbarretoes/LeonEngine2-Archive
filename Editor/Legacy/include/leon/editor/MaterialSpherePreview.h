#pragma once

#include <cstdint>
#include <leon/core/Camera.h>
#include <leon/editor/EditorViewportTarget.h>
#include <leon/level/Level.h>
#include <leon/render/Material.h>
#include <leon/render/StaticMesh.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace leon {
class Renderer;
class ResourceCache;
} // namespace leon

namespace leon::editor {

/// Small offscreen sphere used by Details (and similar) for material preview.
class MaterialSpherePreview {
public:
    void SetMaterialPath(ResourceCache& resources, const std::string& materialPath);
    /// Live preview from an already-resolved CPU material (Material Editor dirty docs).
    void SetMaterial(ResourceCache& resources, const Material& material);
    void Draw(Renderer& renderer, float width, float height);

    [[nodiscard]] const std::string& MaterialPath() const { return materialPath_; }

private:
    void EnsureScene(ResourceCache& resources);
    void ApplyMaterialToScene();

    EditorViewportTarget target_;
    Camera camera_{};
    Level previewLevel_{};
    std::shared_ptr<StaticMesh> sphere_;
    Material material_{};
    std::string materialPath_;
    std::uint64_t materialCacheEpoch_ = 0;
    bool ready_ = false;
};

/// Cached material sphere thumbnails for Content Browser gallery tiles.
class MaterialThumbnailCache {
public:
    /// Ensure a thumbnail exists. Returns GL color texture id, or 0 if still pending.
    /// `budget` limits how many new thumbs are rendered this call (decremented).
    [[nodiscard]] unsigned int Ensure(Renderer& renderer, ResourceCache& resources,
                                      const std::string& materialPath, int pixelSize, int& budget);

    void Clear();

private:
    struct Thumb {
        EditorViewportTarget target;
        bool ready = false;
    };

    void EnsureSharedScene(ResourceCache& resources);
    [[nodiscard]] bool RenderThumb(Renderer& renderer, ResourceCache& resources, Thumb& thumb,
                                   const std::string& materialPath, int pixelSize);

    Camera camera_{};
    Level previewLevel_{};
    std::shared_ptr<StaticMesh> sphere_;
    bool sceneReady_ = false;
    std::unordered_map<std::string, Thumb> thumbs_;
};

/// Cached full-mesh thumbnails for Content Browser gallery tiles (framed to AABB).
class MeshThumbnailCache {
public:
    [[nodiscard]] unsigned int Ensure(Renderer& renderer, ResourceCache& resources,
                                      const std::string& meshPath, int pixelSize, int& budget);

    void Clear();

private:
    struct Thumb {
        EditorViewportTarget target;
        bool ready = false;
    };

    [[nodiscard]] bool RenderThumb(Renderer& renderer, ResourceCache& resources, Thumb& thumb,
                                   const std::string& meshPath, int pixelSize);

    Camera camera_{};
    Level previewLevel_{};
    std::unordered_map<std::string, Thumb> thumbs_;
};

/// Cached 2D texture thumbnails (PNG/JPG/TGA/BMP) for Content Browser tiles.
class TextureThumbnailCache {
public:
    [[nodiscard]] unsigned int Ensure(ResourceCache& resources, const std::string& texturePath,
                                      int& budget);

    void Clear();

private:
    std::unordered_map<std::string, unsigned int> thumbs_;
};

/// Cached skinned-mesh thumbnails (.lskm / .lchar) for Content Browser tiles.
class SkeletalThumbnailCache {
public:
    [[nodiscard]] unsigned int Ensure(Renderer& renderer, ResourceCache& resources,
                                      const std::string& assetPath, int pixelSize, int& budget);

    void Clear();

private:
    struct Thumb {
        EditorViewportTarget target;
        bool ready = false;
        bool failed = false;
    };

    [[nodiscard]] bool RenderThumb(Renderer& renderer, ResourceCache& resources, Thumb& thumb,
                                   const std::string& assetPath, int pixelSize);

    Camera camera_{};
    Level previewLevel_{};
    std::unordered_map<std::string, Thumb> thumbs_;
};

} // namespace leon::editor
