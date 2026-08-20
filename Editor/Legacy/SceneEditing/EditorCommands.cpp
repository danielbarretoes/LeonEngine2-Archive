#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <leon/content/LeonBlueprint.h>
#include <leon/core/Paths.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorDisplayNames.h>
#include <leon/editor/EditorCommands.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/EngineContent.h>
#include <leon/editor/LevelSaver.h>
#include <leon/level/BasicShape.h>
#include <leon/level/Light.h>
#include <nlohmann/json.hpp>
#include <string>

namespace leon::editor {
namespace {

nlohmann::json Vec3(const glm::vec3& v) {
    return nlohmann::json::array({v.x, v.y, v.z});
}

void WriteTransform(nlohmann::json& j, const Transform& t) {
    j["position"] = Vec3(t.position);
    j["rotation"] = Vec3(t.rotationDegrees);
    j["scale"] = Vec3(t.scale);
}

[[nodiscard]] nlohmann::json SerializeSelection(const EditorContext& ctx,
                                                const EditorSelection& sel) {
    nlohmann::json j;
    if (ctx.level == nullptr || !sel.IsValid()) {
        return j;
    }
    switch (sel.kind) {
    case EEditorSelectionKind::StaticMesh:
        if (sel.index < ctx.level->StaticMeshes().size()) {
            const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[sel.index];
            j["kind"] = "StaticMesh";
            j["class"] = mesh.editorClass.empty() ? "StaticMesh" : mesh.editorClass;
            WriteTransform(j, mesh.transform);
            j["tag"] = mesh.tag;
            j["material"] = mesh.materialPath;
            j["mesh"] = mesh.meshPath;
            j["collisionEnabled"] = mesh.collisionEnabled;
            j["simulatePhysics"] = mesh.simulatePhysics;
            j["enableGravity"] = mesh.enableGravity;
            j["hidden"] = mesh.hidden;
            j["mobility"] = mesh.mobility == EComponentMobility::Movable ? "Movable" : "Static";
            j["lightmapResolution"] = mesh.lightmapResolution;
        }
        break;
    case EEditorSelectionKind::PointLight:
        if (sel.index < ctx.level->PointLights().size()) {
            const PointLight& light = ctx.level->PointLights()[sel.index];
            j["kind"] = "PointLight";
            j["position"] = Vec3(light.transform.position);
            j["lightColor"] = Vec3(light.lightColor);
            j["intensity"] = light.intensity;
            j["range"] = light.range;
            j["castShadows"] = light.castShadows;
        }
        break;
    case EEditorSelectionKind::SpotLight:
        if (sel.index < ctx.level->SpotLights().size()) {
            const SpotLight& light = ctx.level->SpotLights()[sel.index];
            j["kind"] = "SpotLight";
            WriteTransform(j, light.transform);
            j["lightColor"] = Vec3(light.lightColor);
            j["intensity"] = light.intensity;
            j["range"] = light.range;
            j["innerConeAngle"] = light.innerConeAngle;
            j["outerConeAngle"] = light.outerConeAngle;
            j["castShadows"] = light.castShadows;
        }
        break;
    case EEditorSelectionKind::PlayerStart:
        if (sel.index < ctx.level->PlayerStarts().size()) {
            j["kind"] = "PlayerStart";
            WriteTransform(j, ctx.level->PlayerStarts()[sel.index].transform);
        }
        break;
    case EEditorSelectionKind::TriggerVolume:
        if (sel.index < ctx.level->TriggerVolumes().size()) {
            const TriggerVolume& volume = ctx.level->TriggerVolumes()[sel.index];
            j["kind"] = "TriggerVolume";
            WriteTransform(j, volume.transform);
            j["interactRadius"] = volume.interactRadius;
            j["interactCost"] = volume.interactCost;
            j["payload"] = volume.payload;
            j["tag"] = volume.tag;
            j["bConsumeOnUse"] = volume.bConsumeOnUse;
        }
        break;
    case EEditorSelectionKind::PainCausingVolume:
        if (sel.index < ctx.level->PainCausingVolumes().size()) {
            const PainCausingVolume& volume = ctx.level->PainCausingVolumes()[sel.index];
            j["kind"] = "PainCausingVolume";
            WriteTransform(j, volume.transform);
            j["damagePerSecond"] = volume.damagePerSecond;
            j["damageInterval"] = volume.damageInterval;
            j["tag"] = volume.tag;
        }
        break;
    case EEditorSelectionKind::AISpawnPoint:
        if (sel.index < ctx.level->AISpawnPoints().size()) {
            const AISpawnPoint& point = ctx.level->AISpawnPoints()[sel.index];
            j["kind"] = "AISpawnPoint";
            WriteTransform(j, point.transform);
            j["tag"] = point.tag;
        }
        break;
    case EEditorSelectionKind::TextRenderActor:
        if (sel.index < ctx.level->TextRenderActors().size()) {
            const TextRenderActor& tr = ctx.level->TextRenderActors()[sel.index];
            j["kind"] = "TextRenderActor";
            WriteTransform(j, tr.transform);
            j["Text"] = tr.Text;
            j["TextRenderColor"] = Vec3(tr.TextRenderColor);
            j["WorldSize"] = tr.WorldSize;
            j["HorizontalAlignment"] = static_cast<int>(tr.HorizontalAlignment);
            j["bHiddenInGame"] = tr.bHiddenInGame;
        }
        break;
    case EEditorSelectionKind::BlueprintInstance:
        if (sel.index < ctx.level->BlueprintInstances().size()) {
            const BlueprintInstance& bp = ctx.level->BlueprintInstances()[sel.index];
            j["kind"] = "BlueprintInstance";
            WriteTransform(j, bp.transform);
            j["blueprint"] = bp.blueprintPath;
            j["actorLabel"] = bp.actorLabel;
        }
        break;
    case EEditorSelectionKind::DirectionalLight:
        if (sel.index < ctx.level->DirectionalLights().size()) {
            const DirectionalLight& light = ctx.level->DirectionalLights()[sel.index];
            j["kind"] = "DirectionalLight";
            j["rotation"] = Vec3(light.transform.rotationDegrees);
            j["lightColor"] = Vec3(light.lightColor);
            j["intensity"] = light.intensity;
            j["castShadows"] = light.castShadows;
        }
        break;
    default:
        break;
    }
    return j;
}

void OffsetTransform(Transform& t, const glm::vec3& delta) {
    t.position += delta;
}

} // namespace

std::vector<EditorClipboardItem>& EditorCommands::Clipboard() {
    static std::vector<EditorClipboardItem> clip;
    return clip;
}

glm::vec3 EditorCommands::SnapPosition(const glm::vec3& p, float gridSize) {
    if (gridSize <= 1.0e-5f) {
        return p;
    }
    auto snap1 = [gridSize](float v) { return std::round(v / gridSize) * gridSize; };
    return {snap1(p.x), snap1(p.y), snap1(p.z)};
}

float EditorCommands::SnapAngle(float degrees, float stepDegrees) {
    if (stepDegrees <= 1.0e-5f) {
        return degrees;
    }
    return std::round(degrees / stepDegrees) * stepDegrees;
}

void EditorCommands::SnapTransform(Transform& t, float gridSize, float angleStep, bool snapScale) {
    t.position = SnapPosition(t.position, gridSize);
    t.rotationDegrees.x = SnapAngle(t.rotationDegrees.x, angleStep);
    t.rotationDegrees.y = SnapAngle(t.rotationDegrees.y, angleStep);
    t.rotationDegrees.z = SnapAngle(t.rotationDegrees.z, angleStep);
    if (snapScale) {
        t.scale = SnapPosition(t.scale, gridSize * 0.25f);
        t.scale = glm::max(t.scale, glm::vec3(0.01f));
    }
}

void EditorCommands::PushPlaceActorsRecent(EditorContext& ctx, EEditorPlaceActorsKind kind,
                                           const std::string& blueprintPath,
                                           const std::string& label) {
    if (kind == EEditorPlaceActorsKind::None) {
        return;
    }
    PlaceActorsRecentEntry entry;
    entry.kind = kind;
    entry.blueprintPath = blueprintPath;
    entry.label = label;
    ctx.placeActorsRecent.erase(
        std::remove_if(ctx.placeActorsRecent.begin(), ctx.placeActorsRecent.end(),
                       [&](const PlaceActorsRecentEntry& e) {
                           return e.kind == kind && e.blueprintPath == blueprintPath;
                       }),
        ctx.placeActorsRecent.end());
    ctx.placeActorsRecent.insert(ctx.placeActorsRecent.begin(), std::move(entry));
    constexpr std::size_t kMaxRecent = 8;
    if (ctx.placeActorsRecent.size() > kMaxRecent) {
        ctx.placeActorsRecent.resize(kMaxRecent);
    }
}

bool* EditorCommands::EditorLockedPtr(EditorContext& ctx, EEditorSelectionKind kind,
                                      std::size_t index) {
    if (ctx.level == nullptr) {
        return nullptr;
    }
    switch (kind) {
    case EEditorSelectionKind::StaticMesh:
        if (index < ctx.level->StaticMeshes().size()) {
            return &ctx.level->StaticMeshes()[index].editorLocked;
        }
        break;
    case EEditorSelectionKind::PointLight:
        if (index < ctx.level->PointLights().size()) {
            return &ctx.level->PointLights()[index].editorLocked;
        }
        break;
    case EEditorSelectionKind::SpotLight:
        if (index < ctx.level->SpotLights().size()) {
            return &ctx.level->SpotLights()[index].editorLocked;
        }
        break;
    case EEditorSelectionKind::DirectionalLight:
        if (index < ctx.level->DirectionalLights().size()) {
            return &ctx.level->DirectionalLights()[index].editorLocked;
        }
        break;
    case EEditorSelectionKind::PlayerStart:
        if (index < ctx.level->PlayerStarts().size()) {
            return &ctx.level->PlayerStarts()[index].editorLocked;
        }
        break;
    case EEditorSelectionKind::TriggerVolume:
        if (index < ctx.level->TriggerVolumes().size()) {
            return &ctx.level->TriggerVolumes()[index].editorLocked;
        }
        break;
    case EEditorSelectionKind::PainCausingVolume:
        if (index < ctx.level->PainCausingVolumes().size()) {
            return &ctx.level->PainCausingVolumes()[index].editorLocked;
        }
        break;
    case EEditorSelectionKind::AISpawnPoint:
        if (index < ctx.level->AISpawnPoints().size()) {
            return &ctx.level->AISpawnPoints()[index].editorLocked;
        }
        break;
    case EEditorSelectionKind::TextRenderActor:
        if (index < ctx.level->TextRenderActors().size()) {
            return &ctx.level->TextRenderActors()[index].editorLocked;
        }
        break;
    case EEditorSelectionKind::BlueprintInstance:
        if (index < ctx.level->BlueprintInstances().size()) {
            return &ctx.level->BlueprintInstances()[index].editorLocked;
        }
        break;
    default:
        break;
    }
    return nullptr;
}

bool* EditorCommands::HiddenPtr(EditorContext& ctx, EEditorSelectionKind kind, std::size_t index) {
    if (ctx.level == nullptr) {
        return nullptr;
    }
    if (kind == EEditorSelectionKind::StaticMesh) {
        if (index < ctx.level->StaticMeshes().size()) {
            return &ctx.level->StaticMeshes()[index].hidden;
        }
        return nullptr;
    }
    if (kind == EEditorSelectionKind::TextRenderActor) {
        if (index < ctx.level->TextRenderActors().size()) {
            return &ctx.level->TextRenderActors()[index].bHiddenInGame;
        }
    }
    return nullptr;
}

bool EditorCommands::IsEditorLocked(const EditorContext& ctx, EEditorSelectionKind kind,
                                    std::size_t index) {
    if (ctx.level == nullptr) {
        return false;
    }
    switch (kind) {
    case EEditorSelectionKind::StaticMesh:
        return index < ctx.level->StaticMeshes().size() &&
               ctx.level->StaticMeshes()[index].editorLocked;
    case EEditorSelectionKind::PointLight:
        return index < ctx.level->PointLights().size() &&
               ctx.level->PointLights()[index].editorLocked;
    case EEditorSelectionKind::SpotLight:
        return index < ctx.level->SpotLights().size() &&
               ctx.level->SpotLights()[index].editorLocked;
    case EEditorSelectionKind::DirectionalLight:
        return index < ctx.level->DirectionalLights().size() &&
               ctx.level->DirectionalLights()[index].editorLocked;
    case EEditorSelectionKind::PlayerStart:
        return index < ctx.level->PlayerStarts().size() &&
               ctx.level->PlayerStarts()[index].editorLocked;
    case EEditorSelectionKind::TriggerVolume:
        return index < ctx.level->TriggerVolumes().size() &&
               ctx.level->TriggerVolumes()[index].editorLocked;
    case EEditorSelectionKind::PainCausingVolume:
        return index < ctx.level->PainCausingVolumes().size() &&
               ctx.level->PainCausingVolumes()[index].editorLocked;
    case EEditorSelectionKind::AISpawnPoint:
        return index < ctx.level->AISpawnPoints().size() &&
               ctx.level->AISpawnPoints()[index].editorLocked;
    case EEditorSelectionKind::TextRenderActor:
        return index < ctx.level->TextRenderActors().size() &&
               ctx.level->TextRenderActors()[index].editorLocked;
    case EEditorSelectionKind::BlueprintInstance:
        return index < ctx.level->BlueprintInstances().size() &&
               ctx.level->BlueprintInstances()[index].editorLocked;
    default:
        return false;
    }
}

bool EditorCommands::PlaceEditorActor(EditorContext& ctx, EEditorPlaceActorsKind kind,
                                      const glm::vec3& worldPos, const std::string& blueprintPath) {
    if (ctx.level == nullptr || ctx.resources == nullptr || kind == EEditorPlaceActorsKind::None) {
        return false;
    }

    // Validate early so a failed Place does not push a useless undo transaction.
    if (kind == EEditorPlaceActorsKind::PointLight &&
        ctx.level->PointLights().size() >= static_cast<std::size_t>(kMaxPointLights)) {
        EditorToast("Maximum point lights (" + std::to_string(kMaxPointLights) + ") reached",
                    EEditorToastKind::Warning, 3.0f);
        return false;
    }
    if (kind == EEditorPlaceActorsKind::SpotLight &&
        ctx.level->SpotLights().size() >= static_cast<std::size_t>(kMaxSpotLights)) {
        EditorToast("Maximum spot lights (" + std::to_string(kMaxSpotLights) + ") reached",
                    EEditorToastKind::Warning, 3.0f);
        return false;
    }
    if (kind == EEditorPlaceActorsKind::DirectionalLight &&
        ctx.level->DirectionalLights().size() >= static_cast<std::size_t>(kMaxDirectionalLights)) {
        EditorToast("Maximum directional lights (" + std::to_string(kMaxDirectionalLights) + ") reached",
                    EEditorToastKind::Warning, 3.0f);
        return false;
    }
    if (kind == EEditorPlaceActorsKind::BlueprintClass && blueprintPath.empty()) {
        return false;
    }

    glm::vec3 pos = worldPos;
    if (ctx.snapEnabled) {
        Transform t{};
        t.position = pos;
        SnapTransform(t, ctx.gridSize, ctx.rotationSnapDegrees, false);
        pos = t.position;
    }

    auto tryCapturePlace = [&]() -> bool {
        return ctx.history != nullptr && ctx.history->Capture(ctx, "Place Actors");
    };

    auto discardPlaceCapture = [&](bool captured) {
        if (captured && ctx.history != nullptr) {
            ctx.history->DiscardLastCapture();
        }
    };

    auto finish = [&](EEditorPlaceActorsKind placedKind, const std::string& bpPath,
                      const std::string& label) {
        PushPlaceActorsRecent(ctx, placedKind, bpPath, label);
        if (placedKind == EEditorPlaceActorsKind::PointLight ||
            placedKind == EEditorPlaceActorsKind::SpotLight ||
            placedKind == EEditorPlaceActorsKind::DirectionalLight) {
            ctx.MarkLightingOutOfDate();
        }
        ctx.MarkDirty();
        return true;
    };

    switch (kind) {
    case EEditorPlaceActorsKind::Cube:
    case EEditorPlaceActorsKind::Sphere:
    case EEditorPlaceActorsKind::Plane:
    case EEditorPlaceActorsKind::Cylinder: {
        EBasicShape shape = EBasicShape::Cube;
        const char* label = "Cube";
        if (kind == EEditorPlaceActorsKind::Sphere) {
            shape = EBasicShape::Sphere;
            label = "Sphere";
        } else if (kind == EEditorPlaceActorsKind::Plane) {
            shape = EBasicShape::Plane;
            label = "Plane";
        } else if (kind == EEditorPlaceActorsKind::Cylinder) {
            shape = EBasicShape::Cylinder;
            label = "Cylinder";
        }
        const bool captured = tryCapturePlace();
        std::string err;
        if (!PlaceBasicShapeActor(ctx, shape, pos, err)) {
            discardPlaceCapture(captured);
            if (!err.empty()) {
                std::cerr << "Place Actors: " << err << '\n';
            }
            return false;
        }
        return finish(kind, {}, label);
    }
    case EEditorPlaceActorsKind::BlockingVolume: {
        (void)tryCapturePlace();
        BasicShape basic;
        basic.type = EBasicShape::Cube;
        basic.transform.position = pos;
        basic.transform.position.y = pos.y + 0.5f;
        StaticMeshComponent mesh = basic.MakeStaticMesh(*ctx.resources);
        mesh.editorClass = "BlockingVolume";
        mesh.hidden = true;
        mesh.collisionEnabled = true;
        mesh.materialPath = "Materials/M_Default.lmat";
        mesh.material = ctx.resources->LoadMaterial(ResolveAssetPath(mesh.materialPath));
        if (!mesh.material.albedoMap) {
            mesh.material = ctx.resources->DefaultMaterial();
        }
        mesh.material.castsShadows = false;
        mesh.materialOverride = true;
        ctx.level->AddStaticMesh(std::move(mesh));
        ctx.Select(EEditorSelectionKind::StaticMesh, ctx.level->StaticMeshes().size() - 1);
        return finish(kind, {}, PlaceActorsKindLabel(kind));
    }
    case EEditorPlaceActorsKind::PointLight: {
        (void)tryCapturePlace();
        PointLight light{};
        light.transform.position = pos + glm::vec3{0, 2, 0};
        ctx.level->PointLights().push_back(light);
        ctx.Select(EEditorSelectionKind::PointLight, ctx.level->PointLights().size() - 1);
        return finish(kind, {}, PlaceActorsKindLabel(kind));
    }
    case EEditorPlaceActorsKind::SpotLight: {
        (void)tryCapturePlace();
        SpotLight light{};
        light.transform.position = pos + glm::vec3{0, 3, 0};
        ctx.level->SpotLights().push_back(light);
        ctx.Select(EEditorSelectionKind::SpotLight, ctx.level->SpotLights().size() - 1);
        return finish(kind, {}, PlaceActorsKindLabel(kind));
    }
    case EEditorPlaceActorsKind::DirectionalLight: {
        (void)tryCapturePlace();
        DirectionalLight light{};
        light.transform.position = pos;
        ctx.level->DirectionalLights().push_back(light);
        ctx.Select(EEditorSelectionKind::DirectionalLight,
                   ctx.level->DirectionalLights().size() - 1);
        return finish(kind, {}, PlaceActorsKindLabel(kind));
    }
    case EEditorPlaceActorsKind::TriggerVolume: {
        (void)tryCapturePlace();
        TriggerVolume volume{};
        volume.transform.position = pos;
        volume.transform.scale = {2.0f, 2.0f, 2.0f};
        volume.interactRadius = 2.0f;
        volume.payload = "Door";
        ctx.level->AddTriggerVolume(volume);
        ctx.Select(EEditorSelectionKind::TriggerVolume, ctx.level->TriggerVolumes().size() - 1);
        return finish(kind, {}, PlaceActorsKindLabel(kind));
    }
    case EEditorPlaceActorsKind::PainCausingVolume: {
        (void)tryCapturePlace();
        PainCausingVolume volume{};
        volume.transform.position = pos;
        volume.transform.scale = {4.0f, 0.5f, 4.0f};
        ctx.level->AddPainCausingVolume(volume);
        ctx.Select(EEditorSelectionKind::PainCausingVolume,
                   ctx.level->PainCausingVolumes().size() - 1);
        return finish(kind, {}, PlaceActorsKindLabel(kind));
    }
    case EEditorPlaceActorsKind::PlayerStart: {
        (void)tryCapturePlace();
        PlayerStart start{};
        start.transform.position = pos;
        ctx.level->AddPlayerStart(start);
        ctx.Select(EEditorSelectionKind::PlayerStart, ctx.level->PlayerStarts().size() - 1);
        return finish(kind, {}, PlaceActorsKindLabel(kind));
    }
    case EEditorPlaceActorsKind::AISpawnPoint: {
        (void)tryCapturePlace();
        AISpawnPoint point{};
        point.transform.position = pos;
        ctx.level->AddAISpawnPoint(point);
        ctx.Select(EEditorSelectionKind::AISpawnPoint, ctx.level->AISpawnPoints().size() - 1);
        return finish(kind, {}, PlaceActorsKindLabel(kind));
    }
    case EEditorPlaceActorsKind::TextRenderActor: {
        (void)tryCapturePlace();
        TextRenderActor tr{};
        tr.transform.position = pos;
        tr.Text = "Text";
        ctx.level->AddTextRenderActor(tr);
        ctx.Select(EEditorSelectionKind::TextRenderActor, ctx.level->TextRenderActors().size() - 1);
        return finish(kind, {}, PlaceActorsKindLabel(kind));
    }
    case EEditorPlaceActorsKind::BlueprintClass: {
        Transform root{};
        root.position = pos;
        const bool captured = tryCapturePlace();
        if (!PlaceBlueprintInLevel(*ctx.resources, *ctx.level, blueprintPath, root, {},
                                   ctx.levelPath, nullptr)) {
            discardPlaceCapture(captured);
            std::cerr << "Place Actors: failed to place Blueprint " << blueprintPath << '\n';
            return false;
        }
        ctx.Select(EEditorSelectionKind::BlueprintInstance,
                   ctx.level->BlueprintInstances().size() - 1);
        const std::string label = std::filesystem::path(blueprintPath).stem().string();
        return finish(kind, blueprintPath, label.empty() ? "Blueprint" : label);
    }
    case EEditorPlaceActorsKind::None:
        break;
    }
    return false;
}

void EditorCommands::DeleteSelection(EditorContext& ctx, EditorHistory* history) {
    if (ctx.level == nullptr || ctx.selected.empty()) {
        return;
    }
    if (history != nullptr) {
        history->Capture(ctx, "Delete Actors");
    }

    // Delete highest indices first per kind to keep indices stable.
    auto selected = ctx.selected;
    std::sort(selected.begin(), selected.end(),
              [](const EditorSelection& a, const EditorSelection& b) {
                  if (a.kind != b.kind) {
                      return static_cast<int>(a.kind) < static_cast<int>(b.kind);
                  }
                  return a.index > b.index;
              });

    for (const EditorSelection& sel : selected) {
        switch (sel.kind) {
        case EEditorSelectionKind::StaticMesh:
            if (sel.index < ctx.level->StaticMeshes().size()) {
                ctx.level->StaticMeshes().erase(ctx.level->StaticMeshes().begin() +
                                                static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::DirectionalLight:
            if (sel.index < ctx.level->DirectionalLights().size() &&
                ctx.level->DirectionalLights().size() > 1) {
                ctx.level->DirectionalLights().erase(ctx.level->DirectionalLights().begin() +
                                                     static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::PointLight:
            if (sel.index < ctx.level->PointLights().size()) {
                ctx.level->PointLights().erase(ctx.level->PointLights().begin() +
                                               static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::SpotLight:
            if (sel.index < ctx.level->SpotLights().size()) {
                ctx.level->SpotLights().erase(ctx.level->SpotLights().begin() +
                                              static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::PlayerStart:
            if (sel.index < ctx.level->PlayerStarts().size()) {
                ctx.level->PlayerStarts().erase(ctx.level->PlayerStarts().begin() +
                                                static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::TriggerVolume:
            if (sel.index < ctx.level->TriggerVolumes().size()) {
                ctx.level->TriggerVolumes().erase(ctx.level->TriggerVolumes().begin() +
                                                  static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::PainCausingVolume:
            if (sel.index < ctx.level->PainCausingVolumes().size()) {
                ctx.level->PainCausingVolumes().erase(ctx.level->PainCausingVolumes().begin() +
                                                      static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::AISpawnPoint:
            if (sel.index < ctx.level->AISpawnPoints().size()) {
                ctx.level->AISpawnPoints().erase(ctx.level->AISpawnPoints().begin() +
                                                 static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::TextRenderActor:
            if (sel.index < ctx.level->TextRenderActors().size()) {
                ctx.level->TextRenderActors().erase(ctx.level->TextRenderActors().begin() +
                                                    static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::BlueprintInstance:
            if (sel.index < ctx.level->BlueprintInstances().size()) {
                const std::uint64_t id = ctx.level->BlueprintInstances()[sel.index].instanceId;
                ctx.level->RemoveBlueprintInstanceChildren(id);
                ctx.level->BlueprintInstances().erase(ctx.level->BlueprintInstances().begin() +
                                                      static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        default:
            break;
        }
    }
    ctx.ClearSelection();
    ctx.MarkDirty();
}

void EditorCommands::DuplicateSelection(EditorContext& ctx, EditorHistory* history) {
    CopySelection(ctx);
    PasteClipboard(ctx, history);
}

void EditorCommands::CopySelection(EditorContext& ctx) {
    Clipboard().clear();
    if (ctx.level == nullptr) {
        return;
    }
    const auto& list =
        ctx.selected.empty() ? std::vector<EditorSelection>{ctx.selection} : ctx.selected;
    for (const EditorSelection& sel : list) {
        nlohmann::json j = SerializeSelection(ctx, sel);
        if (j.is_null() || j.empty() || !j.contains("kind")) {
            continue;
        }
        EditorClipboardItem item;
        item.kind = sel.kind;
        item.json = j.dump();
        Clipboard().push_back(std::move(item));
    }
}

void EditorCommands::PasteClipboard(EditorContext& ctx, EditorHistory* history) {
    if (ctx.level == nullptr || Clipboard().empty()) {
        return;
    }
    if (history != nullptr) {
        history->Capture(ctx, "Paste Actors");
    }
    ctx.ClearSelection();
    constexpr glm::vec3 kOffset{1.0f, 0.0f, 1.0f};

    for (const EditorClipboardItem& item : Clipboard()) {
        nlohmann::json j = nlohmann::json::parse(item.json, nullptr, false);
        if (j.is_discarded()) {
            continue;
        }
        const std::string kind = j.value("kind", "");
        if (kind == "StaticMesh") {
            StaticMeshComponent mesh;
            const std::string cls = j.value("class", "Cube");
            const std::string meshPath = j.value("mesh", "");
            bool placed = false;
            if (ctx.resources != nullptr) {
                EBasicShape shape{};
                if (tryParseBasicShapeName(cls, shape)) {
                    BasicShape basic;
                    basic.type = shape;
                    mesh = basic.MakeStaticMesh(*ctx.resources);
                    mesh.editorClass = cls;
                    placed = true;
                } else if (!meshPath.empty()) {
                    mesh.mesh = ctx.resources->LoadStaticMesh(ResolveAssetPath(meshPath));
                    if (mesh.mesh != nullptr) {
                        mesh.meshPath = meshPath;
                        mesh.editorClass = cls.empty() ? "StaticMesh" : cls;
                        mesh.material = ctx.resources->DefaultMaterial();
                        placed = true;
                    }
                }
            }
            if (!placed) {
                for (const StaticMeshComponent& src : ctx.level->StaticMeshes()) {
                    if ((!meshPath.empty() && src.meshPath == meshPath) || src.editorClass == cls ||
                        (cls == "StaticMesh" && !src.meshPath.empty())) {
                        mesh = src;
                        placed = true;
                        break;
                    }
                }
            }
            if (!placed) {
                std::cerr << "Paste: no template mesh for class " << cls << '\n';
                continue;
            }
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                mesh.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                           j["position"][1].get<float>() + kOffset.y,
                                           j["position"][2].get<float>() + kOffset.z};
            } else {
                OffsetTransform(mesh.transform, kOffset);
            }
            if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
                mesh.transform.rotationDegrees = {j["rotation"][0].get<float>(),
                                                  j["rotation"][1].get<float>(),
                                                  j["rotation"][2].get<float>()};
            }
            if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
                mesh.transform.scale = {j["scale"][0].get<float>(), j["scale"][1].get<float>(),
                                        j["scale"][2].get<float>()};
            }
            mesh.tag = j.value("tag", mesh.tag);
            mesh.editorClass = cls;
            if (!meshPath.empty()) {
                mesh.meshPath = meshPath;
            }
            const std::string matPath = j.value("material", "");
            if (!matPath.empty() && ctx.resources != nullptr) {
                mesh.material = ctx.resources->LoadMaterial(matPath);
                mesh.materialPath = matPath;
                mesh.materialOverride = true;
            }
            mesh.collisionEnabled = j.value("collisionEnabled", mesh.collisionEnabled);
            mesh.simulatePhysics = j.value("simulatePhysics", mesh.simulatePhysics);
            mesh.enableGravity = j.value("enableGravity", mesh.enableGravity);
            mesh.hidden = j.value("hidden", mesh.hidden);
            mesh.lightmap.reset();
            mesh.lightmapPath.clear();
            ctx.level->AddStaticMesh(std::move(mesh));
            ctx.Select(EEditorSelectionKind::StaticMesh, ctx.level->StaticMeshes().size() - 1,
                       true);
        } else if (kind == "PointLight") {
            PointLight light{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                light.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                            j["position"][1].get<float>() + kOffset.y,
                                            j["position"][2].get<float>() + kOffset.z};
            }
            if (j.contains("lightColor") && j["lightColor"].is_array() &&
                j["lightColor"].size() >= 3) {
                light.lightColor = {j["lightColor"][0].get<float>(),
                                    j["lightColor"][1].get<float>(),
                                    j["lightColor"][2].get<float>()};
            }
            light.intensity = j.value("intensity", light.intensity);
            light.range = j.value("range", light.range);
            light.castShadows = j.value("castShadows", light.castShadows);
            ctx.level->PointLights().push_back(light);
            ctx.Select(EEditorSelectionKind::PointLight, ctx.level->PointLights().size() - 1, true);
        } else if (kind == "SpotLight") {
            SpotLight light{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                light.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                            j["position"][1].get<float>() + kOffset.y,
                                            j["position"][2].get<float>() + kOffset.z};
            }
            if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
                light.transform.rotationDegrees = {j["rotation"][0].get<float>(),
                                                   j["rotation"][1].get<float>(),
                                                   j["rotation"][2].get<float>()};
            }
            if (j.contains("lightColor") && j["lightColor"].is_array() &&
                j["lightColor"].size() >= 3) {
                light.lightColor = {j["lightColor"][0].get<float>(),
                                    j["lightColor"][1].get<float>(),
                                    j["lightColor"][2].get<float>()};
            }
            light.intensity = j.value("intensity", light.intensity);
            light.range = j.value("range", light.range);
            light.innerConeAngle = j.value("innerConeAngle", light.innerConeAngle);
            light.outerConeAngle = j.value("outerConeAngle", light.outerConeAngle);
            light.castShadows = j.value("castShadows", light.castShadows);
            ctx.level->SpotLights().push_back(light);
            ctx.Select(EEditorSelectionKind::SpotLight, ctx.level->SpotLights().size() - 1, true);
        } else if (kind == "PlayerStart") {
            PlayerStart start{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                start.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                            j["position"][1].get<float>() + kOffset.y,
                                            j["position"][2].get<float>() + kOffset.z};
            }
            ctx.level->AddPlayerStart(start);
            ctx.Select(EEditorSelectionKind::PlayerStart, ctx.level->PlayerStarts().size() - 1,
                       true);
        } else if (kind == "TriggerVolume") {
            TriggerVolume volume{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                volume.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                             j["position"][1].get<float>() + kOffset.y,
                                             j["position"][2].get<float>() + kOffset.z};
            }
            if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
                volume.transform.rotationDegrees = {j["rotation"][0].get<float>(),
                                                    j["rotation"][1].get<float>(),
                                                    j["rotation"][2].get<float>()};
            }
            if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
                volume.transform.scale = {j["scale"][0].get<float>(), j["scale"][1].get<float>(),
                                          j["scale"][2].get<float>()};
            }
            volume.interactRadius = j.value("interactRadius", volume.interactRadius);
            volume.interactCost = j.value("interactCost", volume.interactCost);
            volume.payload = j.value("payload", volume.payload);
            volume.tag = j.value("tag", volume.tag);
            volume.bConsumeOnUse = j.value("bConsumeOnUse", volume.bConsumeOnUse);
            ctx.level->AddTriggerVolume(volume);
            ctx.Select(EEditorSelectionKind::TriggerVolume, ctx.level->TriggerVolumes().size() - 1,
                       true);
        } else if (kind == "PainCausingVolume") {
            PainCausingVolume volume{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                volume.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                             j["position"][1].get<float>() + kOffset.y,
                                             j["position"][2].get<float>() + kOffset.z};
            }
            if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
                volume.transform.rotationDegrees = {j["rotation"][0].get<float>(),
                                                    j["rotation"][1].get<float>(),
                                                    j["rotation"][2].get<float>()};
            }
            if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
                volume.transform.scale = {j["scale"][0].get<float>(), j["scale"][1].get<float>(),
                                          j["scale"][2].get<float>()};
            }
            volume.damagePerSecond = j.value("damagePerSecond", volume.damagePerSecond);
            volume.damageInterval = j.value("damageInterval", volume.damageInterval);
            volume.tag = j.value("tag", volume.tag);
            ctx.level->AddPainCausingVolume(volume);
            ctx.Select(EEditorSelectionKind::PainCausingVolume,
                       ctx.level->PainCausingVolumes().size() - 1, true);
        } else if (kind == "AISpawnPoint") {
            AISpawnPoint point{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                point.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                            j["position"][1].get<float>() + kOffset.y,
                                            j["position"][2].get<float>() + kOffset.z};
            }
            if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
                point.transform.rotationDegrees = {j["rotation"][0].get<float>(),
                                                   j["rotation"][1].get<float>(),
                                                   j["rotation"][2].get<float>()};
            }
            point.tag = j.value("tag", point.tag);
            ctx.level->AddAISpawnPoint(point);
            ctx.Select(EEditorSelectionKind::AISpawnPoint, ctx.level->AISpawnPoints().size() - 1,
                       true);
        } else if (kind == "TextRenderActor") {
            TextRenderActor tr{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                tr.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                         j["position"][1].get<float>() + kOffset.y,
                                         j["position"][2].get<float>() + kOffset.z};
            }
            if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
                tr.transform.rotationDegrees = {j["rotation"][0].get<float>(),
                                                j["rotation"][1].get<float>(),
                                                j["rotation"][2].get<float>()};
            }
            tr.Text = j.value("Text", tr.Text);
            if (j.contains("TextRenderColor") && j["TextRenderColor"].is_array() &&
                j["TextRenderColor"].size() >= 3) {
                tr.TextRenderColor = {j["TextRenderColor"][0].get<float>(),
                                      j["TextRenderColor"][1].get<float>(),
                                      j["TextRenderColor"][2].get<float>()};
            }
            tr.WorldSize = j.value("WorldSize", tr.WorldSize);
            const int align =
                j.value("HorizontalAlignment", static_cast<int>(tr.HorizontalAlignment));
            if (align >= 0 && align <= static_cast<int>(ETextJustify::Right)) {
                tr.HorizontalAlignment = static_cast<ETextJustify>(align);
            }
            tr.bHiddenInGame = j.value("bHiddenInGame", tr.bHiddenInGame);
            ctx.level->AddTextRenderActor(tr);
            ctx.Select(EEditorSelectionKind::TextRenderActor,
                       ctx.level->TextRenderActors().size() - 1, true);
        } else if (kind == "BlueprintInstance" && ctx.resources != nullptr) {
            Transform root{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                root.position = {j["position"][0].get<float>() + kOffset.x,
                                 j["position"][1].get<float>() + kOffset.y,
                                 j["position"][2].get<float>() + kOffset.z};
            }
            if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
                root.rotationDegrees = {j["rotation"][0].get<float>(),
                                        j["rotation"][1].get<float>(),
                                        j["rotation"][2].get<float>()};
            }
            if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
                root.scale = {j["scale"][0].get<float>(), j["scale"][1].get<float>(),
                              j["scale"][2].get<float>()};
            }
            const std::string bpPath = j.value("blueprint", std::string{});
            const std::string label = j.value("actorLabel", std::string{});
            if (!bpPath.empty() && PlaceBlueprintInLevel(*ctx.resources, *ctx.level, bpPath, root,
                                                         label, ctx.levelPath, nullptr)) {
                ctx.Select(EEditorSelectionKind::BlueprintInstance,
                           ctx.level->BlueprintInstances().size() - 1, true);
            }
        }
    }
    ctx.MarkDirty();
}

std::string* EditorCommands::ActorLabelPtr(EditorContext& ctx, EEditorSelectionKind kind,
                                           std::size_t index) {
    if (ctx.level == nullptr) {
        return nullptr;
    }
    switch (kind) {
    case EEditorSelectionKind::StaticMesh:
        if (index < ctx.level->StaticMeshes().size()) {
            return &ctx.level->StaticMeshes()[index].tag;
        }
        break;
    case EEditorSelectionKind::TriggerVolume:
        if (index < ctx.level->TriggerVolumes().size()) {
            return &ctx.level->TriggerVolumes()[index].tag;
        }
        break;
    case EEditorSelectionKind::PainCausingVolume:
        if (index < ctx.level->PainCausingVolumes().size()) {
            return &ctx.level->PainCausingVolumes()[index].tag;
        }
        break;
    case EEditorSelectionKind::AISpawnPoint:
        if (index < ctx.level->AISpawnPoints().size()) {
            return &ctx.level->AISpawnPoints()[index].tag;
        }
        break;
    case EEditorSelectionKind::TextRenderActor:
        if (index < ctx.level->TextRenderActors().size()) {
            return &ctx.level->TextRenderActors()[index].Text;
        }
        break;
    case EEditorSelectionKind::BlueprintInstance:
        if (index < ctx.level->BlueprintInstances().size()) {
            return &ctx.level->BlueprintInstances()[index].actorLabel;
        }
        break;
    default:
        break;
    }
    return nullptr;
}

bool EditorCommands::CanRenameSelection(const EditorContext& ctx) {
    if (!ctx.selection.IsValid() || ctx.level == nullptr) {
        return false;
    }
    const std::size_t index = ctx.selection.index;
    switch (ctx.selection.kind) {
    case EEditorSelectionKind::StaticMesh:
        return index < ctx.level->StaticMeshes().size();
    case EEditorSelectionKind::TriggerVolume:
        return index < ctx.level->TriggerVolumes().size();
    case EEditorSelectionKind::PainCausingVolume:
        return index < ctx.level->PainCausingVolumes().size();
    case EEditorSelectionKind::AISpawnPoint:
        return index < ctx.level->AISpawnPoints().size();
    case EEditorSelectionKind::TextRenderActor:
        return index < ctx.level->TextRenderActors().size();
    case EEditorSelectionKind::BlueprintInstance:
        return index < ctx.level->BlueprintInstances().size();
    default:
        return false;
    }
}

std::string EditorCommands::BrowsePathForSelection(const EditorContext& ctx,
                                                   const EditorSelection& sel) {
    if (ctx.level == nullptr) {
        return {};
    }
    switch (sel.kind) {
    case EEditorSelectionKind::StaticMesh:
        if (sel.index < ctx.level->StaticMeshes().size()) {
            const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[sel.index];
            if (!mesh.meshPath.empty()) {
                return mesh.meshPath;
            }
            if (!mesh.materialPath.empty()) {
                return mesh.materialPath;
            }
        }
        break;
    case EEditorSelectionKind::BlueprintInstance:
        if (sel.index < ctx.level->BlueprintInstances().size()) {
            return ctx.level->BlueprintInstances()[sel.index].blueprintPath;
        }
        break;
    default:
        break;
    }
    return {};
}

bool EditorCommands::DrawSelectionContextMenuItems(EditorContext& ctx) {
    bool requestRename = false;
    const bool hasSel = ctx.selection.IsValid();
    const bool canRename = CanRenameSelection(ctx);

    if (ImGui::MenuItem("Focus Selected", "F", false, hasSel)) {
        ctx.RequestFocusSelected();
    }
    if (ImGui::MenuItem("Rename", "F2", false, canRename)) {
        requestRename = true;
    }
    if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasSel)) {
        DuplicateSelection(ctx, ctx.history);
    }
    if (ImGui::MenuItem("Delete", "Del", false, hasSel)) {
        DeleteSelection(ctx, ctx.history);
    }

    if (hasSel) {
        if (bool* hidden = HiddenPtr(ctx, ctx.selection.kind, ctx.selection.index)) {
            const bool isHidden = *hidden;
            if (ImGui::MenuItem(isHidden ? "Show" : "Hide")) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx, "Toggle Visibility");
                }
                *hidden = !*hidden;
                ctx.MarkDirty();
            }
        }
        if (bool* locked = EditorLockedPtr(ctx, ctx.selection.kind, ctx.selection.index)) {
            const bool isLocked = *locked;
            if (ImGui::MenuItem(isLocked ? "Unlock" : "Lock")) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx, "Toggle Lock");
                }
                *locked = !*locked;
                ctx.MarkDirty();
            }
        }
        const std::string browse = BrowsePathForSelection(ctx, ctx.selection);
        if (!browse.empty() && ImGui::MenuItem("Browse to Asset")) {
            const std::string abs = ResolveAssetPath(browse);
            ctx.RevealInContentBrowser(abs.empty() ? browse : abs);
        }
    }

    if (hasSel) {
        ImGui::Separator();
        if (ImGui::MenuItem("Clear Selection", "Esc")) {
            ctx.ClearSelection();
        }
    }
    return requestRename;
}

} // namespace leon::editor
