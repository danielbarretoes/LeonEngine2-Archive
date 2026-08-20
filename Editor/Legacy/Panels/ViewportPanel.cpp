#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <ImGuizmo.h>
#include <leon/content/LeonBlueprint.h>
#include <leon/core/Camera.h>
#include <leon/core/Window.h>
#include <leon/editor/EditorCommands.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/panels/ViewportPanel.h>
#include <leon/editor/PieAspectFit.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/gameplay/Actor.h>
#include <leon/gameplay/World.h>
#include <leon/level/Level.h>
#include <leon/render/Frustum.h>
#include <leon/render/Renderer.h>
#include <limits>
#include <string>

namespace leon::editor {
namespace {

constexpr glm::vec3 kSelectionColor{1.0f, 0.55f, 0.12f};

void AddPointAabb(Renderer& renderer, const glm::vec3& center, float halfExtent) {
    const glm::vec3 ext{halfExtent, halfExtent, halfExtent};
    renderer.AddDebugAabb(center - ext, center + ext, kSelectionColor);
}

Actor* FindActorByIndex(World* world, std::size_t index) {
    if (world == nullptr) {
        return nullptr;
    }
    Actor* target = nullptr;
    std::size_t i = 0;
    world->ForEachActor([&](Actor& actor) {
        if (i == index) {
            target = &actor;
        }
        ++i;
    });
    return target;
}

Actor* ResolveSelectedActor(EditorContext& ctx) {
    if (ctx.selection.kind != EEditorSelectionKind::Actor) {
        return nullptr;
    }
    if (ctx.selection.id != 0 && ctx.world != nullptr) {
        if (Actor* byId = ctx.world->FindActorByEditorId(ctx.selection.id)) {
            return byId;
        }
    }
    return FindActorByIndex(ctx.world, ctx.selection.index);
}

} // namespace

void ViewportPanel::EnsureEditorFreeLook(Camera& camera) {
    camera.BeginFreeLookPreservingView();
}

Transform* ViewportPanel::SelectedTransformFor(EditorContext& ctx, const EditorSelection& sel) {
    if (ctx.level == nullptr || !sel.IsValid()) {
        return nullptr;
    }
    switch (sel.kind) {
    case EEditorSelectionKind::StaticMesh:
        if (sel.index < ctx.level->StaticMeshes().size()) {
            return &ctx.level->StaticMeshes()[sel.index].transform;
        }
        break;
    case EEditorSelectionKind::DirectionalLight:
        if (sel.index < ctx.level->DirectionalLights().size()) {
            return &ctx.level->DirectionalLights()[sel.index].transform;
        }
        break;
    case EEditorSelectionKind::PointLight:
        if (sel.index < ctx.level->PointLights().size()) {
            return &ctx.level->PointLights()[sel.index].transform;
        }
        break;
    case EEditorSelectionKind::SpotLight:
        if (sel.index < ctx.level->SpotLights().size()) {
            return &ctx.level->SpotLights()[sel.index].transform;
        }
        break;
    case EEditorSelectionKind::PlayerStart:
        if (sel.index < ctx.level->PlayerStarts().size()) {
            return &ctx.level->PlayerStarts()[sel.index].transform;
        }
        break;
    case EEditorSelectionKind::TriggerVolume:
        if (sel.index < ctx.level->TriggerVolumes().size()) {
            return &ctx.level->TriggerVolumes()[sel.index].transform;
        }
        break;
    case EEditorSelectionKind::PainCausingVolume:
        if (sel.index < ctx.level->PainCausingVolumes().size()) {
            return &ctx.level->PainCausingVolumes()[sel.index].transform;
        }
        break;
    case EEditorSelectionKind::AISpawnPoint:
        if (sel.index < ctx.level->AISpawnPoints().size()) {
            return &ctx.level->AISpawnPoints()[sel.index].transform;
        }
        break;
    case EEditorSelectionKind::TextRenderActor:
        if (sel.index < ctx.level->TextRenderActors().size()) {
            return &ctx.level->TextRenderActors()[sel.index].transform;
        }
        break;
    case EEditorSelectionKind::BlueprintInstance:
        if (sel.index < ctx.level->BlueprintInstances().size()) {
            return &ctx.level->BlueprintInstances()[sel.index].transform;
        }
        break;
    case EEditorSelectionKind::Actor:
        // Multi-gizmo does not drive gameplay Actors; primary selection uses scratch.
        if (sel.Equals(ctx.selection.kind, ctx.selection.index)) {
            Actor* actor = ResolveSelectedActor(ctx);
            if (actor == nullptr) {
                return nullptr;
            }
            actorGizmoScratch_.position = actor->GetActorLocation();
            actorGizmoScratch_.rotationDegrees = {0.0f, actor->GetActorYaw(), 0.0f};
            actorGizmoScratch_.scale = {1.0f, 1.0f, 1.0f};
            return &actorGizmoScratch_;
        }
        break;
    default:
        break;
    }
    return nullptr;
}

Transform* ViewportPanel::SelectedTransform(EditorContext& ctx) {
    return SelectedTransformFor(ctx, ctx.selection);
}

bool ViewportPanel::SelectedFocusPoint(EditorContext& ctx, glm::vec3& outPoint) {
    if (Transform* t = SelectedTransform(ctx)) {
        outPoint = t->position;
        return true;
    }
    return false;
}

void ViewportPanel::FocusSelection(EditorContext& ctx) {
    glm::vec3 point{};
    if (SelectedFocusPoint(ctx, point) && ctx.camera != nullptr) {
        if (ctx.camera->Mode() == ECameraMode::FreeLook) {
            const glm::vec3 forward = ctx.camera->ForwardVector();
            const float dist = ctx.camera->IsOrthographic() ? 40.0f : 5.0f;
            ctx.camera->SetEyeLocation(point - forward * dist);
        } else {
            ctx.camera->SetTarget(point);
        }
    }
}

void ViewportPanel::FocusLevelBounds(EditorContext& ctx) {
    if (ctx.camera == nullptr || ctx.level == nullptr) {
        return;
    }
    glm::vec3 boundsMin{std::numeric_limits<float>::max()};
    glm::vec3 boundsMax{std::numeric_limits<float>::lowest()};
    bool any = false;
    for (const StaticMeshComponent& mesh : ctx.level->StaticMeshes()) {
        if (mesh.hidden || mesh.mesh == nullptr || !mesh.mesh->Valid()) {
            continue;
        }
        const Aabb box = Aabb::fromLocalTransformed(mesh.mesh->LocalMin(), mesh.mesh->LocalMax(),
                                                    mesh.EffectiveModelMatrix());
        boundsMin = glm::min(boundsMin, box.min);
        boundsMax = glm::max(boundsMax, box.max);
        any = true;
    }
    if (!any) {
        return;
    }
    const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    const glm::vec3 extent = boundsMax - boundsMin;
    const float radius = std::max({extent.x, extent.y, extent.z, 1.0f}) * 0.55f;
    ctx.camera->SetMode(ECameraMode::Orbit);
    ctx.camera->SetTarget(center);
    ctx.camera->SetDistance(std::clamp(radius * 2.4f, 6.0f, 120.0f));
    ctx.camera->SetYawPitch(22.0f, 36.0f);
}

void ViewportPanel::ApplyViewMode(EditorContext& ctx, Camera& camera) {
    if (ctx.viewMode == lastAppliedViewMode_) {
        return;
    }
    lastAppliedViewMode_ = ctx.viewMode;
    if (ctx.viewMode == EEditorViewMode::Perspective) {
        return;
    }

    EnsureEditorFreeLook(camera);
    const glm::vec3 pivot = camera.EyeLocation() + camera.ForwardVector() * 8.0f;
    constexpr float kDist = 40.0f;
    switch (ctx.viewMode) {
    case EEditorViewMode::OrthoTop:
        camera.SetYawPitch(90.0f, -89.5f);
        break;
    case EEditorViewMode::OrthoFront:
        camera.SetYawPitch(-90.0f, 0.0f);
        break;
    case EEditorViewMode::OrthoSide:
        camera.SetYawPitch(0.0f, 0.0f);
        break;
    default:
        break;
    }
    camera.SetEyeLocation(pivot - camera.ForwardVector() * kDist);
    if (camera.OrthoHeight() < 1.0f) {
        camera.SetOrthoHeight(20.0f);
    }
}

void ViewportPanel::DrawEditorHelpers(EditorContext& ctx) {
    if (ctx.renderer == nullptr || ctx.level == nullptr || (ctx.piePlaying && !ctx.pieNewWindow)) {
        return;
    }
    Renderer& r = *ctx.renderer;
    constexpr glm::vec3 kPlayerStartColor{0.25f, 0.85f, 1.0f};
    for (const PlayerStart& start : ctx.level->PlayerStarts()) {
        const glm::vec3& p = start.transform.position;
        const glm::vec3 ext{0.35f, 0.35f, 0.35f};
        r.AddDebugAabb(p - ext, p + ext, kPlayerStartColor);
        const float yawRad = glm::radians(start.transform.rotationDegrees.y);
        const glm::vec3 forward{std::sin(yawRad), 0.0f, std::cos(yawRad)};
        r.AddDebugArrow(p + glm::vec3{0.0f, 0.9f, 0.0f},
                        p + glm::vec3{0.0f, 0.9f, 0.0f} + forward * 0.9f, kPlayerStartColor);
    }

    constexpr glm::vec3 kTriggerRadiusColor{0.2f, 0.9f, 0.95f};
    for (const TriggerVolume& volume : ctx.level->TriggerVolumes()) {
        // Overlap is XZ interactRadius (not transform.scale AABB) — draw the playable radius only.
        const glm::vec3& p = volume.transform.position;
        const float radius = volume.interactRadius > 0.0f ? volume.interactRadius : 2.0f;
        constexpr int kSegments = 16;
        glm::vec3 prev{p.x + radius, p.y, p.z};
        for (int i = 1; i <= kSegments; ++i) {
            const float angle =
                (static_cast<float>(i) / static_cast<float>(kSegments)) * 6.2831853f;
            const glm::vec3 next{p.x + std::cos(angle) * radius, p.y,
                                 p.z + std::sin(angle) * radius};
            r.AddDebugLine(prev, next, kTriggerRadiusColor);
            prev = next;
        }
        const glm::vec3 marker{0.15f, 0.15f, 0.15f};
        r.AddDebugAabb(p - marker, p + marker, kTriggerRadiusColor);
    }

    constexpr glm::vec3 kPainColor{1.0f, 0.35f, 0.15f};
    for (const PainCausingVolume& volume : ctx.level->PainCausingVolumes()) {
        const glm::vec3 half = glm::abs(volume.transform.scale) * 0.5f;
        const glm::vec3& p = volume.transform.position;
        r.AddDebugAabb(p - half, p + half, kPainColor);
    }

    constexpr glm::vec3 kAISpawnColor{0.35f, 0.95f, 0.4f};
    for (const AISpawnPoint& point : ctx.level->AISpawnPoints()) {
        const glm::vec3& p = point.transform.position;
        const glm::vec3 ext{0.3f, 0.3f, 0.3f};
        r.AddDebugAabb(p - ext, p + ext, kAISpawnColor);
        const float yawRad = glm::radians(point.transform.rotationDegrees.y);
        const glm::vec3 forward{std::sin(yawRad), 0.0f, std::cos(yawRad)};
        r.AddDebugArrow(p + glm::vec3{0.0f, 0.75f, 0.0f},
                        p + glm::vec3{0.0f, 0.75f, 0.0f} + forward * 0.75f, kAISpawnColor);
    }

    constexpr glm::vec3 kTextRenderColor{1.0f, 0.92f, 0.55f};
    for (const TextRenderActor& tr : ctx.level->TextRenderActors()) {
        if (tr.bHiddenInGame) {
            continue;
        }
        const glm::vec3& p = tr.transform.position;
        const glm::vec3 ext{0.12f, 0.12f, 0.12f};
        r.AddDebugAabb(p - ext, p + ext, kTextRenderColor);
    }
}

void ViewportPanel::DrawSelectionOverlay(EditorContext& ctx) {
    if (ctx.renderer == nullptr || ctx.level == nullptr || !ctx.selection.IsValid()) {
        return;
    }
    Renderer& r = *ctx.renderer;

    switch (ctx.selection.kind) {
    case EEditorSelectionKind::StaticMesh: {
        if (ctx.selection.index >= ctx.level->StaticMeshes().size()) {
            break;
        }
        const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[ctx.selection.index];
        if (mesh.mesh != nullptr && mesh.mesh->Valid()) {
            const Aabb box = Aabb::fromLocalTransformed(
                mesh.mesh->LocalMin(), mesh.mesh->LocalMax(), mesh.EffectiveModelMatrix());
            r.AddDebugAabb(box.min, box.max, kSelectionColor);
        } else {
            AddPointAabb(r, mesh.transform.position, 0.35f);
        }
        break;
    }
    case EEditorSelectionKind::DirectionalLight:
        if (ctx.selection.index < ctx.level->DirectionalLights().size()) {
            AddPointAabb(r, ctx.level->DirectionalLights()[ctx.selection.index].transform.position,
                         0.25f);
        }
        break;
    case EEditorSelectionKind::PointLight:
        if (ctx.selection.index < ctx.level->PointLights().size()) {
            AddPointAabb(r, ctx.level->PointLights()[ctx.selection.index].transform.position,
                         0.25f);
        }
        break;
    case EEditorSelectionKind::SpotLight:
        if (ctx.selection.index < ctx.level->SpotLights().size()) {
            AddPointAabb(r, ctx.level->SpotLights()[ctx.selection.index].transform.position, 0.25f);
        }
        break;
    case EEditorSelectionKind::PlayerStart:
        if (ctx.selection.index < ctx.level->PlayerStarts().size()) {
            AddPointAabb(r, ctx.level->PlayerStarts()[ctx.selection.index].transform.position,
                         0.35f);
        }
        break;
    case EEditorSelectionKind::TriggerVolume:
        if (ctx.selection.index < ctx.level->TriggerVolumes().size()) {
            const TriggerVolume& volume = ctx.level->TriggerVolumes()[ctx.selection.index];
            const glm::vec3 half = glm::abs(volume.transform.scale) * 0.5f;
            r.AddDebugAabb(volume.transform.position - half, volume.transform.position + half,
                           kSelectionColor);
        }
        break;
    case EEditorSelectionKind::PainCausingVolume:
        if (ctx.selection.index < ctx.level->PainCausingVolumes().size()) {
            const PainCausingVolume& volume = ctx.level->PainCausingVolumes()[ctx.selection.index];
            const glm::vec3 half = glm::abs(volume.transform.scale) * 0.5f;
            r.AddDebugAabb(volume.transform.position - half, volume.transform.position + half,
                           kSelectionColor);
        }
        break;
    case EEditorSelectionKind::AISpawnPoint:
        if (ctx.selection.index < ctx.level->AISpawnPoints().size()) {
            AddPointAabb(r, ctx.level->AISpawnPoints()[ctx.selection.index].transform.position,
                         0.35f);
        }
        break;
    case EEditorSelectionKind::TextRenderActor:
        if (ctx.selection.index < ctx.level->TextRenderActors().size()) {
            AddPointAabb(r, ctx.level->TextRenderActors()[ctx.selection.index].transform.position,
                         0.2f);
        }
        break;
    case EEditorSelectionKind::BlueprintInstance:
        if (ctx.selection.index < ctx.level->BlueprintInstances().size()) {
            AddPointAabb(r, ctx.level->BlueprintInstances()[ctx.selection.index].transform.position,
                         0.45f);
        }
        break;
    case EEditorSelectionKind::Actor: {
        Actor* actor = ResolveSelectedActor(ctx);
        if (actor != nullptr) {
            AddPointAabb(r, actor->GetRootComponent().GetComponentLocation(), 0.4f);
        }
        break;
    }
    default:
        break;
    }
}

void ViewportPanel::HandleCameraInput(EditorContext& ctx, float deltaTime) {
    if (ctx.piePlaying && !ctx.pieNewWindow) {
        dragMode_ = EViewportDrag::None;
        flyActive_ = false;
        return;
    }
    if (ctx.camera == nullptr || ctx.window == nullptr) {
        dragMode_ = EViewportDrag::None;
        flyActive_ = false;
        return;
    }
    // Only block while actively dragging a gizmo — IsOver() was killing RMB look near handles.
    if (ImGuizmo::IsUsing()) {
        dragMode_ = EViewportDrag::None;
        return;
    }

    const bool ortho = ctx.viewMode != EEditorViewMode::Perspective;
    Window& window = *ctx.window;
    const ImGuiIO& io = ImGui::GetIO();
    // Prefer ImGui buttons/deltas so docking / DPI / capture match the Viewport image.
    const bool alt = io.KeyAlt;
    const bool rmb = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const bool mmb = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    const bool lmb = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool shift = io.KeyShift;
    const bool canStartDrag = ctx.viewportHovered || flyActive_;

    if (!ortho && rmb && canStartDrag && !alt) {
        if (!flyActive_) {
            EnsureEditorFreeLook(*ctx.camera);
            flyActive_ = true;
            if (ctx.viewportHovered) {
                ImGui::SetWindowFocus();
            }
        }
    } else if (!rmb) {
        flyActive_ = false;
    }

    EViewportDrag wanted = EViewportDrag::None;
    if (!ortho && alt && lmb && ctx.viewportHovered) {
        wanted = EViewportDrag::Orbit;
    } else if (!ortho && alt && rmb && ctx.viewportHovered) {
        wanted = EViewportDrag::Dolly;
    } else if ((mmb || (ortho && rmb)) && ctx.viewportHovered) {
        wanted = EViewportDrag::Pan;
    } else if (!ortho && flyActive_) {
        wanted = EViewportDrag::Look;
    }

    if (wanted != EViewportDrag::None) {
        if (dragMode_ == wanted) {
            if (!suppressDragDelta_) {
                const float dx = io.MouseDelta.x;
                const float dy = io.MouseDelta.y;
                switch (wanted) {
                case EViewportDrag::Orbit:
                    if (ctx.camera->Mode() == ECameraMode::FreeLook) {
                        ctx.camera->AddLook(dx * 0.25f, -dy * 0.25f);
                    } else {
                        ctx.camera->Orbit(dx * 0.25f, -dy * 0.25f);
                    }
                    break;
                case EViewportDrag::Look:
                    EnsureEditorFreeLook(*ctx.camera);
                    ctx.camera->AddLook(dx * 0.25f, -dy * 0.25f);
                    break;
                case EViewportDrag::Pan: {
                    const float scale = ortho ? (ctx.camera->OrthoHeight() * 0.0025f)
                                              : (std::max(ctx.camera->Distance(), 1.0f) * 0.0025f);
                    ctx.camera->Pan(-dx * scale, dy * scale);
                    break;
                }
                case EViewportDrag::Dolly:
                    if (ctx.camera->Mode() == ECameraMode::FreeLook) {
                        ctx.camera->SetEyeLocation(ctx.camera->EyeLocation() +
                                                   ctx.camera->ForwardVector() * (-dy * 0.05f));
                    } else {
                        ctx.camera->Zoom(dy * 0.05f);
                    }
                    break;
                default:
                    break;
                }
            }
            suppressDragDelta_ = false;
        } else {
            suppressDragDelta_ = true;
        }
        dragMode_ = wanted;
    } else {
        dragMode_ = EViewportDrag::None;
        suppressDragDelta_ = false;
    }

    if (ctx.viewportHovered && !flyActive_) {
        const float wheel = io.MouseWheel;
        if (wheel != 0.0f) {
            if (ortho) {
                ctx.camera->SetOrthoHeight(ctx.camera->OrthoHeight() *
                                           (wheel > 0.0f ? 0.9f : 1.1f));
            } else if (ctx.camera->Mode() == ECameraMode::FreeLook) {
                ctx.camera->SetEyeLocation(ctx.camera->EyeLocation() +
                                           ctx.camera->ForwardVector() * (wheel * 0.5f));
            } else {
                ctx.camera->Zoom(-wheel * 0.5f);
            }
        }
    }

    const bool wantMove = (flyActive_ && !ortho) ||
                          (ortho && ctx.viewportHovered &&
                           (window.IsKeyPressed(GLFW_KEY_W) || window.IsKeyPressed(GLFW_KEY_S) ||
                            window.IsKeyPressed(GLFW_KEY_A) || window.IsKeyPressed(GLFW_KEY_D) ||
                            window.IsKeyPressed(GLFW_KEY_Q) || window.IsKeyPressed(GLFW_KEY_E)));
    if (wantMove && !io.WantTextInput) {
        EnsureEditorFreeLook(*ctx.camera);
        const float speed =
            (shift ? 18.0f : 6.0f) * deltaTime * (ortho ? ctx.camera->OrthoHeight() * 0.15f : 1.0f);
        glm::vec3 eye = ctx.camera->EyeLocation();
        const glm::vec3 forward = ctx.camera->ForwardVector();
        const glm::vec3 right = ctx.camera->RightVector();
        const glm::vec3 up{0.0f, 1.0f, 0.0f};
        if (ortho) {
            const glm::vec3 camUp = glm::normalize(glm::cross(right, forward));
            if (window.IsKeyPressed(GLFW_KEY_W)) {
                eye += camUp * speed;
            }
            if (window.IsKeyPressed(GLFW_KEY_S)) {
                eye -= camUp * speed;
            }
            if (window.IsKeyPressed(GLFW_KEY_A)) {
                eye -= right * speed;
            }
            if (window.IsKeyPressed(GLFW_KEY_D)) {
                eye += right * speed;
            }
        } else {
            if (window.IsKeyPressed(GLFW_KEY_W)) {
                eye += forward * speed;
            }
            if (window.IsKeyPressed(GLFW_KEY_S)) {
                eye -= forward * speed;
            }
            if (window.IsKeyPressed(GLFW_KEY_A)) {
                eye -= right * speed;
            }
            if (window.IsKeyPressed(GLFW_KEY_D)) {
                eye += right * speed;
            }
            if (window.IsKeyPressed(GLFW_KEY_Q)) {
                eye -= up * speed;
            }
            if (window.IsKeyPressed(GLFW_KEY_E)) {
                eye += up * speed;
            }
        }
        ctx.camera->SetEyeLocation(eye);
    }
}

void ViewportPanel::HandleViewportPicking(EditorContext& ctx) {
    Camera* cam =
        (ctx.pieNewWindow && ctx.editorViewCamera != nullptr) ? ctx.editorViewCamera : ctx.camera;
    if (ctx.level == nullptr || cam == nullptr || !ctx.viewportHovered) {
        return;
    }
    // Alt+LMB is orbit — do not pick.
    if (ImGui::GetIO().KeyAlt) {
        return;
    }
    // Place Actors (Modes): LMB places; skip selection picking.
    if (ctx.IsPlaceActorsActive()) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ctx.ClearPlaceActors();
            return;
        }
        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGuizmo::IsOver() ||
            ImGuizmo::IsUsing()) {
            return;
        }
        glm::vec3 placePos = cam->Target();
        (void)WorldPointUnderMouse(ctx, placePos);
        (void)EditorCommands::PlaceEditorActor(ctx, ctx.placeActorsKind, placePos,
                                               ctx.placeActorsBlueprintPath);
        return;
    }
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    const float imageW = std::max(1.0f, itemMax.x - itemMin.x);
    const float imageH = std::max(1.0f, itemMax.y - itemMin.y);
    const float localX = mouse.x - itemMin.x;
    const float localY = mouse.y - itemMin.y;
    if (localX < 0.0f || localY < 0.0f || localX > imageW || localY > imageH) {
        return;
    }

    // Mouse → world ray through the viewport image (handles letterboxing / size mismatch).
    const float ndcX = ((localX / imageW) * 2.0f) - 1.0f;
    const float ndcY = 1.0f - ((localY / imageH) * 2.0f);
    const glm::mat4 invVP = glm::inverse(cam->ProjectionMatrix() * cam->ViewMatrix());
    glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farH = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearH.w) < 1.0e-8f || std::abs(farH.w) < 1.0e-8f) {
        return;
    }
    nearH /= nearH.w;
    farH /= farH.w;
    const glm::vec3 rayOrigin = glm::vec3(nearH);
    const glm::vec3 rayDir = glm::normalize(glm::vec3(farH) - rayOrigin);

    float bestT = std::numeric_limits<float>::max();
    EEditorSelectionKind bestKind = EEditorSelectionKind::None;
    std::size_t bestIndex = 0;

    auto considerAabb = [&](EEditorSelectionKind kind, std::size_t index, const Aabb& box) {
        float t = 0.0f;
        if (!box.intersectRay(rayOrigin, rayDir, t) || t < 0.0f || t >= bestT) {
            return;
        }
        bestT = t;
        bestKind = kind;
        bestIndex = index;
    };

    for (std::size_t i = 0; i < ctx.level->StaticMeshes().size(); ++i) {
        const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[i];
        if (mesh.hidden) {
            continue;
        }
        EEditorSelectionKind kind = EEditorSelectionKind::StaticMesh;
        std::size_t index = i;
        if (mesh.blueprintInstanceId != 0) {
            for (std::size_t b = 0; b < ctx.level->BlueprintInstances().size(); ++b) {
                if (ctx.level->BlueprintInstances()[b].instanceId == mesh.blueprintInstanceId) {
                    kind = EEditorSelectionKind::BlueprintInstance;
                    index = b;
                    break;
                }
            }
        }
        if (mesh.mesh != nullptr && mesh.mesh->Valid()) {
            considerAabb(kind, index,
                         Aabb::fromLocalTransformed(mesh.mesh->LocalMin(), mesh.mesh->LocalMax(),
                                                    mesh.EffectiveModelMatrix()));
        } else {
            const glm::vec3& p = mesh.transform.position;
            considerAabb(kind, index, Aabb{p - glm::vec3(0.35f), p + glm::vec3(0.35f)});
        }
    }
    for (std::size_t i = 0; i < ctx.level->BlueprintInstances().size(); ++i) {
        const glm::vec3& p = ctx.level->BlueprintInstances()[i].transform.position;
        considerAabb(EEditorSelectionKind::BlueprintInstance, i,
                     Aabb{p - glm::vec3(0.5f), p + glm::vec3(0.5f)});
    }

    for (std::size_t i = 0; i < ctx.level->PointLights().size(); ++i) {
        const glm::vec3& p = ctx.level->PointLights()[i].transform.position;
        considerAabb(EEditorSelectionKind::PointLight, i,
                     Aabb{p - glm::vec3(0.4f), p + glm::vec3(0.4f)});
    }
    for (std::size_t i = 0; i < ctx.level->SpotLights().size(); ++i) {
        const glm::vec3& p = ctx.level->SpotLights()[i].transform.position;
        considerAabb(EEditorSelectionKind::SpotLight, i,
                     Aabb{p - glm::vec3(0.4f), p + glm::vec3(0.4f)});
    }
    for (std::size_t i = 0; i < ctx.level->PlayerStarts().size(); ++i) {
        const glm::vec3& p = ctx.level->PlayerStarts()[i].transform.position;
        considerAabb(EEditorSelectionKind::PlayerStart, i,
                     Aabb{p - glm::vec3(0.5f), p + glm::vec3(0.5f)});
    }
    for (std::size_t i = 0; i < ctx.level->TriggerVolumes().size(); ++i) {
        const TriggerVolume& volume = ctx.level->TriggerVolumes()[i];
        const glm::vec3 half = glm::abs(volume.transform.scale) * 0.5f;
        considerAabb(EEditorSelectionKind::TriggerVolume, i,
                     Aabb{volume.transform.position - half, volume.transform.position + half});
    }
    for (std::size_t i = 0; i < ctx.level->PainCausingVolumes().size(); ++i) {
        const PainCausingVolume& volume = ctx.level->PainCausingVolumes()[i];
        const glm::vec3 half = glm::abs(volume.transform.scale) * 0.5f;
        considerAabb(EEditorSelectionKind::PainCausingVolume, i,
                     Aabb{volume.transform.position - half, volume.transform.position + half});
    }
    for (std::size_t i = 0; i < ctx.level->AISpawnPoints().size(); ++i) {
        const glm::vec3& p = ctx.level->AISpawnPoints()[i].transform.position;
        considerAabb(EEditorSelectionKind::AISpawnPoint, i,
                     Aabb{p - glm::vec3(0.5f), p + glm::vec3(0.5f)});
    }
    for (std::size_t i = 0; i < ctx.level->TextRenderActors().size(); ++i) {
        const glm::vec3& p = ctx.level->TextRenderActors()[i].transform.position;
        considerAabb(EEditorSelectionKind::TextRenderActor, i,
                     Aabb{p - glm::vec3(0.35f), p + glm::vec3(0.35f)});
    }
    for (std::size_t i = 0; i < ctx.level->DirectionalLights().size(); ++i) {
        const glm::vec3& p = ctx.level->DirectionalLights()[i].transform.position;
        considerAabb(EEditorSelectionKind::DirectionalLight, i,
                     Aabb{p - glm::vec3(0.4f), p + glm::vec3(0.4f)});
    }

    if (bestKind != EEditorSelectionKind::None) {
        ctx.Select(bestKind, bestIndex, ImGui::GetIO().KeyCtrl);
    } else if (!ImGui::GetIO().KeyCtrl) {
        ctx.ClearSelection();
    }
}

void ViewportPanel::DrawViewportMenuBar(EditorContext& ctx) {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    const char* perspectiveLabel = "Perspective";
    switch (ctx.viewMode) {
    case EEditorViewMode::OrthoTop:
        perspectiveLabel = "Top";
        break;
    case EEditorViewMode::OrthoFront:
        perspectiveLabel = "Front";
        break;
    case EEditorViewMode::OrthoSide:
        perspectiveLabel = "Side";
        break;
    default:
        break;
    }
    if (ImGui::BeginMenu(perspectiveLabel)) {
        if (ImGui::MenuItem("Perspective", "Alt+1", ctx.viewMode == EEditorViewMode::Perspective)) {
            ctx.viewMode = EEditorViewMode::Perspective;
        }
        if (ImGui::MenuItem("Top", "Alt+2", ctx.viewMode == EEditorViewMode::OrthoTop)) {
            ctx.viewMode = EEditorViewMode::OrthoTop;
        }
        if (ImGui::MenuItem("Front", "Alt+3", ctx.viewMode == EEditorViewMode::OrthoFront)) {
            ctx.viewMode = EEditorViewMode::OrthoFront;
        }
        if (ImGui::MenuItem("Side", "Alt+4", ctx.viewMode == EEditorViewMode::OrthoSide)) {
            ctx.viewMode = EEditorViewMode::OrthoSide;
        }
        ImGui::EndMenu();
    }

    const char* viewModeLabel = "Lit";
    switch (ctx.viewportViewMode) {
    case EEditorViewportViewMode::PlayerCollision:
        viewModeLabel = "Player Collision";
        break;
    case EEditorViewportViewMode::Wireframe:
        viewModeLabel = "Wireframe";
        break;
    default:
        break;
    }
    if (ImGui::BeginMenu(viewModeLabel)) {
        if (ImGui::MenuItem("Lit", "Alt+5", ctx.viewportViewMode == EEditorViewportViewMode::Lit)) {
            ctx.viewportViewMode = EEditorViewportViewMode::Lit;
        }
        if (ImGui::MenuItem("Player Collision", "Alt+6",
                            ctx.viewportViewMode == EEditorViewportViewMode::PlayerCollision)) {
            ctx.viewportViewMode = EEditorViewportViewMode::PlayerCollision;
        }
        if (ImGui::MenuItem("Wireframe", "Alt+7",
                            ctx.viewportViewMode == EEditorViewportViewMode::Wireframe)) {
            ctx.viewportViewMode = EEditorViewportViewMode::Wireframe;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Show")) {
        ImGui::MenuItem("Grid", nullptr, &ctx.showGrid);
        ImGui::MenuItem("Stats", "F3", &ctx.showStats);
        ImGui::Separator();
        ImGui::MenuItem("Snap", nullptr, &ctx.snapEnabled);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat("Grid Size", &ctx.gridSize, 0.05f, 0.05f, 50.0f, "%.2f");
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat("Rotation Snap", &ctx.rotationSnapDegrees, 1.0f, 1.0f, 90.0f, "%.0f");
        ImGui::EndMenu();
    }

    // Transform gizmo modes belong on the Viewport (W/E/R shortcuts still work in the panel).
    auto gizmoModeButton = [&](const char* id, const char* label, EGizmoOperation op) {
        const bool active = ctx.gizmoOp == op;
        if (ui::Button(id, label, active ? ui::EUiVariant::Primary : ui::EUiVariant::Ghost,
                       ui::EUiSize::Sm)) {
            ctx.gizmoOp = op;
        }
    };
    ui::Separator();
    gizmoModeButton("##gizmo_t", "Translate", EGizmoOperation::Translate);
    gizmoModeButton("##gizmo_r", "Rotate", EGizmoOperation::Rotate);
    gizmoModeButton("##gizmo_s", "Scale", EGizmoOperation::Scale);
    if (ui::Button("##gizmo_space", ctx.gizmoSpace == EGizmoSpace::Local ? "Local" : "World",
                   ui::EUiVariant::Outline, ui::EUiSize::Sm)) {
        ctx.gizmoSpace =
            ctx.gizmoSpace == EGizmoSpace::Local ? EGizmoSpace::World : EGizmoSpace::Local;
    }

    ImGui::EndMenuBar();
}

void ViewportPanel::DrawSelectionContextMenu(EditorContext& ctx) {
    if (ImGui::BeginPopup("##viewport_sel_ctx")) {
        if (EditorCommands::DrawSelectionContextMenuItems(ctx)) {
            ctx.requestRenameSelected = true;
        }
        ImGui::EndPopup();
    }
}

void ViewportPanel::Draw(EditorContext& ctx) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_MenuBar)) {
        ImGui::PopStyleVar();
        ImGui::End();
        return;
    }
    ImGui::PopStyleVar();

    DrawViewportMenuBar(ctx);

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    const int panelW = std::max(1, static_cast<int>(size.x));
    const int panelH = std::max(1, static_cast<int>(size.y));
    const bool playLocksViewport = ctx.piePlaying && !ctx.pieNewWindow;

    int drawX = 0;
    int drawY = 0;
    int drawW = panelW;
    int drawH = panelH;
    if (playLocksViewport) {
        FitPieViewport(panelW, panelH, ctx.pieAspect, drawX, drawY, drawW, drawH);
    }
    ctx.viewportWidth = drawW;
    ctx.viewportHeight = drawH;
    ctx.viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    RenderScene(ctx);

    if (target_.Valid()) {
        if (playLocksViewport) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(cursor,
                                ImVec2(cursor.x + static_cast<float>(panelW),
                                       cursor.y + static_cast<float>(panelH)),
                                IM_COL32(0, 0, 0, 255));
            ImGui::SetCursorScreenPos(
                ImVec2(cursor.x + static_cast<float>(drawX), cursor.y + static_cast<float>(drawY)));
        }
        const ImVec2 imageSize{static_cast<float>(drawW), static_cast<float>(drawH)};
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(target_.colorTexture())),
                     imageSize, ImVec2(0, 1), ImVec2(1, 0));
        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const bool imageHovered =
            ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        if (playLocksViewport) {
            ImGui::SetCursorScreenPos(cursor);
            ImGui::Dummy(ImVec2(static_cast<float>(panelW), static_cast<float>(panelH)));
        }

        // Hover the rendered image (not just the dock window chrome) for camera / PIE look.
        ctx.viewportHovered =
            imageHovered || (flyActive_ && ImGui::IsMouseDown(ImGuiMouseButton_Right));

        Camera* viewCamForOverlay = (ctx.pieNewWindow && ctx.editorViewCamera != nullptr)
                                        ? ctx.editorViewCamera
                                        : ctx.camera;
        // During Selected Viewport PIE the play camera is ctx.camera.
        if (ctx.piePlaying && !ctx.pieNewWindow) {
            viewCamForOverlay = ctx.camera;
        }
        if (viewCamForOverlay != nullptr) {
            DrawAxisIndicator(*viewCamForOverlay, imageMin, imageSize);
        }
        DrawViewModeOverlay(ctx, imageMin);
        DrawStatsOverlay(ctx, imageMin);
        DrawDebugHintsOverlay(ctx, imageMin);
        // TextRender labels are drawn in 3D inside Renderer::DrawScene (world-fixed plane).
        if (ctx.IsPlaceActorsActive()) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const char* placeHint = ctx.placeActorsKind == EEditorPlaceActorsKind::BlueprintClass
                                        ? "Place Blueprint — LMB to place, Esc to cancel"
                                        : "Place Actors — LMB to place, Esc to cancel";
            draw->AddText(ImVec2(imageMin.x + 12.0f, imageMin.y + 28.0f),
                          IM_COL32(255, 220, 120, 255), placeHint);
        }

        // Unreal-like: short RMB click → context menu / cancel place; hold → fly look.
        if (!playLocksViewport && imageHovered && !ImGui::GetIO().KeyAlt) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                rmbContextPending_ = true;
                rmbPressPos_ = ImGui::GetMousePos();
            }
            if (rmbContextPending_ && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                const ImVec2 d = ImGui::GetMousePos();
                const float dx = d.x - rmbPressPos_.x;
                const float dy = d.y - rmbPressPos_.y;
                if (dx * dx + dy * dy > 16.0f) {
                    rmbContextPending_ = false;
                }
            }
            if (rmbContextPending_ && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                rmbContextPending_ = false;
                flyActive_ = false;
                dragMode_ = EViewportDrag::None;
                if (ctx.IsPlaceActorsActive()) {
                    ctx.ClearPlaceActors();
                } else if (ctx.selection.IsValid()) {
                    ImGui::OpenPopup("##viewport_sel_ctx");
                }
            }
        } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            rmbContextPending_ = false;
        }
        DrawSelectionContextMenu(ctx);

        Camera* previousCtxCamera = ctx.camera;
        if (ctx.piePlaying && ctx.pieNewWindow && ctx.editorViewCamera != nullptr) {
            ctx.camera = ctx.editorViewCamera;
        }
        if (!playLocksViewport) {
            HandleCameraInput(ctx, ctx.deltaTime);
        }
        ctx.camera = previousCtxCamera;

        if (!playLocksViewport) {
            HandleAssetDrop(ctx);
        }

        const bool rmb = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        if (!playLocksViewport && ctx.viewportFocused && !ImGui::GetIO().WantTextInput && !rmb &&
            !flyActive_) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) {
                ctx.gizmoOp = EGizmoOperation::Translate;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_E)) {
                ctx.gizmoOp = EGizmoOperation::Rotate;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                ctx.gizmoOp = EGizmoOperation::Scale;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F)) {
                ctx.RequestFocusSelected();
            }
        }

        Transform* transform = SelectedTransform(ctx);
        if (transform != nullptr &&
            EditorCommands::IsEditorLocked(ctx, ctx.selection.kind, ctx.selection.index)) {
            transform = nullptr; // Lock: no gizmo
        }
        const bool multiGizmo = ctx.selected.size() > 1 && transform != nullptr;
        if (multiGizmo) {
            if (!ImGuizmo::IsUsing() || !multiGizmoSeeded_) {
                glm::vec3 avg{0.0f};
                int n = 0;
                for (const EditorSelection& s : ctx.selected) {
                    if (EditorCommands::IsEditorLocked(ctx, s.kind, s.index)) {
                        continue;
                    }
                    if (Transform* t = SelectedTransformFor(ctx, s)) {
                        avg += t->position;
                        ++n;
                    }
                }
                if (n > 0) {
                    multiGizmoScratch_ = *transform;
                    multiGizmoScratch_.position = avg / static_cast<float>(n);
                    multiGizmoSeeded_ = true;
                }
            }
            transform = &multiGizmoScratch_;
        } else {
            multiGizmoSeeded_ = false;
        }

        Camera* viewCam = (ctx.pieNewWindow && ctx.editorViewCamera != nullptr)
                              ? ctx.editorViewCamera
                              : ctx.camera;

        if (!playLocksViewport && transform != nullptr && viewCam != nullptr) {
            const bool gizmoUsing = ImGuizmo::IsUsing();
            if (ctx.history != nullptr && gizmoUsing && !gizmoWasUsing_) {
                ctx.history->Capture(ctx, "Move Actors");
            }
            const Transform before = *transform;
            const bool changed = gizmo_.Manipulate(ctx, viewCam->ViewMatrix(),
                                                   viewCam->ProjectionMatrix(), *transform);
            if (changed) {
                if (ctx.snapEnabled) {
                    EditorCommands::SnapTransform(*transform, ctx.gridSize, ctx.rotationSnapDegrees,
                                                  false);
                }
                if (multiGizmo) {
                    // Apply TRS deltas so each actor keeps its relative offset (not absolute copy).
                    const glm::vec3 dPos = transform->position - before.position;
                    const glm::vec3 dRot = transform->rotationDegrees - before.rotationDegrees;
                    const glm::vec3 scaleRatio{
                        (std::abs(before.scale.x) > 1e-6f) ? (transform->scale.x / before.scale.x)
                                                           : 1.0f,
                        (std::abs(before.scale.y) > 1e-6f) ? (transform->scale.y / before.scale.y)
                                                           : 1.0f,
                        (std::abs(before.scale.z) > 1e-6f) ? (transform->scale.z / before.scale.z)
                                                           : 1.0f,
                    };
                    for (const EditorSelection& s : ctx.selected) {
                        if (EditorCommands::IsEditorLocked(ctx, s.kind, s.index)) {
                            continue;
                        }
                        if (Transform* t = SelectedTransformFor(ctx, s)) {
                            t->position += dPos;
                            t->rotationDegrees += dRot;
                            t->scale.x *= scaleRatio.x;
                            t->scale.y *= scaleRatio.y;
                            t->scale.z *= scaleRatio.z;
                            if (s.kind == EEditorSelectionKind::BlueprintInstance &&
                                ctx.level != nullptr &&
                                s.index < ctx.level->BlueprintInstances().size()) {
                                BlueprintInstance& bp = ctx.level->BlueprintInstances()[s.index];
                                if (!SyncBlueprintInstanceTransforms(*ctx.level, bp,
                                                                     ctx.levelPath)) {
                                    blueprintGizmoNeedsExpand_ = true;
                                    blueprintGizmoExpandInstanceId_ = bp.instanceId;
                                }
                            }
                        }
                    }
                } else if (ctx.selection.kind == EEditorSelectionKind::Actor) {
                    if (Actor* actor = ResolveSelectedActor(ctx)) {
                        actor->SetActorLocationAndRotation(actorGizmoScratch_.position,
                                                           actorGizmoScratch_.rotationDegrees.y);
                    }
                } else if (ctx.selection.kind == EEditorSelectionKind::BlueprintInstance &&
                           ctx.level != nullptr &&
                           ctx.selection.index < ctx.level->BlueprintInstances().size()) {
                    BlueprintInstance& bp = ctx.level->BlueprintInstances()[ctx.selection.index];
                    if (!SyncBlueprintInstanceTransforms(*ctx.level, bp, ctx.levelPath)) {
                        blueprintGizmoNeedsExpand_ = true;
                        blueprintGizmoExpandInstanceId_ = bp.instanceId;
                    }
                }
                ctx.MarkDirty();
                if (ctx.level != nullptr) {
                    auto selectionNeedsRebake = [&](const EditorSelection& s) {
                        if (s.kind != EEditorSelectionKind::StaticMesh ||
                            s.index >= ctx.level->StaticMeshes().size()) {
                            return false;
                        }
                        const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[s.index];
                        return mesh.mobility == EComponentMobility::Static &&
                               (!mesh.lightmapId.empty() || mesh.UsesLightmap());
                    };
                    if (multiGizmo) {
                        for (const EditorSelection& s : ctx.selected) {
                            if (selectionNeedsRebake(s)) {
                                ctx.MarkLightingOutOfDate();
                                break;
                            }
                        }
                    } else if (selectionNeedsRebake(ctx.selection)) {
                        ctx.MarkLightingOutOfDate();
                    }
                }
            }
            gizmoWasUsing_ = gizmoUsing;
            if (!gizmoUsing) {
                multiGizmoSeeded_ = false;
            }
        } else {
            gizmoWasUsing_ = false;
            multiGizmoSeeded_ = false;
        }

        // Fallback full expand if live sync failed during the last gizmo drag.
        if (blueprintGizmoNeedsExpand_ && !ImGuizmo::IsUsing() && ctx.resources != nullptr &&
            ctx.level != nullptr && blueprintGizmoExpandInstanceId_ != 0) {
            for (BlueprintInstance& bp : ctx.level->BlueprintInstances()) {
                if (bp.instanceId == blueprintGizmoExpandInstanceId_) {
                    (void)ExpandBlueprintInstance(*ctx.resources, *ctx.level, bp, ctx.levelPath);
                    break;
                }
            }
            blueprintGizmoNeedsExpand_ = false;
            blueprintGizmoExpandInstanceId_ = 0;
        }

        if (!playLocksViewport && !ImGui::GetIO().WantTextInput &&
            (ctx.viewportHovered || ctx.viewportFocused) && !ctx.contentBrowserFocused) {
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                EditorCommands::DeleteSelection(ctx, ctx.history);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_End) && transform != nullptr) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx, "Snap to Floor");
                }
                auto snapOne = [&](Transform& t) {
                    t.position.y = 0.0f;
                    if (ctx.snapEnabled) {
                        t.position = EditorCommands::SnapPosition(t.position, ctx.gridSize);
                    }
                };
                if (multiGizmo) {
                    for (const EditorSelection& s : ctx.selected) {
                        if (EditorCommands::IsEditorLocked(ctx, s.kind, s.index)) {
                            continue;
                        }
                        if (Transform* t = SelectedTransformFor(ctx, s)) {
                            snapOne(*t);
                        }
                    }
                } else if (ctx.selection.kind == EEditorSelectionKind::Actor) {
                    if (Actor* actor = ResolveSelectedActor(ctx)) {
                        glm::vec3 loc = actor->GetActorLocation();
                        loc.y = 0.0f;
                        if (ctx.snapEnabled) {
                            loc = EditorCommands::SnapPosition(loc, ctx.gridSize);
                        }
                        actor->SetActorLocationAndRotation(loc, actor->GetActorYaw());
                    }
                } else {
                    snapOne(*transform);
                }
                if (ctx.level != nullptr) {
                    auto selectionNeedsRebake = [&](const EditorSelection& s) {
                        if (s.kind != EEditorSelectionKind::StaticMesh ||
                            s.index >= ctx.level->StaticMeshes().size()) {
                            return false;
                        }
                        const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[s.index];
                        return mesh.mobility == EComponentMobility::Static &&
                               (!mesh.lightmapId.empty() || mesh.UsesLightmap());
                    };
                    if (multiGizmo) {
                        for (const EditorSelection& s : ctx.selected) {
                            if (selectionNeedsRebake(s)) {
                                ctx.MarkLightingOutOfDate();
                                break;
                            }
                        }
                    } else if (selectionNeedsRebake(ctx.selection)) {
                        ctx.MarkLightingOutOfDate();
                    }
                }
                ctx.MarkDirty();
            }
        }

        if (!playLocksViewport && !ImGuizmo::IsUsing()) {
            HandleViewportPicking(ctx);
        }
    } else {
        ImGui::Dummy(size);
        ctx.viewportHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        if (!playLocksViewport) {
            HandleCameraInput(ctx, ctx.deltaTime);
            HandleAssetDrop(ctx);
        }
    }

    ImGui::End();
}

void ViewportPanel::RenderScene(EditorContext& ctx) {
    if (ctx.renderer == nullptr || ctx.level == nullptr) {
        return;
    }

    Camera* viewCam = ctx.camera;
    if (ctx.piePlaying && ctx.pieNewWindow && ctx.editorViewCamera != nullptr) {
        viewCam = ctx.editorViewCamera;
    }
    if (viewCam == nullptr) {
        return;
    }

    // Temporarily point context camera helpers at the view used for this panel.
    Camera* previousCtxCamera = ctx.camera;
    ctx.camera = viewCam;

    const ImGuiIO& io = ImGui::GetIO();
    const float scaleX = std::max(io.DisplayFramebufferScale.x, 1.0f);
    const float scaleY = std::max(io.DisplayFramebufferScale.y, 1.0f);
    const int fbW = std::max(1, static_cast<int>(std::lround(ctx.viewportWidth * scaleX)));
    const int fbH = std::max(1, static_cast<int>(std::lround(ctx.viewportHeight * scaleY)));

    if (!target_.EnsureSize(fbW, fbH)) {
        ctx.camera = previousCtxCamera;
        return;
    }

    ApplyViewMode(ctx, *viewCam);
    const float aspect =
        static_cast<float>(ctx.viewportWidth) / static_cast<float>(std::max(ctx.viewportHeight, 1));
    if (ctx.viewMode != EEditorViewMode::Perspective) {
        viewCam->SetOrthographic(viewCam->OrthoHeight(), aspect, 0.1f, 500.0f);
    } else {
        viewCam->SetPerspective(viewCam->FieldOfView(), aspect, 0.1f, 500.0f);
    }

    if (ctx.requestFocusSelected) {
        FocusSelection(ctx);
        ctx.requestFocusSelected = false;
    }

    // During Selected Viewport PIE, force Lit (match Shipping).
    const bool playLocksViewport = ctx.piePlaying && !ctx.pieNewWindow;
    const bool playerCollision =
        !playLocksViewport && ctx.viewportViewMode == EEditorViewportViewMode::PlayerCollision;
    const bool wireframe =
        !playLocksViewport && ctx.viewportViewMode == EEditorViewportViewMode::Wireframe;

    // Player Collision = collision wireframes only (Unreal VMI_CollisionPawn-like).
    if (!playerCollision) {
        DrawEditorHelpers(ctx);
        DrawSelectionOverlay(ctx);
        DrawGrid(ctx);
        if (!playLocksViewport) {
            AppendEngineDebugOverlay(ctx);
        }
    } else {
        AppendPlayerCollisionOverlay(ctx);
    }

    // Flow: View Mode
    // 1. Lit / Wireframe → full DrawScene (Wireframe = polygon mode + no post)
    // 2. Player Collision → disable scene geometry, flush collision wireframes only
    ctx.renderer->SetSceneGeometryEnabled(!playerCollision);
    ctx.renderer->SetWireframeMode(wireframe);
    ctx.renderer->SetDrawFramebuffer(target_.fbo());
    ctx.renderer->BeginFrame(fbW, fbH);
    ctx.renderer->DrawScene(*ctx.level, *viewCam);
    if (playLocksViewport && ctx.piePaintPlayOverlay) {
        ctx.piePaintPlayOverlay(fbW, fbH);
    }
    ctx.renderer->SetDrawFramebuffer(0);
    ctx.renderer->SetSceneGeometryEnabled(true);
    ctx.renderer->SetWireframeMode(false);
    target_.End();

    overlayView_ = viewCam->ViewMatrix();
    overlayProjection_ = viewCam->ProjectionMatrix();
    overlayEye_ = viewCam->GetCameraLocation();
    overlayFov_ = viewCam->FieldOfView();
    overlayOrtho_ = viewCam->IsOrthographic();
    overlayFbW_ = fbW;
    overlayFbH_ = fbH;
    overlayMatricesValid_ = true;

    ctx.camera = previousCtxCamera;
}

} // namespace leon::editor
