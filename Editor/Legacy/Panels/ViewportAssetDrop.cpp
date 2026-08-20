#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <leon/content/LeonBlueprint.h>
#include <leon/core/Camera.h>
#include <leon/core/Paths.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorCommands.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/EngineContent.h>
#include <leon/editor/panels/ViewportPanel.h>
#include <leon/editor/StaticMeshAssetPath.h>
#include <leon/level/Level.h>
#include <leon/render/Frustum.h>
#include <limits>
#include <optional>
#include <string>

namespace leon::editor {

std::optional<std::size_t> ViewportPanel::PickStaticMeshUnderMouse(EditorContext& ctx) const {
    Camera* cam =
        (ctx.pieNewWindow && ctx.editorViewCamera != nullptr) ? ctx.editorViewCamera : ctx.camera;
    if (ctx.level == nullptr || cam == nullptr) {
        return std::nullopt;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    const float imageW = std::max(1.0f, itemMax.x - itemMin.x);
    const float imageH = std::max(1.0f, itemMax.y - itemMin.y);
    const float localX = mouse.x - itemMin.x;
    const float localY = mouse.y - itemMin.y;
    if (localX < 0.0f || localY < 0.0f || localX > imageW || localY > imageH) {
        return std::nullopt;
    }

    const float ndcX = ((localX / imageW) * 2.0f) - 1.0f;
    const float ndcY = 1.0f - ((localY / imageH) * 2.0f);
    const glm::mat4 invVP = glm::inverse(cam->ProjectionMatrix() * cam->ViewMatrix());
    glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farH = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearH.w) < 1.0e-8f || std::abs(farH.w) < 1.0e-8f) {
        return std::nullopt;
    }
    nearH /= nearH.w;
    farH /= farH.w;
    const glm::vec3 rayOrigin = glm::vec3(nearH);
    const glm::vec3 rayDir = glm::normalize(glm::vec3(farH) - rayOrigin);

    float bestT = std::numeric_limits<float>::max();
    std::optional<std::size_t> bestIndex;
    for (std::size_t i = 0; i < ctx.level->StaticMeshes().size(); ++i) {
        const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[i];
        if (mesh.hidden) {
            continue;
        }
        Aabb box;
        if (mesh.mesh != nullptr && mesh.mesh->Valid()) {
            box = Aabb::fromLocalTransformed(mesh.mesh->LocalMin(), mesh.mesh->LocalMax(),
                                             mesh.EffectiveModelMatrix());
        } else {
            const glm::vec3& p = mesh.transform.position;
            box = Aabb{p - glm::vec3(0.35f), p + glm::vec3(0.35f)};
        }
        float t = 0.0f;
        if (box.intersectRay(rayOrigin, rayDir, t) && t >= 0.0f && t < bestT) {
            bestT = t;
            bestIndex = i;
        }
    }
    return bestIndex;
}

void ViewportPanel::ClearMaterialDropPreview(EditorContext& ctx) {
    if (!materialPreviewActive_ || ctx.level == nullptr) {
        materialPreviewActive_ = false;
        materialPreviewPath_.clear();
        return;
    }
    if (materialPreviewMeshIndex_ < ctx.level->StaticMeshes().size()) {
        StaticMeshComponent& mesh = ctx.level->StaticMeshes()[materialPreviewMeshIndex_];
        mesh.material = materialPreviewSaved_;
        mesh.materials = materialPreviewSavedSlots_;
        mesh.materialOverride = materialPreviewSavedOverride_;
        mesh.materialPath = materialPreviewSavedPath_;
    }
    materialPreviewActive_ = false;
    materialPreviewPath_.clear();
    materialPreviewSavedSlots_.clear();
}

void ViewportPanel::ApplyMaterialDropPreview(EditorContext& ctx, std::size_t meshIndex,
                                             const std::string& materialPath,
                                             const Material& material) {
    if (ctx.level == nullptr || meshIndex >= ctx.level->StaticMeshes().size()) {
        return;
    }
    if (materialPreviewActive_ && materialPreviewMeshIndex_ == meshIndex &&
        materialPreviewPath_ == materialPath) {
        return;
    }
    if (materialPreviewActive_) {
        ClearMaterialDropPreview(ctx);
    }

    StaticMeshComponent& mesh = ctx.level->StaticMeshes()[meshIndex];
    materialPreviewSaved_ = mesh.material;
    materialPreviewSavedSlots_ = mesh.materials;
    materialPreviewSavedOverride_ = mesh.materialOverride;
    materialPreviewSavedPath_ = mesh.materialPath;
    materialPreviewMeshIndex_ = meshIndex;
    materialPreviewPath_ = materialPath;
    materialPreviewActive_ = true;

    // Clear per-slot materials so override is visible on every submesh during preview.
    mesh.materials.clear();
    mesh.material = material;
    mesh.materialOverride = true;
    mesh.materialPath = materialPath;
}

bool ViewportPanel::HandleMaterialDrag(EditorContext& ctx, const std::string& path,
                                       bool isDelivery) {
    if (ctx.resources == nullptr || ctx.level == nullptr) {
        return false;
    }

    std::string loadPath;
    std::string persistPath = path;
    if (path == "leon:Engine/Materials/M_Default") {
        persistPath = "Materials/M_Default.lmat";
        loadPath = ResolveAssetPath(persistPath);
    } else if (path == "leon:Engine/Materials/M_WorldGrid") {
        persistPath = "Materials/M_WorldGrid.lmat";
        loadPath = ResolveAssetPath(persistPath);
    } else {
        namespace fs = std::filesystem;
        std::string ext = fs::path(path).extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext != ".lmat") {
            if (isDelivery && ext == ".lmgraph") {
                EditorToast("Assign a Material Instance (.lmat), not a Material (.lmgraph). "
                            "Use Create Material Instance on the Material first.",
                            EEditorToastKind::Warning, 4.0f);
            }
            return false;
        }
        loadPath = path;
        persistPath = MakePackRelativeAssetPath(ctx, path);
        if (persistPath.empty()) {
            persistPath = path;
        }
    }

    Material material = ctx.resources->LoadMaterial(loadPath.empty() ? persistPath : loadPath);
    if (path == "leon:Engine/Materials/M_Default" && !material.albedoMap) {
        material = ctx.resources->DefaultMaterial();
    }

    const std::optional<std::size_t> hit = PickStaticMeshUnderMouse(ctx);
    if (!hit.has_value()) {
        ClearMaterialDropPreview(ctx);
        if (isDelivery) {
            // Fall back to current selection (previous behaviour).
            if (ctx.selection.kind == EEditorSelectionKind::StaticMesh &&
                ctx.selection.index < ctx.level->StaticMeshes().size()) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                StaticMeshComponent& mesh = ctx.level->StaticMeshes()[ctx.selection.index];
                mesh.materials.clear();
                mesh.material = material;
                mesh.materialOverride = true;
                mesh.materialPath = persistPath;
                ctx.MarkDirty();
            }
        }
        return true;
    }

    if (isDelivery) {
        // Commit: keep preview material (or apply fresh), capture undo from pre-preview state.
        if (materialPreviewActive_ && materialPreviewMeshIndex_ == *hit) {
            // Restore original into history snapshot, then re-apply commit.
            StaticMeshComponent& mesh = ctx.level->StaticMeshes()[*hit];
            const Material commitMat = mesh.material;
            const std::string commitPath = persistPath;
            mesh.material = materialPreviewSaved_;
            mesh.materials = materialPreviewSavedSlots_;
            mesh.materialOverride = materialPreviewSavedOverride_;
            mesh.materialPath = materialPreviewSavedPath_;
            materialPreviewActive_ = false;
            materialPreviewPath_.clear();
            if (ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            mesh.materials.clear();
            mesh.material = commitMat;
            mesh.materialOverride = true;
            mesh.materialPath = commitPath;
            ctx.Select(EEditorSelectionKind::StaticMesh, *hit);
            ctx.MarkDirty();
            return true;
        }
        ClearMaterialDropPreview(ctx);
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        StaticMeshComponent& mesh = ctx.level->StaticMeshes()[*hit];
        mesh.materials.clear();
        mesh.material = material;
        mesh.materialOverride = true;
        mesh.materialPath = persistPath;
        ctx.Select(EEditorSelectionKind::StaticMesh, *hit);
        ctx.MarkDirty();
        return true;
    }

    ApplyMaterialDropPreview(ctx, *hit, persistPath, material);
    return true;
}

bool ViewportPanel::WorldPointUnderMouse(EditorContext& ctx, glm::vec3& outPoint) const {
    Camera* cam =
        (ctx.pieNewWindow && ctx.editorViewCamera != nullptr) ? ctx.editorViewCamera : ctx.camera;
    if (cam == nullptr) {
        return false;
    }
    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    const float imageW = std::max(1.0f, itemMax.x - itemMin.x);
    const float imageH = std::max(1.0f, itemMax.y - itemMin.y);
    const float localX = mouse.x - itemMin.x;
    const float localY = mouse.y - itemMin.y;
    if (localX < 0.0f || localY < 0.0f || localX > imageW || localY > imageH) {
        return false;
    }

    const float ndcX = ((localX / imageW) * 2.0f) - 1.0f;
    const float ndcY = 1.0f - ((localY / imageH) * 2.0f);
    const glm::mat4 invVP = glm::inverse(cam->ProjectionMatrix() * cam->ViewMatrix());
    glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farH = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearH.w) < 1.0e-8f || std::abs(farH.w) < 1.0e-8f) {
        return false;
    }
    nearH /= nearH.w;
    farH /= farH.w;
    const glm::vec3 origin = glm::vec3(nearH);
    const glm::vec3 dir = glm::normalize(glm::vec3(farH) - origin);

    // Surface snap: closest StaticMesh AABB hit along the ray (when enabled).
    if (ctx.surfaceSnapEnabled && ctx.level != nullptr) {
        float bestT = std::numeric_limits<float>::max();
        bool hit = false;
        for (const StaticMeshComponent& mesh : ctx.level->StaticMeshes()) {
            if (mesh.hidden) {
                continue;
            }
            Aabb box;
            if (mesh.mesh != nullptr && mesh.mesh->Valid()) {
                box = Aabb::fromLocalTransformed(mesh.mesh->LocalMin(), mesh.mesh->LocalMax(),
                                                 mesh.EffectiveModelMatrix());
            } else {
                const glm::vec3& p = mesh.transform.position;
                box = Aabb{p - glm::vec3(0.35f), p + glm::vec3(0.35f)};
            }
            float t = 0.0f;
            if (box.intersectRay(origin, dir, t) && t > 0.05f && t < bestT) {
                bestT = t;
                hit = true;
            }
        }
        if (hit) {
            outPoint = origin + dir * bestT;
            return true;
        }
    }

    // Intersect ground plane y = 0; fall back to a point ahead of the camera.
    if (std::abs(dir.y) > 1.0e-5f) {
        const float t = -origin.y / dir.y;
        if (t > 0.05f) {
            outPoint = origin + dir * t;
            return true;
        }
    }
    outPoint = origin + dir * 5.0f;
    outPoint.y = std::max(outPoint.y, 0.0f);
    return true;
}

void ViewportPanel::HandleAssetDrop(EditorContext& ctx) {
    if (ctx.level == nullptr || ctx.resources == nullptr || (ctx.piePlaying && !ctx.pieNewWindow)) {
        if (materialPreviewActive_) {
            ClearMaterialDropPreview(ctx);
        }
        return;
    }

    // Drag cancelled / left the viewport — restore any hover material preview.
    if (ImGui::GetDragDropPayload() == nullptr && materialPreviewActive_) {
        ClearMaterialDropPreview(ctx);
    }

    if (!ImGui::BeginDragDropTarget()) {
        if (materialPreviewActive_) {
            ClearMaterialDropPreview(ctx);
        }
        return;
    }

    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
        "LEON_ASSET_PATH",
        ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
    if (payload == nullptr || payload->Data == nullptr) {
        ImGui::EndDragDropTarget();
        if (materialPreviewActive_) {
            ClearMaterialDropPreview(ctx);
        }
        return;
    }
    const std::string path(static_cast<const char*>(payload->Data));
    const bool isDelivery = payload->IsDelivery();
    ImGui::EndDragDropTarget();
    if (path.empty()) {
        return;
    }

    // Materials: live preview on the mesh under the cursor; commit on drop.
    if (HandleMaterialDrag(ctx, path, isDelivery)) {
        return;
    }
    if (!isDelivery) {
        return;
    }

    // Non-material drops only apply on mouse release.
    ClearMaterialDropPreview(ctx);

    glm::vec3 dropPos = ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0, 0};
    (void)WorldPointUnderMouse(ctx, dropPos);

    if (IsEngineContentPath(path)) {
        std::string err;
        if (!ApplyEngineContent(ctx, path, dropPos, err) && !err.empty()) {
            std::cerr << "Viewport drop: " << err << '\n';
        }
        return;
    }

    namespace fs = std::filesystem;
    const fs::path file(path);
    std::string ext = file.extension().string();
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Levels: open on drop via pendingOpenPath → EditorLayout::OpenLevel.
    if (ext == ".llev") {
        if (ctx.engine != nullptr) {
            ctx.pendingOpenPath = path;
        }
        return;
    }

    // Blueprint Class → place Blueprint Instance.
    if (ext == ".lbp") {
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        Transform root{};
        root.position = dropPos;
        if (ctx.snapEnabled) {
            EditorCommands::SnapTransform(root, ctx.gridSize, ctx.rotationSnapDegrees, false);
        }
        const std::string bpRel = MakePackRelativeAssetPath(ctx, path);
        if (PlaceBlueprintInLevel(*ctx.resources, *ctx.level, bpRel, root, {}, ctx.levelPath,
                                  nullptr)) {
            ctx.Select(EEditorSelectionKind::BlueprintInstance,
                       ctx.level->BlueprintInstances().size() - 1);
            ctx.MarkDirty();
        } else {
            std::cerr << "Viewport drop: failed to place Blueprint " << path << '\n';
        }
        return;
    }

    // Static meshes: cooked .lmesh, or import source with a cooked sibling.
    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".lmesh") {
        const std::string cookedAbs = ResolveCookedStaticMeshPath(path);
        if (cookedAbs.empty()) {
            EditorToast("No cooked .lmesh beside this source. Use File → Import / Reimport first.",
                        EEditorToastKind::Warning, 4.0f);
            return;
        }
        std::string meshPath = MakePackRelativeAssetPath(ctx, cookedAbs);
        if (meshPath.empty()) {
            meshPath = cookedAbs;
        }
        auto mesh = ctx.resources->LoadStaticMesh(ResolveAssetPath(meshPath));
        if (mesh == nullptr || !mesh->Valid()) {
            mesh = ctx.resources->LoadStaticMesh(cookedAbs);
        }
        if (mesh == nullptr || !mesh->Valid()) {
            std::cerr << "Viewport drop: failed to load mesh " << cookedAbs << '\n';
            return;
        }
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        StaticMeshComponent actor;
        actor.mesh = std::move(mesh);
        actor.transform.position = dropPos;
        actor.editorClass = "StaticMesh";
        actor.meshPath = meshPath;
        actor.collisionEnabled = true;
        actor.material = ctx.resources->DefaultMaterial();
        ctx.level->AddStaticMesh(std::move(actor));
        ctx.Select(EEditorSelectionKind::StaticMesh, ctx.level->StaticMeshes().size() - 1);
        ctx.MarkDirty();
        return;
    }

    // HDR env maps.
    if (ext == ".hdr") {
        const std::string rel = MakePackRelativeAssetPath(ctx, path);
        auto env = ctx.resources->LoadEnvMap(ResolveAssetPath(rel));
        if (env == nullptr) {
            env = ctx.resources->LoadEnvMap(path);
        }
        if (env != nullptr) {
            if (ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            ctx.level->SetEnvironment(env);
            ctx.level->SetEnvironmentPath(rel);
            ctx.MarkDirty();
        }
        return;
    }

    std::cerr << "Viewport drop: unsupported asset " << path << '\n';
}

} // namespace leon::editor
