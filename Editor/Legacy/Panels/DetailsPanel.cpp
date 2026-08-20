#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <leon/content/LeonBlueprint.h>
#include <leon/core/Paths.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorDisplayNames.h>
#include <leon/editor/EditorCatalogs.h>
#include <leon/editor/EditorCommands.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/EditorToast.h>
#include <leon/level/LightmapBaker.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/DetailsPanel.h>
#include <leon/editor/ReflectDraw.h>
#include <leon/editor/StaticMeshAssetPath.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/gameplay/Actor.h>
#include <leon/gameplay/Character.h>
#include <leon/gameplay/World.h>
#include <leon/level/LeonLevelFormat.h>
#include <leon/level/Light.h>
#include <leon/physics/PhysicsAsset.h>
#include <leon/render/Renderer.h>
#include <leon/render/ResourceCache.h>
#include <typeinfo>

namespace leon::editor {
namespace {

bool EditTransformFields(Transform& t, EditorContext& ctx,
                         bool* outDeactivatedAfterEdit = nullptr) {
    bool changed = false;
    bool deactivated = false;
    auto drag3 = [&](const char* id, const char* label, float* v, float speed) {
        if (ui::DragFloat3(id, label, v, speed)) {
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                deactivated = true;
            }
            if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            return true;
        }
        return false;
    };
    changed |= drag3("##loc", "Location", &t.position.x, 0.05f);
    changed |= drag3("##rot", "Rotation", &t.rotationDegrees.x, 0.5f);
    changed |= drag3("##scale", "Scale", &t.scale.x, 0.05f);
    if (outDeactivatedAfterEdit != nullptr) {
        *outDeactivatedAfterEdit = deactivated;
    }
    return changed;
}

[[nodiscard]] const char* DemangleType(const char* name) {
    if (name == nullptr) {
        return "Unknown";
    }
    if (std::strncmp(name, "class ", 6) == 0) {
        return name + 6;
    }
    if (std::strncmp(name, "struct ", 7) == 0) {
        return name + 7;
    }
    return name;
}

[[nodiscard]] std::string MaterialDisplayLabel(const std::string& path) {
    if (path.empty()) {
        return "(none)";
    }
    const auto slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

} // namespace

void DetailsPanel::RefreshMaterialList(EditorContext& ctx) {
    if (ctx.requestContentRefresh) {
        materialsProjectKey_.clear();
    }
    if (!materialsProjectKey_.empty() && materialsProjectKey_ == ctx.projectPath &&
        !materials_.empty()) {
        return;
    }
    materialsProjectKey_ = ctx.projectPath;
    materials_ = CollectEditorMaterials(ctx.projectPath);
}

void DetailsPanel::DrawMaterialPicker(EditorContext& ctx, StaticMeshComponent& mesh) {
    RefreshMaterialList(ctx);

    if (ctx.resources == nullptr) {
        ImGui::TextDisabled("Materials unavailable (no ResourceCache)");
        return;
    }

    const std::size_t slotCount = [&]() -> std::size_t {
        if (mesh.mesh != nullptr && mesh.mesh->HasMaterials()) {
            return mesh.mesh->Materials().size();
        }
        return 1;
    }();

    auto applySlotMaterial = [&](std::size_t slot, const std::string& authoringPath,
                                 const std::string& loadAbs) {
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx, "Set Material Slot");
        }
        mesh.materialOverride = false;
        mesh.materialPath.clear();
        if (mesh.materialPaths.size() < slotCount) {
            mesh.materialPaths.resize(slotCount);
        }
        if (mesh.materials.size() < slotCount) {
            mesh.materials.resize(slotCount);
            if (mesh.mesh != nullptr && mesh.mesh->HasMaterials()) {
                for (std::size_t i = 0; i < slotCount; ++i) {
                    if (i < mesh.mesh->Materials().size()) {
                        mesh.materials[i] = mesh.mesh->Materials()[i];
                    }
                }
            }
        }
        mesh.materialPaths[slot] = authoringPath;
        mesh.materials[slot] = ctx.resources->LoadMaterial(loadAbs);
        ctx.MarkDirty();
    };

    auto clearSlotToMeshDefault = [&](std::size_t slot) {
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx, "Reset Material Slot");
        }
        mesh.materialOverride = false;
        mesh.materialPath.clear();
        if (slot < mesh.materialPaths.size()) {
            mesh.materialPaths[slot].clear();
        }
        if (mesh.materials.size() < slotCount) {
            mesh.materials.resize(slotCount);
        }
        if (mesh.mesh != nullptr && slot < mesh.mesh->Materials().size()) {
            if (mesh.materials.size() <= slot) {
                mesh.materials.resize(slot + 1);
            }
            mesh.materials[slot] = mesh.mesh->Materials()[slot];
        }
        // Drop empty trailing overrides so save omits unused slots when all cleared.
        while (!mesh.materialPaths.empty() && mesh.materialPaths.back().empty()) {
            mesh.materialPaths.pop_back();
        }
        if (mesh.materialPaths.empty()) {
            mesh.materials.clear();
        }
        ctx.MarkDirty();
    };

    ImGui::TextUnformatted(slotCount > 1 ? "Material slots" : "Material");
    for (std::size_t slot = 0; slot < slotCount; ++slot) {
        ImGui::PushID(static_cast<int>(slot));
        std::string currentPath;
        if (slot < mesh.materialPaths.size() && !mesh.materialPaths[slot].empty()) {
            currentPath = mesh.materialPaths[slot];
        } else if (mesh.materialOverride && !mesh.materialPath.empty()) {
            currentPath = mesh.materialPath;
        } else if (mesh.mesh != nullptr && slot < mesh.mesh->CpuData().materialSlotPaths.size()) {
            currentPath = mesh.mesh->CpuData().materialSlotPaths[slot];
        } else if (!mesh.materialPath.empty() && slotCount == 1) {
            currentPath = mesh.materialPath;
        }

        std::string slotLabel = "Slot " + std::to_string(slot);
        if (mesh.mesh != nullptr && slot < mesh.mesh->CpuData().materialSlotNames.size() &&
            !mesh.mesh->CpuData().materialSlotNames[slot].empty()) {
            slotLabel += " (" + mesh.mesh->CpuData().materialSlotNames[slot] + ")";
        }

        const std::string previewLabel =
            currentPath.empty() ? std::string("(mesh default)") : MaterialDisplayLabel(currentPath);
        ui::SectionLabel(slotLabel.c_str());
        ui::UiSelectDesc slotDesc;
        slotDesc.preview = previewLabel.c_str();
        slotDesc.previewIcon = ui::UiIcon(ELucideIcon::Layers);
        slotDesc.width = -1.0f;
        if (ui::BeginSelect("##mat_slot", slotDesc)) {
            if (ui::SelectItem("(mesh default)", currentPath.empty())) {
                clearSlotToMeshDefault(slot);
            }
            for (const EditorMaterialEntry& entry : materials_) {
                const bool selected =
                    entry.authoringPath == currentPath || entry.absolutePath == currentPath;
                if (ui::SelectItem(entry.displayName.c_str(), selected,
                                   ui::UiIcon(ELucideIcon::Layers))) {
                    const std::string loadPath = entry.absolutePath.empty()
                                                     ? ResolveAssetPath(entry.authoringPath)
                                                     : entry.absolutePath;
                    applySlotMaterial(slot, entry.authoringPath, loadPath);
                }
            }
            ui::EndSelect();
        }
        ImGui::PopID();
    }

    {
        ui::UiButtonDesc pickMat;
        pickMat.label = "Pick Material…";
        pickMat.icon = ui::UiIcon(ELucideIcon::Search);
        pickMat.variant = ui::EUiVariant::Secondary;
        pickMat.size = ui::EUiSize::Sm;
        if (ui::Button("##pick_mat", pickMat)) {
            ctx.BeginPickMaterial();
        }
    }
    ImGui::SameLine();
    const std::string editPath = !mesh.materialPaths.empty() && !mesh.materialPaths[0].empty()
                                     ? mesh.materialPaths[0]
                                     : mesh.materialPath;
    {
        ui::UiButtonDesc editMat;
        editMat.label = "Edit Material…";
        editMat.icon = ui::UiIcon(ELucideIcon::Layers);
        editMat.variant = ui::EUiVariant::Secondary;
        editMat.size = ui::EUiSize::Sm;
        editMat.disabled = editPath.empty();
        if (ui::Button("##edit_mat", editMat) && !editPath.empty()) {
            ctx.requestOpenMaterialPath = !ctx.projectPath.empty()
                                              ? ResolveContentAssetPath(ctx.projectPath, editPath)
                                              : ResolveAssetPath(editPath);
            ctx.showMaterialEditor = true;
        }
    }
    if (!ctx.pendingMaterialPickPath.empty()) {
        const std::string loadAbs = ctx.pendingMaterialPickPath;
        const std::string authoring = MakePackRelativeAssetPath(ctx, loadAbs);
        // Apply pick to Element 0 (or sole slot).
        applySlotMaterial(0, authoring, loadAbs);
        ctx.pendingMaterialPickPath.clear();
        ctx.requestPickMaterial = false;
        materialsProjectKey_.clear();
    }

    if (mesh.materialOverride && !mesh.materialPath.empty() && mesh.materialPaths.empty()) {
        ImGui::TextDisabled("Legacy single Material override (clears when you set an Element)");
        if (ui::Button("##clear_mat_ov", "Clear override (use mesh materials)",
                       ui::EUiVariant::Ghost, ui::EUiSize::Sm)) {
            if (ctx.history != nullptr) {
                ctx.history->Capture(ctx, "Clear Material Override");
            }
            mesh.materialOverride = false;
            mesh.materialPath.clear();
            mesh.materials.clear();
            ctx.MarkDirty();
        }
    }

    if (ctx.renderer != nullptr && ctx.resources != nullptr) {
        ImGui::Spacing();
        ui::SectionLabel("Preview");
        const std::string previewKey = !editPath.empty() ? editPath : "Materials/M_Default.lmat";
        const std::string previewPath = !ctx.projectPath.empty()
                                            ? ResolveContentAssetPath(ctx.projectPath, previewKey)
                                            : ResolveAssetPath(previewKey);
        materialPreview_.SetMaterialPath(*ctx.resources, previewPath);
        materialPreview_.Draw(*ctx.renderer, 160.0f, 160.0f);
    }
}

void DetailsPanel::DrawMeshPicker(EditorContext& ctx, StaticMeshComponent& mesh) {
    if (ctx.resources == nullptr) {
        return;
    }

    {
        ui::UiButtonDesc pickMesh;
        pickMesh.label = "Pick Mesh…";
        pickMesh.icon = ui::UiIcon(ELucideIcon::Box);
        pickMesh.variant = ui::EUiVariant::Secondary;
        pickMesh.size = ui::EUiSize::Sm;
        if (ui::Button("##pick_mesh", pickMesh)) {
            ctx.BeginPickMesh();
        }
    }
    if (ctx.requestPickMesh) {
        ImGui::SameLine();
        ui::Hint("Select a Static Mesh in Content Browser");
    }

    if (ctx.pendingMeshPickPath.empty()) {
        return;
    }

    const std::string pickedAbs = std::move(ctx.pendingMeshPickPath);
    ctx.pendingMeshPickPath.clear();
    ctx.requestPickMesh = false;

    const std::string cookedAbs = ResolveCookedStaticMeshPath(pickedAbs);
    if (cookedAbs.empty()) {
        return;
    }
    const std::string rel = MakePackRelativeAssetPath(ctx, cookedAbs);
    auto loaded = ctx.resources->LoadStaticMesh(ResolveAssetPath(rel));
    if (loaded == nullptr || !loaded->Valid()) {
        loaded = ctx.resources->LoadStaticMesh(cookedAbs);
    }
    if (loaded == nullptr || !loaded->Valid()) {
        return;
    }

    if (ctx.history != nullptr) {
        (void)ctx.history->Capture(ctx, "Set Static Mesh");
    }
    mesh.mesh = std::move(loaded);
    mesh.meshPath = rel.empty() ? cookedAbs : rel;
    if (mesh.editorClass.empty() || mesh.editorClass == "Cube" || mesh.editorClass == "Sphere" ||
        mesh.editorClass == "Plane") {
        mesh.editorClass = "StaticMesh";
    }
    ctx.MarkDirty();
}

void DetailsPanel::DrawMultiSelection(EditorContext& ctx) {
    ctx.ResolveSelectionIndices();
    const std::size_t count = ctx.selected.size();
    ImGui::Text("%zu Actors selected", count);

    EEditorSelectionKind kind = ctx.selected.front().kind;
    bool homogeneous = true;
    for (const EditorSelection& s : ctx.selected) {
        if (s.kind != kind) {
            homogeneous = false;
            break;
        }
    }
    if (!homogeneous) {
        ImGui::TextDisabled("Select actors of the same type to multi-edit properties.");
        return;
    }

    auto forEachStaticMesh = [&](auto&& fn) {
        for (const EditorSelection& s : ctx.selected) {
            if (s.index < ctx.level->StaticMeshes().size()) {
                fn(ctx.level->StaticMeshes()[s.index]);
            }
        }
    };
    auto forEachTransform = [&](auto&& fn) -> bool {
        bool any = false;
        for (const EditorSelection& s : ctx.selected) {
            Transform* t = nullptr;
            switch (s.kind) {
            case EEditorSelectionKind::StaticMesh:
                if (s.index < ctx.level->StaticMeshes().size()) {
                    t = &ctx.level->StaticMeshes()[s.index].transform;
                }
                break;
            case EEditorSelectionKind::PointLight:
                if (s.index < ctx.level->PointLights().size()) {
                    t = &ctx.level->PointLights()[s.index].transform;
                }
                break;
            case EEditorSelectionKind::SpotLight:
                if (s.index < ctx.level->SpotLights().size()) {
                    t = &ctx.level->SpotLights()[s.index].transform;
                }
                break;
            case EEditorSelectionKind::DirectionalLight:
                if (s.index < ctx.level->DirectionalLights().size()) {
                    t = &ctx.level->DirectionalLights()[s.index].transform;
                }
                break;
            case EEditorSelectionKind::PlayerStart:
                if (s.index < ctx.level->PlayerStarts().size()) {
                    t = &ctx.level->PlayerStarts()[s.index].transform;
                }
                break;
            case EEditorSelectionKind::TriggerVolume:
                if (s.index < ctx.level->TriggerVolumes().size()) {
                    t = &ctx.level->TriggerVolumes()[s.index].transform;
                }
                break;
            case EEditorSelectionKind::PainCausingVolume:
                if (s.index < ctx.level->PainCausingVolumes().size()) {
                    t = &ctx.level->PainCausingVolumes()[s.index].transform;
                }
                break;
            case EEditorSelectionKind::AISpawnPoint:
                if (s.index < ctx.level->AISpawnPoints().size()) {
                    t = &ctx.level->AISpawnPoints()[s.index].transform;
                }
                break;
            case EEditorSelectionKind::TextRenderActor:
                if (s.index < ctx.level->TextRenderActors().size()) {
                    t = &ctx.level->TextRenderActors()[s.index].transform;
                }
                break;
            case EEditorSelectionKind::BlueprintInstance:
                if (s.index < ctx.level->BlueprintInstances().size()) {
                    t = &ctx.level->BlueprintInstances()[s.index].transform;
                }
                break;
            default:
                break;
            }
            if (t != nullptr) {
                fn(*t, s);
                any = true;
            }
        }
        return any;
    };

    Transform editXf{};
    bool hasXf = false;
    forEachTransform([&](Transform& t, const EditorSelection&) {
        if (!hasXf) {
            editXf = t;
            hasXf = true;
        }
    });
    if (hasXf && ui::CollapsingSection("Transform")) {
        const Transform before = editXf;
        if (EditTransformFields(editXf, ctx)) {
            // Apply TRS deltas so each actor keeps its relative offset (same as multi-gizmo).
            const glm::vec3 dPos = editXf.position - before.position;
            const glm::vec3 dRot = editXf.rotationDegrees - before.rotationDegrees;
            const glm::vec3 scaleRatio{
                (std::abs(before.scale.x) > 1e-6f) ? (editXf.scale.x / before.scale.x) : 1.0f,
                (std::abs(before.scale.y) > 1e-6f) ? (editXf.scale.y / before.scale.y) : 1.0f,
                (std::abs(before.scale.z) > 1e-6f) ? (editXf.scale.z / before.scale.z) : 1.0f,
            };
            forEachTransform([&](Transform& t, const EditorSelection& s) {
                if (EditorCommands::IsEditorLocked(ctx, s.kind, s.index)) {
                    return;
                }
                t.position += dPos;
                t.rotationDegrees += dRot;
                t.scale.x *= scaleRatio.x;
                t.scale.y *= scaleRatio.y;
                t.scale.z *= scaleRatio.z;
                if (s.kind == EEditorSelectionKind::BlueprintInstance) {
                    BlueprintInstance& bp = ctx.level->BlueprintInstances()[s.index];
                    if (!SyncBlueprintInstanceTransforms(*ctx.level, bp, ctx.levelPath) &&
                        ctx.resources != nullptr) {
                        (void)ExpandBlueprintInstance(*ctx.resources, *ctx.level, bp,
                                                      ctx.levelPath);
                    }
                }
            });
            ctx.MarkDirty();
            bool lightingTouched = false;
            for (const EditorSelection& s : ctx.selected) {
                if (s.kind == EEditorSelectionKind::StaticMesh &&
                    s.index < ctx.level->StaticMeshes().size()) {
                    const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[s.index];
                    if (mesh.mobility == EComponentMobility::Static &&
                        (!mesh.lightmapId.empty() || mesh.UsesLightmap())) {
                        lightingTouched = true;
                        break;
                    }
                }
                if (s.kind == EEditorSelectionKind::DirectionalLight ||
                    s.kind == EEditorSelectionKind::PointLight ||
                    s.kind == EEditorSelectionKind::SpotLight) {
                    lightingTouched = true;
                    break;
                }
            }
            if (lightingTouched) {
                ctx.MarkLightingOutOfDate();
            }
        }
    }

    if (kind == EEditorSelectionKind::StaticMesh) {
        if (ui::CollapsingSection("Static Mesh")) {
            bool hidden = false;
            bool first = true;
            forEachStaticMesh([&](StaticMeshComponent& mesh) {
                if (first) {
                    hidden = mesh.hidden;
                    first = false;
                }
            });
            if (ui::Checkbox("##hidden_multi", "Hidden", &hidden, ui::EUiSize::Sm)) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                forEachStaticMesh([&](StaticMeshComponent& mesh) { mesh.hidden = hidden; });
                ctx.MarkDirty();
            }

            int mobility = 0;
            first = true;
            forEachStaticMesh([&](StaticMeshComponent& mesh) {
                if (first) {
                    mobility = static_cast<int>(mesh.mobility);
                    first = false;
                }
            });
            ui::SectionLabel("Type");
            ui::UiSelectDesc mobilityDesc;
            mobilityDesc.preview = mobility == 0 ? "Static" : "Movable";
            mobilityDesc.width = -1.0f;
            mobilityDesc.size = ui::EUiSize::Sm;
            if (ui::BeginSelect("##mobility_multi", mobilityDesc)) {
                int next = mobility;
                if (ui::SelectItem("Static", mobility == 0)) {
                    next = 0;
                }
                if (ui::SelectItem("Movable", mobility == 1)) {
                    next = 1;
                }
                if (next != mobility) {
                    mobility = next;
                    if (ctx.history != nullptr) {
                        ctx.history->Capture(ctx);
                    }
                    forEachStaticMesh([&](StaticMeshComponent& mesh) {
                        mesh.mobility = static_cast<EComponentMobility>(mobility);
                        if (mesh.mobility == EComponentMobility::Movable) {
                            mesh.lightmap.reset();
                            mesh.lightmapId.clear();
                            mesh.lightmapPath.clear();
                        }
                    });
                    ctx.MarkDirty();
                }
                ui::EndSelect();
            }

            // Apply material from primary selection picker onto all selected meshes.
            if (ctx.selection.index < ctx.level->StaticMeshes().size()) {
                StaticMeshComponent& primary = ctx.level->StaticMeshes()[ctx.selection.index];
                const std::string beforePath = primary.materialPath;
                const auto beforeSlots = primary.materialPaths;
                DrawMaterialPicker(ctx, primary);
                if (primary.materialPath != beforePath || primary.materialPaths != beforeSlots) {
                    forEachStaticMesh([&](StaticMeshComponent& mesh) {
                        if (&mesh == &primary) {
                            return;
                        }
                        mesh.material = primary.material;
                        mesh.materials = primary.materials;
                        mesh.materialOverride = primary.materialOverride;
                        mesh.materialPath = primary.materialPath;
                        mesh.materialPaths = primary.materialPaths;
                    });
                }
            }
        }
    }
}

void DetailsPanel::Draw(EditorContext& ctx) {
    if (!ctx.showDetails) {
        return;
    }
    if (!ui::BeginPanel("Details", {.pOpen = &ctx.showDetails})) {
        ui::EndPanel();
        return;
    }

    if (ctx.level == nullptr || !ctx.selection.IsValid()) {
        ui::Hint("Nothing selected");
        ui::EndPanel();
        return;
    }

    if (ctx.selected.size() > 1) {
        DrawMultiSelection(ctx);
        ui::EndPanel();
        return;
    }

    ui::PushFormLayout();

    switch (ctx.selection.kind) {
    case EEditorSelectionKind::StaticMesh: {
        if (ctx.selection.index >= ctx.level->StaticMeshes().size()) {
            break;
        }
        StaticMeshComponent& mesh = ctx.level->StaticMeshes()[ctx.selection.index];
        ImGui::Text("Static mesh — %s",
                    mesh.editorClass.empty() ? "Mesh"
                                             : HumanizeClassName(mesh.editorClass.c_str()));

        if (ui::CollapsingSection("Transform")) {
            if (EditTransformFields(mesh.transform, ctx)) {
                if (mesh.mobility == EComponentMobility::Static &&
                    (!mesh.lightmapId.empty() || mesh.UsesLightmap())) {
                    ctx.MarkLightingOutOfDate();
                }
                ctx.MarkDirty();
            }
        }
        if (ui::CollapsingSection("Static Mesh")) {
            if (!mesh.meshPath.empty()) {
                ImGui::TextDisabled("Mesh: %s", mesh.meshPath.c_str());
                ui::UiButtonDesc browseMesh;
                browseMesh.label = "Browse to Asset";
                browseMesh.icon = ui::UiIcon(ELucideIcon::FolderOpen);
                browseMesh.variant = ui::EUiVariant::Ghost;
                browseMesh.size = ui::EUiSize::Sm;
                if (ui::Button("##browse_mesh", browseMesh)) {
                    std::string abs = ResolveLevelAssetPath(ctx.levelPath, mesh.meshPath);
                    if (abs.empty()) {
                        abs = ResolveAssetPath(mesh.meshPath);
                    }
                    ctx.RevealInContentBrowser(abs.empty() ? mesh.meshPath : abs);
                }
            } else {
                ImGui::TextDisabled("Mesh: (procedural / unbound)");
            }
            DrawMeshPicker(ctx, mesh);
            DrawMaterialPicker(ctx, mesh);
            if (!mesh.materialPath.empty()) {
                ui::UiButtonDesc browseMat;
                browseMat.label = "Browse to Asset";
                browseMat.icon = ui::UiIcon(ELucideIcon::FolderOpen);
                browseMat.variant = ui::EUiVariant::Ghost;
                browseMat.size = ui::EUiSize::Sm;
                if (ui::Button("##browse_mat", browseMat)) {
                    std::string abs = ResolveLevelAssetPath(ctx.levelPath, mesh.materialPath);
                    if (abs.empty()) {
                        abs = ResolveAssetPath(mesh.materialPath);
                    }
                    ctx.RevealInContentBrowser(abs.empty() ? mesh.materialPath : abs);
                }
            }
        }
        {
            const EComponentMobility mobilityBefore = mesh.mobility;
            ReflectDrawCallbacks cbs;
            // Lightmass UI is drawn below (avoid duplicate CollapsingHeader ID).
            cbs.skipProperties = {"materialPath", "materialPaths", "lightmapResolution"};
            cbs.onChanged = [&]() {
                if (mesh.simulatePhysics) {
                    mesh.collisionEnabled = true;
                }
                if (mesh.mobility == EComponentMobility::Movable &&
                    mobilityBefore == EComponentMobility::Static) {
                    mesh.lightmap.reset();
                    mesh.lightmapId.clear();
                    mesh.lightmapPath.clear();
                }
                ctx.MarkDirty();
            };
            (void)DrawReflectedObject(mesh, ctx, std::move(cbs));
        }
        if (ui::CollapsingSection("Level of detail")) {
            ImGui::TextDisabled("Distance thresholds (world units) for LOD 1/2/3 meshes");
            bool lodChanged = false;
            for (int li = 0; li < 3; ++li) {
                char label[32];
                (void)std::snprintf(label, sizeof(label), "LOD %d distance", li + 1);
                lodChanged |=
                    ImGui::DragFloat(label, &mesh.lodDistances[static_cast<std::size_t>(li)], 1.0f,
                                     0.0f, 500.0f, "%.1f");
            }
            lodChanged |= ImGui::DragInt("Minimum LOD", &mesh.minLod, 0.1f, 0, 3);
            if (lodChanged) {
                ctx.MarkDirty();
            }
        }
        if (ui::CollapsingSection("Baked lighting")) {
            ImGui::TextDisabled(mesh.mobility == EComponentMobility::Static
                                    ? "Receives build lighting / baked map data"
                                    : "Realtime lighting only");
            int resIndex = 0;
            const int current = leon::ClampLightmapResolution(mesh.lightmapResolution);
            for (int i = 0; i < leon::kLightmapResolutionCount; ++i) {
                if (leon::kLightmapResolutions[i] == current) {
                    resIndex = i;
                    break;
                }
            }
            // Labels must stay power-of-two — no free DragInt (avoids 302, etc.).
            const char* resLabels[] = {"64", "128", "256", "512", "1024"};
            static_assert(sizeof(resLabels) / sizeof(resLabels[0]) ==
                              sizeof(leon::kLightmapResolutions) / sizeof(leon::kLightmapResolutions[0]),
                          "lightmap resolution labels must match kLightmapResolutions");
            ui::SectionLabel("Resolution");
            ui::UiSelectDesc resDesc;
            resDesc.preview = resLabels[resIndex];
            resDesc.size = ui::EUiSize::Sm;
            resDesc.width = -1.0f;
            if (ui::BeginSelect("##lightmap_res", resDesc)) {
                for (int i = 0; i < leon::kLightmapResolutionCount; ++i) {
                    if (ui::SelectItem(resLabels[i], i == resIndex)) {
                        mesh.lightmapResolution = leon::kLightmapResolutions[i];
                        ctx.MarkDirty();
                        ctx.MarkLightingOutOfDate();
                    }
                }
                ui::EndSelect();
            }
            if (mesh.UsesLightmap()) {
                ImGui::TextDisabled("Lightmap baked (%dx%d)", mesh.lightmapResolution,
                                    mesh.lightmapResolution);
            } else {
                ImGui::TextDisabled("No lightmap — use Toolbar Build Lighting");
            }
        }
        break;
    }
    case EEditorSelectionKind::DirectionalLight: {
        if (ctx.selection.index >= ctx.level->DirectionalLights().size()) {
            break;
        }
        DirectionalLight& light = ctx.level->DirectionalLights()[ctx.selection.index];
        ImGui::TextUnformatted("Directional Light");
        if (ui::CollapsingSection("Transform")) {
            if (EditTransformFields(light.transform, ctx)) {
                ctx.MarkLightingOutOfDate();
                ctx.MarkDirty();
            }
        }
        ReflectDrawCallbacks lightCbs;
        lightCbs.onChanged = [&]() {
            ctx.MarkLightingOutOfDate();
            ctx.MarkDirty();
        };
        (void)DrawReflectedObject(light, ctx, std::move(lightCbs));
        ImGui::TextDisabled("Cast Shadows → ShadowDepth map (+ Shadows / + Post preset).");
        break;
    }
    case EEditorSelectionKind::PointLight: {
        if (ctx.selection.index >= ctx.level->PointLights().size()) {
            break;
        }
        PointLight& light = ctx.level->PointLights()[ctx.selection.index];
        ImGui::TextUnformatted("Point Light");
        if (ui::CollapsingSection("Transform")) {
            if (EditTransformFields(light.transform, ctx)) {
                ctx.MarkLightingOutOfDate();
                ctx.MarkDirty();
            }
        }
        ReflectDrawCallbacks cbs;
        cbs.onChanged = [&]() {
            ctx.MarkLightingOutOfDate();
            ctx.MarkDirty();
        };
        (void)DrawReflectedObject(light, ctx, std::move(cbs));
        if (ui::CollapsingSection("Lightmass")) {
            if (ImGui::Checkbox("Affect Light Bake", &light.castShadows)) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                ctx.MarkLightingOutOfDate();
                ctx.MarkDirty();
            }
            ImGui::TextDisabled("Build Lighting occlusion only — no runtime shadow map.");
        }
        break;
    }
    case EEditorSelectionKind::SpotLight: {
        if (ctx.selection.index >= ctx.level->SpotLights().size()) {
            break;
        }
        SpotLight& light = ctx.level->SpotLights()[ctx.selection.index];
        ImGui::TextUnformatted("Spot Light");
        if (ui::CollapsingSection("Transform")) {
            if (EditTransformFields(light.transform, ctx)) {
                ctx.MarkLightingOutOfDate();
                ctx.MarkDirty();
            }
        }
        ReflectDrawCallbacks spotCbs;
        spotCbs.onChanged = [&]() {
            ctx.MarkLightingOutOfDate();
            ctx.MarkDirty();
        };
        (void)DrawReflectedObject(light, ctx, std::move(spotCbs));
        if (ui::CollapsingSection("Lightmass")) {
            if (ImGui::Checkbox("Affect Light Bake", &light.castShadows)) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                ctx.MarkLightingOutOfDate();
                ctx.MarkDirty();
            }
            ImGui::TextDisabled("Build Lighting occlusion only — no runtime shadow map.");
        }
        break;
    }
    case EEditorSelectionKind::PlayerStart: {
        if (ctx.selection.index >= ctx.level->PlayerStarts().size()) {
            break;
        }
        PlayerStart& start = ctx.level->PlayerStarts()[ctx.selection.index];
        ImGui::TextUnformatted("Player Start");
        if (ui::CollapsingSection("Transform")) {
            if (EditTransformFields(start.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        break;
    }
    case EEditorSelectionKind::TriggerVolume: {
        if (ctx.selection.index >= ctx.level->TriggerVolumes().size()) {
            break;
        }
        TriggerVolume& volume = ctx.level->TriggerVolumes()[ctx.selection.index];
        ImGui::TextUnformatted("Trigger Volume");
        if (ui::CollapsingSection("Transform")) {
            if (EditTransformFields(volume.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        (void)DrawReflectedObject(volume, ctx);
        break;
    }
    case EEditorSelectionKind::PainCausingVolume: {
        if (ctx.selection.index >= ctx.level->PainCausingVolumes().size()) {
            break;
        }
        PainCausingVolume& volume = ctx.level->PainCausingVolumes()[ctx.selection.index];
        ImGui::TextUnformatted("Pain Causing Volume");
        if (ui::CollapsingSection("Transform")) {
            if (EditTransformFields(volume.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        (void)DrawReflectedObject(volume, ctx);
        break;
    }
    case EEditorSelectionKind::AISpawnPoint: {
        if (ctx.selection.index >= ctx.level->AISpawnPoints().size()) {
            break;
        }
        AISpawnPoint& point = ctx.level->AISpawnPoints()[ctx.selection.index];
        ImGui::TextUnformatted("AI Spawn Point");
        if (ui::CollapsingSection("Transform")) {
            if (EditTransformFields(point.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        (void)DrawReflectedObject(point, ctx);
        break;
    }
    case EEditorSelectionKind::TextRenderActor: {
        if (ctx.selection.index >= ctx.level->TextRenderActors().size()) {
            break;
        }
        TextRenderActor& tr = ctx.level->TextRenderActors()[ctx.selection.index];
        ImGui::TextUnformatted("Text render");
        if (ui::CollapsingSection("Transform")) {
            if (EditTransformFields(tr.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        (void)DrawReflectedObject(tr, ctx);
        break;
    }
    case EEditorSelectionKind::BlueprintInstance: {
        if (ctx.selection.index >= ctx.level->BlueprintInstances().size()) {
            break;
        }
        BlueprintInstance& bp = ctx.level->BlueprintInstances()[ctx.selection.index];
        ImGui::TextUnformatted("Blueprint");
        if (ui::CollapsingSection("Transform")) {
            bool transformEditEnded = false;
            if (EditTransformFields(bp.transform, ctx, &transformEditEnded)) {
                if (!SyncBlueprintInstanceTransforms(*ctx.level, bp, ctx.levelPath) &&
                    ctx.resources != nullptr) {
                    (void)ExpandBlueprintInstance(*ctx.resources, *ctx.level, bp, ctx.levelPath);
                }
                ctx.MarkDirty();
            }
            if (transformEditEnded &&
                !SyncBlueprintInstanceTransforms(*ctx.level, bp, ctx.levelPath) &&
                ctx.resources != nullptr) {
                (void)ExpandBlueprintInstance(*ctx.resources, *ctx.level, bp, ctx.levelPath);
            }
        }
        if (ui::CollapsingSection("Blueprint")) {
            char labelBuf[128];
            (void)std::snprintf(labelBuf, sizeof(labelBuf), "%s", bp.actorLabel.c_str());
            if (ui::InputText("##actor_label", "Actor Label", labelBuf, sizeof(labelBuf))) {
                bp.actorLabel = labelBuf;
                ctx.MarkDirty();
            }
            ImGui::TextWrapped("Blueprint: %s", bp.blueprintPath.c_str());
            ui::UiButtonDesc browseBp;
            browseBp.label = "Browse to Asset";
            browseBp.icon = ui::UiIcon(ELucideIcon::FolderOpen);
            browseBp.variant = ui::EUiVariant::Ghost;
            browseBp.size = ui::EUiSize::Sm;
            if (ui::Button("##browse_bp", browseBp)) {
                std::string abs = ResolveLevelAssetPath(ctx.levelPath, bp.blueprintPath);
                if (abs.empty()) {
                    abs = ResolveAssetPath(bp.blueprintPath);
                }
                ctx.RevealInContentBrowser(abs.empty() ? bp.blueprintPath : abs);
            }
        }
        break;
    }
    case EEditorSelectionKind::Actor: {
        if (ctx.world == nullptr) {
            ImGui::TextDisabled("No World");
            break;
        }
        Actor* target = (ctx.selection.id != 0) ? ctx.world->FindActorByEditorId(ctx.selection.id)
                                                : ctx.FindActorByIndex(ctx.selection.index);
        if (target == nullptr) {
            ImGui::TextDisabled("Actor gone");
            break;
        }
        ImGui::Text("Actor — %s", DemangleType(typeid(*target).name()));
        ImGui::TextDisabled("Editor ID: %llu",
                            static_cast<unsigned long long>(target->GetEditorId()));

        if (ui::CollapsingSection("Transform")) {
            glm::vec3 loc = target->GetActorLocation();
            float yaw = target->GetActorYaw();
            bool changed = false;
            changed |= ui::DragFloat3("##actor_loc", "Location", &loc.x, 0.05f);
            changed |= ui::DragFloat("##actor_yaw", "Rotation (Yaw)", &yaw, 0.5f);
            if (changed) {
                target->SetActorLocationAndRotation(loc, yaw);
            }
        }
        if (ui::CollapsingSection("Actor")) {
            ImGui::TextDisabled("Root SceneComponent children: %zu",
                                target->GetRootComponent().GetAttachChildren().size());
            ImGui::TextDisabled("Level mesh index: %zu", target->LevelMeshIndex());
        }
        if (auto* character = dynamic_cast<Character*>(target)) {
            if (ui::CollapsingSection("Skeletal Mesh")) {
                SkeletalMeshComponent& mesh = character->GetMesh();
                ImGui::TextDisabled(
                    "Bones: %d",
                    mesh.HasValidMesh() ? mesh.GetSkeletalMesh()->GetSkeleton().BoneCount() : 0);
                char physBuf[512]{};
                std::snprintf(physBuf, sizeof(physBuf), "%s", mesh.GetPhysicsAssetPath().c_str());
                ui::FieldLabel("Physics Asset");
                if (ui::InputText("##phys_asset", nullptr, physBuf, sizeof(physBuf))) {
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    if (physBuf[0] == '\0') {
                        mesh.SetPhysicsAsset(nullptr);
                    } else if (!mesh.SetPhysicsAssetPath(physBuf)) {
                        ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.3f, 1.0f),
                                           "Failed to load .lphys");
                    }
                }
                if (const PhysicsAsset* phys = mesh.GetPhysicsAsset()) {
                    ImGui::TextDisabled("Bodies: %zu", phys->bodies.size());
                } else {
                    ImGui::TextDisabled("No PhysicsAsset assigned (.lphys)");
                }
            }
        }
        break;
    }
    default:
        break;
    }

    ui::PopFormLayout();
    ui::EndPanel();
}

} // namespace leon::editor
